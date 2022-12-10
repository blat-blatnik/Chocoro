#include "engine.h"

//TODO
// [ ] try no freeze frames
// [-] squash (it doesn't look nice)

// Constants

#define TILE_SIZE 8
#define MAX_TILE_VARIANTS 50
#define NUM_TILES (SCREEN_SIZE / TILE_SIZE)
#define MAX_SPEED 20.0f
#define GRAVITY 0.1f
#define CHOCORO_SIZE 5.0f
#define CHOCORO_RADIUS (CHOCORO_SIZE / 2)
#define DASH_SPEED 6.0f
#define ZAP_FRAMES 60
#define ZAP_SPEED 10.0f
#define AIR_FRICTION 0.02f
#define GAS_FRICTION 0.10f
#define BALLOON_RADIUS 8
#define BALLOON_BOOST 4.0f
#define GASER_WIDTH 6
#define GASER_HEIGHT 16
#define GASER_VISION_RADIUS 500.0f
#define GASER_BOTTLE_COOLDOWN_FRAMES 120
#define GASER_BOTTLE_SPEED 5.0f
#define GASER_FADE_FRAMES 10
#define BOTTLE_RADIUS 4.0f
#define BOTTLE_EXPLOSION_RADIUS 40.0f
#define GAS_LINGER_FRAMES (5 * FPS)
#define GAS_FADE_IN_FRAMES 15
#define GAS_FADE_OUT_FRAMES 30
#define GUNNER_WIDTH 6
#define GUNNER_HEIGHT 16
#define GUNNER_AIM_FRAMES 100
#define GUNNER_CHARGE_FRAMES 30
#define GUNNER_FIRE_FRAMES 80
#define GUNNER_CYCLE_FRAMES (GUNNER_AIM_FRAMES + GUNNER_CHARGE_FRAMES + GUNNER_FIRE_FRAMES)
#define GUNNER_FIRE_START (GUNNER_AIM_FRAMES + GUNNER_CHARGE_FRAMES)
#define GUNNER_GUN_DISTANCE 9.0f
#define GUNNER_FADE_FRAMES 5
#define BEAM_RADIUS 15.0f
#define BEAM_WIDTH (2 * BEAM_RADIUS)
#define RESTART_HOLD_FRAMES (2 * FPS)
#define RESTART_DURATION_FRAMES 40
#define LEVEL_TRANSITION_DURATION_FRAMES 30

// Enumerations

typedef enum Tile {
	TILE_FLOOR,
	TILE_ENTRANCE,
	TILE_EXIT,
	TILE_WALL,
	TILE_CARPET,
	TILE_TRAMPOLINE,
	TILE_SLIME,
	TILE_ENUM_COUNT
} Tile;

typedef enum GridAlign {
	GRID_ALIGN_FREEFORM,
	GRID_ALIGN_FULL,
	GRID_ALIGN_HALF
} GridAlign;

typedef enum GameState {
	GAME_STATE_Splash,
	GAME_STATE_Playing,
	GAME_STATE_Paused,
	GAME_STATE_GameOver,
	GAME_STATE_LevelTransition,
	GAME_STATE_Editor,
	GAME_STATE_Restart,
	GAME_STATE_Credits,
	GAME_STATE_ENUM_COUNT
} GameState;

// Datatypes

typedef struct Chocoro {
	Vector2 position;
	Vector2 velocity;
	Ring(Vector2, FPS) trail;
	bool dashReady;
	bool zapReady;
	int lastDashFrame;
	int freezeFrames;
	int zapFrames;
	bool hitTileLastFrame;
} Chocoro;

typedef struct Balloon {
	Vector2 position;
	Vector2 offset;
	int framesPopped;
} Balloon;

typedef struct Bottle {
	Vector2 position;
	Vector2 velocity;
	int frame;
} Bottle;

typedef struct Gaser {
	Vector2 position;
	int bottleCooldownFrames;
	int framesSeenChocoro;
	int framesKilled;
} Gaser;

typedef struct Gunner {
	Vector2 position;
	float aimAngle;
	int framesSeenChocoro;
	int framesKilled;
} Gunner;

typedef struct DashLine {
	Vector2 position;
	Vector2 velocity;
	int frameStart;
	int frameEnd;
} DashLine;

typedef struct Cloud {
	Vector2 position;
	Vector2 velocity;
	float radius;
	Color color;
	int frame;
	int lifetime;
} Cloud;

typedef struct Gas {
	Vector2 position;
	float radius;
	int frame;
} Gas;

typedef struct ChocoCamera {
	Camera2D camera;
	float trauma;
	float shakyTime;
} ChocoCamera;

// Globals

bool TileIsPassable[TILE_ENUM_COUNT] = {
	[TILE_FLOOR] = true,
	[TILE_ENTRANCE] = true,
	[TILE_EXIT] = true,
};
const float TileFriction[TILE_ENUM_COUNT] = {
	[TILE_WALL] = 0.4f,
	[TILE_ENTRANCE] = 0.4f,
	[TILE_EXIT] = 0.4f,
	[TILE_CARPET] = 1.0f,
	[TILE_TRAMPOLINE] = -0.5f,
	[TILE_SLIME] = 0.9f,
};
const float TileStickyness[TILE_ENUM_COUNT] = {
	[TILE_WALL] = 0,
	[TILE_ENTRANCE] = 0,
	[TILE_EXIT] = 0,
	[TILE_CARPET] = 0.5f,
	[TILE_TRAMPOLINE] = 0,
	[TILE_SLIME] = 1,
};
const float TileDebrisSize[TILE_ENUM_COUNT] = {
	[TILE_WALL] = 0,
	[TILE_ENTRANCE] = 0,
	[TILE_EXIT] = 0,
	[TILE_CARPET] = 2.0f,
	[TILE_SLIME] = 2.5f,
	[TILE_TRAMPOLINE] = 1.0f,
};
const Color TileColors[TILE_ENUM_COUNT] = {
	[TILE_FLOOR] = { 80, 80, 80, 255 },
	[TILE_ENTRANCE] = { 80, 80, 80, 255 },
	[TILE_EXIT] = { 80, 80, 80, 255 },
	[TILE_WALL] = { 0, 82, 172, 255 },
	[TILE_CARPET] = { 100, 0, 255, 255 },
	[TILE_TRAMPOLINE] = { 255, 0, 229, 255 },
	[TILE_SLIME] = { 0, 170, 20, 255 },
};
const char *TileNames[TILE_ENUM_COUNT] = {
	[TILE_FLOOR] = "floor",
	[TILE_ENTRANCE] = "entrance",
	[TILE_EXIT] = "exit",
	[TILE_WALL] = "wall",
	[TILE_SLIME] = "slime",
	[TILE_CARPET] = "carpet",
	[TILE_TRAMPOLINE] = "trampoline",
};
Texture TileTextures[TILE_ENUM_COUNT][MAX_TILE_VARIANTS];
int TileNumVariants[TILE_ENUM_COUNT];

int framesPlaying;
int bestFramesPlaying;
int numLevels;
int levelIndex;
int editorGridAlign;
int mouseHoldFrames;
bool editorSaveNeeded;
int gameStateFrame;
char *deathLine1 = "";
char *deathLine2 = "";
GameState gameState;
GameState previousGameState = -1;
Vector2 editorDragOffset;
int editorSelectedTile = -1;
float editorTileBrushRadius;
Balloon *editorSelectedBalloon;
Gaser *editorSelectedGaser;
Gunner *editorSelectedGunner;
Chocoro *editorSelectedChocoro;
int tileVariants[NUM_TILES][NUM_TILES];
Tile tiles[NUM_TILES][NUM_TILES];
Chocoro chocoro = { .position = { 25, SCREEN_SIZE } };
List(Balloon, 50) balloons;
List(Gaser, 50) gasers;
List(Gunner, 50) gunners;
Ring(DashLine, 50) dashLines;
List(Cloud, 500) clouds;
Ring(Gas, 200) gases;
List(Bottle, 20) bottles;
ChocoCamera camera = {
	.camera.target = { 0.5f * SCREEN_SIZE, 0.5f * SCREEN_SIZE },
	.camera.offset = { 0.5f * SCREEN_SIZE, 0.5f * SCREEN_SIZE },
	.camera.zoom = 1,
};
Texture chocoroTexture;
Texture chocoroZappedTexture;
Texture chocoroExhaustedTexture;
Texture chocoroDeadTexture;
Texture zapTexture;
Texture balloonTexture;
Texture gaserTexture;
Texture bottleTexture;
Texture gunnerIdleTexture;
Texture gunnerAimingTexture;
Texture gunTexture;
Texture beamTexture;
Texture beamStartTexture;
Texture lightningTexture0;
Texture lightningTexture1;
Texture lightningTexture2;
Texture castleTexture;
Texture mouseClickTexture;
Texture mouseHoldTexture;
Sound pingSound;
Sound popSound;
Sound dashSound;
Sound slimeSounds[2];
Sound trampolineSound;
Sound wallSound;
Music zapSound;
float zapVolume;
Sound laserChargeSound;
Sound laserBeamSound;
Sound bottleSound;
Sound gasSound;
Sound hitSound;
Sound ghostSound;
Sound thunder0Sound;
Sound thunder1Sound;
Music music;

// Tile

void UpdateEntranceAndExitPassable(void)
{
	int x0 = ClampInt((int)floorf((chocoro.position.x - CHOCORO_RADIUS) / TILE_SIZE), 0, NUM_TILES - 1);
	int y0 = ClampInt((int)floorf((chocoro.position.y - CHOCORO_RADIUS) / TILE_SIZE), 0, NUM_TILES - 1);
	int x1 = ClampInt((int)ceilf((chocoro.position.x + CHOCORO_RADIUS) / TILE_SIZE), 0, NUM_TILES - 1);
	int y1 = ClampInt((int)ceilf((chocoro.position.y + CHOCORO_RADIUS) / TILE_SIZE), 0, NUM_TILES - 1);
	bool standingInEntrance = false;
	bool standingInExit = false;
	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			Tile tile = tiles[y][x];
			standingInEntrance |= (tile == TILE_ENTRANCE);
			standingInExit |= (tile == TILE_EXIT);
		}
	}

	if (TileIsPassable[TILE_ENTRANCE] and not standingInEntrance)
		TileIsPassable[TILE_ENTRANCE] = false;

	int numEnemies = balloons.count + gasers.count + gunners.count;
	if (TileIsPassable[TILE_EXIT] and numEnemies > 0 and not standingInExit)
		TileIsPassable[TILE_EXIT] = false;
	if (numEnemies <= 0)
		TileIsPassable[TILE_EXIT] = true;
}

// Helpers

void FillTilesRect(int x0, int y0, int x1, int y1, Tile tile, int variant)
{
	x0 = ClampInt(x0, 0, NUM_TILES - 1);
	y0 = ClampInt(y0, 0, NUM_TILES - 1);
	x1 = ClampInt(x1, 0, NUM_TILES - 1);
	y1 = ClampInt(y1, 0, NUM_TILES - 1);
	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			tiles[y][x] = tile;
			tileVariants[y][x] = variant;
		}
	}
}
void FillTilesLine(int x0, int y0, int x1, int y1, Tile tile, int variant)
{
	// https://stackoverflow.com/a/12934943
	x0 = ClampInt(x0, 0, NUM_TILES - 1);
	y0 = ClampInt(y0, 0, NUM_TILES - 1);
	x1 = ClampInt(x1, 0, NUM_TILES - 1);
	y1 = ClampInt(y1, 0, NUM_TILES - 1);
	int dx = +abs(x1 - x0);
	int dy = -abs(y1 - y0);
	int sx = x0 < x1 ? +1 : -1;
	int sy = y0 < y1 ? +1 : -1;

	int x = x0;
	int y = y0;
	int error = dx + dy;
	for (;;)
	{
		tiles[y][x] = tile;
		tileVariants[y][x] = variant;
		if (x == x1 and y == y1)
			return;

		if (2 * error > dy)
		{
			error += dy;
			x += sx;
		}
		else if (2 * error < dx)
		{
			error += dx;
			y += sy;
		}
	}
}
Sweep SweepCircleTiles(Vector2 position, Vector2 velocity, float radius, int *hitX, int *hitY)
{
	// Intersect against all grid cells that we can possibly collide with.
	// Maybe better to use line drawing algo? https://stackoverflow.com/a/12934943
	float xmin = position.x + fminf(0, velocity.x) - radius;
	float ymin = position.y + fminf(0, velocity.y) - radius;
	float xmax = position.x + fmaxf(0, velocity.x) + radius;
	float ymax = position.y + fmaxf(0, velocity.y) + radius;
	int x0 = ClampInt((int)floorf(xmin / TILE_SIZE), 0, NUM_TILES - 1);
	int y0 = ClampInt((int)floorf(ymin / TILE_SIZE), 0, NUM_TILES - 1);
	int x1 = ClampInt((int)ceilf(xmax / TILE_SIZE), 0, NUM_TILES - 1);
	int y1 = ClampInt((int)ceilf(ymax / TILE_SIZE), 0, NUM_TILES - 1);
	Sweep closest = { INFINITY };
	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			Tile tile = tiles[y][x];
			if (not TileIsPassable[tile])
			{
				Rectangle rect = { (float)(x * TILE_SIZE), (float)(y * TILE_SIZE), TILE_SIZE, TILE_SIZE };
				Sweep sweep = SweepCircleRect(position, velocity, radius, rect);
				if (sweep.t < closest.t)
				{
					*hitX = x;
					*hitY = y;
					closest = sweep;
				}
			}
		}
	}
	return closest;
}
bool PointIsVisibleFrom(Vector2 position, Vector2 target)
{
	// https://stackoverflow.com/a/12934943
	int x0 = ClampInt((int)floorf(position.x / TILE_SIZE), 0, NUM_TILES - 1);
	int y0 = ClampInt((int)floorf(position.y / TILE_SIZE), 0, NUM_TILES - 1);
	int x1 = ClampInt((int)floorf(target.x / TILE_SIZE), 0, NUM_TILES - 1);
	int y1 = ClampInt((int)floorf(target.y / TILE_SIZE), 0, NUM_TILES - 1);
	int dx = +abs(x1 - x0);
	int dy = -abs(y1 - y0);
	int sx = x0 < x1 ? +1 : -1;
	int sy = y0 < y1 ? +1 : -1;

	int x = x0;
	int y = y0;
	int error = dx + dy;
	for (;;)
	{
		Tile tile = tiles[y][x];
		if (not TileIsPassable[tile])
			return false;
		if (x == x1 and y == y1)
			return true;

		if (2 * error > dy)
		{
			error += dy;
			x += sx;
		}
		else if (2 * error < dx)
		{
			error += dx;
			y += sy;
		}
	}
}
void BreakBottle(Bottle *bottle, Vector2 velocity, Vector2 normal)
{
	unused(velocity);

	float speed = Vector2Length(bottle->velocity);
	float normalAngle = Atan2Vector(normal);

	SetSoundPitch(bottleSound, RandomFloat(0.9f, 1.1f));
	PlaySound(bottleSound);
	SetSoundPitch(gasSound, RandomFloat(0.9f, 1.0f));
	PlaySound(gasSound);

	int numClouds = (int)(20 + 2 * speed);
	for (int i = 0; i < numClouds; ++i)
	{
		Cloud *cloud = ListAllocate(&clouds);
		if (not cloud)
			break;

		cloud->position = bottle->position;
		cloud->position.x += RandomFloat(-2, +2);
		cloud->position.y += RandomFloat(-2, +2);
		cloud->radius = 1;
		cloud->frame = RandomInt(-2, 1);
		cloud->lifetime = RandomInt(10, 20);
		cloud->color = GREEN;
		cloud->velocity = Vector2FromPolar(
			RandomFloat(8, 15) * speed,
			normalAngle + RandomNormal(0, PI / 4));
	}

	for (int i = 0; i < 40; ++i)
	{
		Vector2 offset = RandomVectorInCircle(BOTTLE_EXPLOSION_RADIUS);
		float distance = Vector2Length(offset);
		float distanceT = distance / BOTTLE_EXPLOSION_RADIUS;

		Gas *gas = RingAllocate(&gases);
		gas->position = Vector2Add(bottle->position, offset);
		gas->radius = Lerp(15, 4, distanceT) + RandomFloat(-1, +1);
		gas->frame = 0;
	}
}
void EditorDeselect(void)
{
	editorSelectedTile = -1;
	editorDragOffset = (Vector2){ 0 };

	Rectangle screenRect = { 0, 0, SCREEN_SIZE, SCREEN_SIZE };
	if (editorSelectedBalloon)
	{
		if (not CheckCollisionCircleRec(editorSelectedBalloon->position, BALLOON_RADIUS, screenRect))
		{
			int index = (int)(editorSelectedBalloon - &balloons.items[0]);
			ListSwapRemove(&balloons, index);
		}
		editorSelectedBalloon = NULL;
		editorSaveNeeded = true;
	}
	if (editorSelectedGaser)
	{
		Rectangle rect = RectangleFromCenterV(editorSelectedGaser->position, (Vector2) { GASER_WIDTH, GASER_HEIGHT });
		if (not CheckCollisionRecs(rect, screenRect))
		{
			int index = (int)(editorSelectedGaser - &gasers.items[0]);
			ListSwapRemove(&gasers, index);
		}
		editorSelectedGaser = NULL;
		editorSaveNeeded = true;
	}
	if (editorSelectedGunner)
	{
		Rectangle rect = RectangleFromCenterV(editorSelectedGunner->position, (Vector2) { GUNNER_WIDTH, GUNNER_HEIGHT });
		if (not CheckCollisionRecs(rect, screenRect))
		{
			int index = (int)(editorSelectedGunner - &gunners.items[0]);
			ListSwapRemove(&gunners, index);
		}
		editorSelectedGunner = NULL;
		editorSaveNeeded = true;
	}
	if (editorSelectedChocoro)
	{
		chocoro.position.x = Clamp(chocoro.position.x, 0, SCREEN_SIZE - CHOCORO_SIZE);
		chocoro.position.y = Clamp(chocoro.position.y, 0, SCREEN_SIZE - CHOCORO_SIZE);
		editorSelectedChocoro = NULL;
		editorSaveNeeded = true;
	}
}
Vector2 RoundToTileEx(Vector2 position, float multiple)
{
	float x = roundf(position.x / (TILE_SIZE * multiple)) * TILE_SIZE * multiple;
	float y = roundf(position.y / (TILE_SIZE * multiple)) * TILE_SIZE * multiple;
	return (Vector2) { x, y };
}
Vector2 RoundToTile(Vector2 position)
{
	return RoundToTileEx(position, 1);
}
Vector2 GunnerGunPosition(Gunner *gunner)
{
	float angle = FlipAngle(gunner->aimAngle);
	Vector2 toGun = Vector2Rotate((Vector2) { GUNNER_GUN_DISTANCE, 0 }, angle);
	return Vector2Add(gunner->position, toGun);
}
void StopAllSounds(void)
{
	StopSound(thunder0Sound);
	StopSound(thunder1Sound);
	StopSound(laserChargeSound);
	StopSound(laserBeamSound);
	StopSound(bottleSound);
	StopSound(gasSound);
	StopMusicStream(zapSound);
}
void PlayPing(void)
{
	bool anyAlive = false;
	for (int i = 0; i < balloons.count; ++i)
		if (not balloons.items[i].framesPopped)
			anyAlive = true;
	for (int i = 0; i < gunners.count; ++i)
		if (not gunners.items[i].framesKilled)
			anyAlive = true;
	for (int i = 0; i < gasers.count; ++i)
		if (not gasers.items[i].framesKilled)
			anyAlive = true;

	if (anyAlive)
	{
		SetSoundVolume(pingSound, 0.1f);
		SetSoundPitch(pingSound, 0.60f);
		PlaySound(pingSound);
	}
	else
	{
		SetSoundVolume(pingSound, 0.3f);
		SetSoundPitch(pingSound, 0.80f);
		PlaySound(pingSound);
	}
}
char *TimeStringFromFrames(int frames)
{
	int totalSeconds = frames / 60;
	int seconds = totalSeconds % 60;
	int minutes = (totalSeconds / 60) % 60;
	int hours = totalSeconds / 60 / 60;

	if (hours == 0)
		return String("%02d:%02d", minutes, seconds);
	else
		return String("%02d:%02d:%02d", hours, minutes, seconds);
}

// Serialize

void SerializeLevel(SerialOp op)
{
	if (op == LOAD)
	{
		ZeroArray(tiles);
		ZeroArray(tileVariants);
		ZeroStruct(&chocoro);
		ZeroStruct(&balloons);
		ZeroStruct(&gasers);
		ZeroStruct(&gunners);
		ZeroStruct(&gases);
		ZeroStruct(&clouds);
		ZeroStruct(&dashLines);
		ZeroStruct(&bottles);
	}

	char *path = String("level%d.bin", levelIndex);
	FILE *file = BeginSerialize(op, path);

	#define LEVEL_FILE_VERSION 1
	int levelFileVersion = LEVEL_FILE_VERSION;
	Serialize(op, file, &levelFileVersion);
	WarnReturnIf(levelFileVersion != 0 and levelFileVersion != LEVEL_FILE_VERSION);

	for (int y = 0; y < NUM_TILES; ++y)
	{
		uint8_t packedTiles[NUM_TILES][2];
		if (op == SAVE)
		{
			for (int x = 0; x < NUM_TILES; ++x)
			{
				packedTiles[x][0] = (uint8_t)tiles[y][x];
				packedTiles[x][1] = (uint8_t)tileVariants[y][x];
			}
		}
		SerializeBytes(op, file, &packedTiles[0][0], NUM_TILES * sizeof packedTiles[0]);
		if (op == LOAD)
		{
			for (int x = 0; x < NUM_TILES; ++x)
			{
				tiles[y][x] = (Tile)packedTiles[x][0];
				tileVariants[y][x] = (int)packedTiles[x][1];
			}
		}
	}

	Serialize(op, file, &chocoro.position);

	Serialize(op, file, &balloons.count);
	balloons.count = ClampInt(balloons.count, 0, countof(balloons.items) - 1);
	for (int i = 0; i < balloons.count; ++i)
		Serialize(op, file, &balloons.items[i].position);

	Serialize(op, file, &gasers.count);
	gasers.count = ClampInt(gasers.count, 0, countof(gasers.items) - 1);
	for (int i = 0; i < gasers.count; ++i)
		Serialize(op, file, &gasers.items[i].position);

	Serialize(op, file, &gunners.count);
	gunners.count = ClampInt(gunners.count, 0, countof(gunners.items) - 1);
	for (int i = 0; i < gunners.count; ++i)
		Serialize(op, file, &gunners.items[i].position);

	EndSerialize(op, &file);

	if (op == SAVE)
		TraceLog(LOG_INFO, "Saved '%s'.", path);
	else if (op == LOAD)
		TraceLog(LOG_INFO, "Loaded '%s'.", path);
}

// Update

void UpdateChocoCamera(void)
{
	camera.shakyTime = (float)(103.14 * Time.now);
}
void UpdateBalloons(void)
{
	float t = (float)(Time.now * 1.33);
	for (int i = 0; i < balloons.count; ++i)
	{
		Balloon *balloon = &balloons.items[i];
		if (balloon->framesPopped)
		{
			ListSwapRemove(&balloons, i);
			--i;
			continue;
		}
		balloon->offset.x = PerlinNoise1(0xBA11 + i, -1, +1, t);
		balloon->offset.y = PerlinNoise1(0x0008 + i, -3, +3, t);
	}
}
void UpdateGasers(void)
{
	for (int i = 0; i < gasers.count; ++i)
	{
		Gaser *gaser = &gasers.items[i];
		if (gaser->framesKilled)
		{
			if (++gaser->framesKilled > GASER_FADE_FRAMES)
			{
				ListSwapRemove(&gasers, i);
				--i;
			}
			continue;
		}

		float dx = chocoro.position.x - gaser->position.x;
		float dy = chocoro.position.y - gaser->position.y;
		float distance = sqrtf(dx * dx + dy * dy); // Vision is worse from above.

		bool seesChocoro = true;
		if (seesChocoro and distance > GASER_VISION_RADIUS)
			seesChocoro = false;
		if (seesChocoro and not PointIsVisibleFrom(gaser->position, chocoro.position))
			seesChocoro = false;

		if (seesChocoro and not gaser->framesSeenChocoro)
			gaser->bottleCooldownFrames = GASER_BOTTLE_COOLDOWN_FRAMES;

		if (seesChocoro)
			gaser->framesSeenChocoro++;
		else
			gaser->framesSeenChocoro = 0;

		if (seesChocoro)
		{
			if (gaser->bottleCooldownFrames-- < 0)
			{
				// https://en.wikipedia.org/wiki/Projectile_motion#Angle_of_reach (actually it's the section after this one).
				float v = GASER_BOTTLE_SPEED;
				float g = -GRAVITY;
				float v2 = v * v;
				float v4 = v2 * v2;
				float determinant = v4 - g * (g * dx * dx + 2 * dy * v * v);
				if (determinant >= 0)
				{
					Bottle *bottle = ListAllocate(&bottles);
					if (bottle)
					{
						float q = sqrtf(determinant);
						float angle1 = atanf((v2 + q) / (g * dx));
						float angle2 = atanf((v2 - q) / (g * dx));
						float angle = fabsf(angle1) <= fabsf(angle2) ? angle1 : angle2;
						if (chocoro.position.x < gaser->position.x)
							angle = WrapAngle(angle + PI);

						bottle->position = gaser->position;
						bottle->velocity = Vector2FromPolar(GASER_BOTTLE_SPEED, angle);
						bottle->frame = 0;
						gaser->bottleCooldownFrames = GASER_BOTTLE_COOLDOWN_FRAMES;
					}
				}
			}
		}
	}
}
void UpdateGunners(void)
{
	for (int i = 0; i < gunners.count; ++i)
	{
		Gunner *gunner = &gunners.items[i];
		if (gunner->framesKilled)
		{
			if (++gunner->framesKilled > GUNNER_FADE_FRAMES)
			{
				ListSwapRemove(&gunners, i);
				--i;
			}
			continue;
		}

		++gunner->framesSeenChocoro;
		int frame = WrapInt(gunner->framesSeenChocoro, 0, GUNNER_CYCLE_FRAMES);
		bool seesChocoro = PointIsVisibleFrom(gunner->position, chocoro.position);
		if (frame >= GUNNER_AIM_FRAMES)
			seesChocoro = true; // @HACK: Don't stop charging or firing if chocoro goes out of range.

		if (not seesChocoro)
			gunner->framesSeenChocoro = 0;
		if (seesChocoro and frame < GUNNER_AIM_FRAMES)
			gunner->aimAngle = Atan2Vector(Vector2Subtract(gunner->position, chocoro.position));
		
		Vector2 gunPosition = GunnerGunPosition(gunner);
		if (frame >= GUNNER_AIM_FRAMES and frame < GUNNER_FIRE_START)
		{
			for (int j = 0; j < RandomInt(0, 2); ++j)
			{
				DashLine *line = RingAllocate(&dashLines);
				float angle = FlipAngle(gunner->aimAngle) + RandomFloat(-PI / 2, +PI / 2);
				Vector2 offset = Vector2FromPolar(RandomFloat(15, 30), angle);
				line->position = Vector2Add(gunPosition, offset);
				line->velocity = Vector2Negate(offset);
				line->frameStart = Time.frame;
				line->frameEnd = line->frameStart + RandomInt(3, 5);
			}
		}

		int numClouds = 0;
		if (frame == GUNNER_AIM_FRAMES)
		{
			PlaySound(laserChargeSound);
		}
		else if (frame == GUNNER_FIRE_START)
		{
			PlaySound(laserBeamSound);
			camera.trauma += 1.0f;
			numClouds = 20;
		}
		else if (frame >= GUNNER_FIRE_START)
		{
			camera.trauma = fmaxf(0.5f, camera.trauma);
			numClouds = RandomInt(0, 2);
		}

		for (int j = 0; j < numClouds; ++j)
		{
			Cloud *cloud = ListAllocate(&clouds);
			if (not cloud)
				break;

			cloud->position = gunPosition;
			cloud->position.x += RandomFloat(-2, +2);
			cloud->position.y += RandomFloat(-2, +2);
			cloud->velocity = Vector2FromPolar(RandomFloat(10, 20), FlipAngle(gunner->aimAngle) + RandomFloat(-PI / 2, +PI / 2));
			cloud->frame = 0;
			cloud->lifetime = RandomInt(10, 30);
			cloud->color = ScaleBrightness(RED, -RandomFloat(0.1f, 0.5f));
			cloud->radius = RandomFloat(0.5f, 2.5f);
		}
	}
}
void UpdateBottles(void)
{
	for (int i = 0; i < bottles.count; ++i)
	{
		Bottle *bottle = &bottles.items[i];

		bottle->frame++;
		bottle->velocity = Vector2Scale(bottle->velocity, 1 - AIR_FRICTION);
		bottle->velocity.y += GRAVITY;

		Sweep closestSweep = { INFINITY };

		int hitTileX = INT_MIN;
		int hitTileY = INT_MIN;
		Sweep tileSweep = SweepCircleTiles(bottle->position, bottle->velocity, BOTTLE_RADIUS, &hitTileX, &hitTileY);
		if (closestSweep.t > tileSweep.t)
			closestSweep = tileSweep;

		int hitChocoro = INT_MIN;
		Sweep chocoroSweep = SweepCircleCircle(bottle->position, bottle->velocity, BOTTLE_RADIUS, chocoro.position, CHOCORO_RADIUS);
		if (closestSweep.t > chocoroSweep.t)
		{
			closestSweep = chocoroSweep;
			hitTileX = INT_MIN;
			hitTileY = INT_MIN;
			hitChocoro = 1;
		}

		if (hitChocoro != INT_MIN)
		{
			BreakBottle(bottle, chocoro.velocity, Vector2Normalize(chocoro.velocity));
			chocoro.velocity = Vector2Scale(chocoro.velocity, 0.1f);
			chocoro.velocity = Vector2Add(chocoro.velocity, bottle->velocity);
			ListSwapRemove(&bottles, i);
			--i;
			deathLine1 = "Chocoro suffered a bottle induced concussion.";
			deathLine2 = "He will never recover.";
			gameState = GAME_STATE_GameOver;
		}
		else if (hitTileX != INT_MIN)
		{
			bottle->position = Vector2Add(bottle->position, Vector2Scale(bottle->velocity, closestSweep.t));
			BreakBottle(bottle, (Vector2) { 0 }, closestSweep.normal);
			ListSwapRemove(&bottles, i);
			--i;
		}
		else
		{
			int numClouds = RandomInt(0, 3);
			for (int j = 0; j < numClouds; ++j)
			{
				Cloud *cloud = ListAllocate(&clouds);
				if (not cloud)
					break;

				cloud->position = bottle->position;
				cloud->position.x += RandomFloat(-2, +2);
				cloud->position.y += RandomFloat(-2, +2);
				cloud->velocity = (Vector2){ RandomFloat(-2, +2), RandomFloat(-2, +2) };
				cloud->frame = 0;
				cloud->radius = RandomFloat(2, 4);
				cloud->color = GREEN;
				cloud->lifetime = 20;
			}

			bottle->position = Vector2Add(bottle->position, bottle->velocity);
			if (not CheckCollisionCircleRec(bottle->position, BOTTLE_RADIUS, (Rectangle) { 0, 0, SCREEN_SIZE, SCREEN_SIZE }))
			{
				ListSwapRemove(&bottles, i);
				--i;
			}
		}
	}
}
void UpdateChocoro(void)
{
	bool inGas = false;
	RingForeach(i, gases)
	{
		Gas *gas = &gases.items[i];
		if (gas->frame >= 0 and gas->frame < GAS_LINGER_FRAMES - GAS_FADE_OUT_FRAMES)
		{
			if (CheckCollisionCircles(chocoro.position, CHOCORO_RADIUS, gas->position, gas->radius))
			{
				inGas = true;
				break;
			}
		}
	}

	RingAdd(&chocoro.trail, chocoro.position);
	chocoro.zapFrames = MaxInt(chocoro.zapFrames - 1, 0);

	if (chocoro.zapFrames)
	{
		if (not IsMusicStreamPlaying(zapSound))
		{
			zapVolume = 1;
			SetMusicVolume(zapSound, zapVolume);
			PlayMusicStream(zapSound);
			PlaySound(dashSound);
		}
		SetMusicPitch(zapSound, PerlinNoise1(0xCAAC, 1.5f, 1.6f, (float)Time.now));
	}
	else
	{
		zapVolume *= 0.90f;
		if (zapVolume < 0.01f)
			StopMusicStream(zapSound);
		else
			SetMusicVolume(zapSound, zapVolume);
	}

	if (not chocoro.zapFrames)
		chocoro.velocity.y += GRAVITY;
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		if (chocoro.dashReady or chocoro.zapReady)
		{
			Vector2 toMouse = Vector2Subtract(Input.mouseUnclampedPosition, chocoro.position);
			Vector2 dashDirection = Vector2Normalize(toMouse);
			float dashSpeed = chocoro.zapReady ? ZAP_SPEED : DASH_SPEED;
			float speedInDirection = dashSpeed + fmaxf(0, Vector2DotProduct(dashDirection, chocoro.velocity));
			chocoro.velocity = Vector2Scale(dashDirection, speedInDirection);

			//chocoro.velocity = Vector2Add(chocoro.velocity, dashVelocity);
			//chocoro.velocity = dashVelocity;
			chocoro.lastDashFrame = Time.frame;
			if (chocoro.zapReady)
			{
				chocoro.zapFrames = ZAP_FRAMES;
				SetSoundPitch(thunder0Sound, RandomFloat(1.2f, 1.3f));
				PlaySound(thunder0Sound);
			}
			chocoro.dashReady = false;
			chocoro.zapReady = false;

			camera.trauma += 0.6f;

			float intensity = Clamp01(Vector2Length(chocoro.velocity) / 10);
			SetSoundVolume(dashSound, 0.8f + 0.5f * intensity);
			SetSoundPitch(dashSound, 0.9f + intensity);
			PlaySound(dashSound);

			Vector2 particleDirection = Vector2Negate(Vector2Normalize(chocoro.velocity));
			Vector2 span = Vector2Scale(dashDirection, CHOCORO_RADIUS + 1);
			Vector2 left = RotateCounterClockwise(span);
			Vector2 right = RotateClockwise(span);
			int numLines = RandomInt(10, 15);
			for (int i = 0; i < numLines; ++i)
			{
				DashLine *line = RingAllocate(&dashLines);
				float t = RandomNormal(0.5f, 0.4f);
				float lobe = 0.5f * (cosf((t - 0.5f) * PI) - 1) * CHOCORO_RADIUS;
				Vector2 offset = Vector2Lerp(left, right, t);
				offset = Vector2Add(offset, Vector2Scale(particleDirection, lobe));
				line->position = Vector2Add(Vector2Add(chocoro.position, Vector2Scale(chocoro.velocity, 3)), offset);
				line->velocity = Vector2Scale(particleDirection, RandomFloat(10, 30) - lobe);
				line->frameStart = Time.frame + RandomInt(0, 4);
				line->frameEnd = line->frameStart + RandomInt(5, 10);
			}
		}
	}
	if (not chocoro.zapFrames)
	{
		float friction = inGas ? GAS_FRICTION : AIR_FRICTION;
		chocoro.velocity = Vector2Scale(chocoro.velocity, 1 - friction);
		chocoro.velocity.y += GRAVITY * (friction - AIR_FRICTION);
	}

	chocoro.velocity = Vector2ClampValue(chocoro.velocity, 0, MAX_SPEED); // Otherwise weird stuff happens with lots of trampolines :)
	bool hitTileThisFrame = false;

	Vector2 remainingMotion = chocoro.velocity;
	for (int iteration = 0; iteration < 10; ++iteration)
	{
		Sweep closestSweep = { 1 };

		int hitTileX = INT_MIN;
		int hitTileY = INT_MIN;
		Sweep tileSweep = SweepCircleTiles(chocoro.position, remainingMotion, CHOCORO_RADIUS, &hitTileX, &hitTileY);
		if (tileSweep.t < closestSweep.t)
			closestSweep = tileSweep;

		int hitBalloon = INT_MIN;
		for (int i = 0; i < balloons.count; ++i)
		{
			Balloon *balloon = &balloons.items[i];
			if (not balloon->framesPopped)
			{
				Sweep sweep = SweepCircleCircle(chocoro.position, remainingMotion, CHOCORO_RADIUS, balloon->position, BALLOON_RADIUS + 2);
				if (sweep.t < closestSweep.t)
				{
					hitBalloon = i;
					hitTileX = INT_MIN;
					hitTileY = INT_MIN;
					closestSweep = sweep;
				}
			}
		}

		int hitGaser = INT_MIN;
		for (int i = 0; i < gasers.count; ++i)
		{
			Gaser *gaser = &gasers.items[i];
			if (not gaser->framesKilled)
			{
				Rectangle rect = RectangleFromCenterV(gaser->position, (Vector2) { GASER_WIDTH, GASER_HEIGHT });
				Sweep sweep = SweepCircleRect(chocoro.position, remainingMotion, CHOCORO_RADIUS, rect);
				if (sweep.t < closestSweep.t)
				{
					hitGaser = i;
					hitTileX = INT_MIN;
					hitTileY = INT_MIN;
					hitBalloon = INT_MIN;
					closestSweep = sweep;
				}
			}
		}

		int hitGunner = INT_MIN;
		for (int i = 0; i < gunners.count; ++i)
		{
			Gunner *gunner = &gunners.items[i];
			if (not gunner->framesKilled)
			{
				Rectangle rect = RectangleFromCenterV(gunner->position, (Vector2) { GUNNER_WIDTH, GUNNER_HEIGHT });
				Sweep sweep = SweepCircleRect(chocoro.position, remainingMotion, CHOCORO_RADIUS, rect);
				if (sweep.t < closestSweep.t)
				{
					hitGunner = i;
					hitGaser = INT_MIN;
					hitTileX = INT_MIN;
					hitTileY = INT_MIN;
					hitBalloon = INT_MIN;
					closestSweep = sweep;
				}
			}
		}

		int hitBottle = INT_MIN;
		for (int i = 0; i < bottles.count; ++i)
		{
			Bottle *bottle = &bottles.items[i];
			Sweep sweep = SweepCircleCircle(chocoro.position, remainingMotion, CHOCORO_RADIUS, bottle->position, BOTTLE_RADIUS);
			if (sweep.t < closestSweep.t)
			{
				hitBottle = i;
				hitTileX = INT_MIN;
				hitTileY = INT_MIN;
				hitBalloon = INT_MIN;
				hitGaser = INT_MIN;
				hitGunner = INT_MIN;
				closestSweep = sweep;
			}
		}

		Vector2 normal = closestSweep.normal;
		Vector2 tangent = RotateClockwise(normal);
		Vector2 delta = Vector2Scale(remainingMotion, closestSweep.t);
		chocoro.position = Vector2Add(chocoro.position, delta);
		remainingMotion = Vector2Subtract(remainingMotion, delta);

		if (hitTileX != INT_MIN)
		{
			float magnitude = Vector2Length(chocoro.velocity);
			float soundIntensity = Clamp01(magnitude / 10);

			Tile tile = tiles[hitTileY][hitTileX];
			if (not chocoro.hitTileLastFrame)
			{
				chocoro.hitTileLastFrame = true;
				if (tile == TILE_SLIME)
				{
					Sound sound = slimeSounds[RandomInt(0, countof(slimeSounds))];
					SetSoundVolume(sound, 1.0f + soundIntensity);
					PlaySound(sound);
				}
				else if (tile == TILE_TRAMPOLINE and soundIntensity > 0.05f)
				{
					SetSoundPitch(trampolineSound, 0.7f + 0.5f * soundIntensity);
					SetSoundVolume(trampolineSound, 0.5f + soundIntensity);
					PlaySound(trampolineSound);
				}
				else if ((tile == TILE_WALL or tile == TILE_ENTRANCE or tile == TILE_EXIT or tile == TILE_CARPET) and soundIntensity > 0.05f)
				{
					SetSoundPitch(wallSound, 0.5f + soundIntensity);
					SetSoundVolume(wallSound, 0.5f + soundIntensity);
					PlaySound(wallSound);
				}
			}

			remainingMotion = Vector2Reflect(remainingMotion, normal);
			chocoro.velocity = Vector2Reflect(chocoro.velocity, normal);
			if (tile == TILE_SLIME)
				chocoro.zapFrames = 0;
			if (not chocoro.zapFrames)
				chocoro.dashReady = true;
			if (tile == TILE_CARPET)
				chocoro.zapReady = true;

			Vector2 out = Vector2Normalize(chocoro.velocity);
			float outAngle = Atan2Vector(out);
			float directness = Clamp01(Vector2DotProduct(normal, out));

			remainingMotion = Vector2Scale(remainingMotion, 1 - TileFriction[tile]);
			chocoro.velocity = Vector2Scale(chocoro.velocity, 1 - TileFriction[tile]);

			remainingMotion = Vector2Add(remainingMotion, Vector2Scale(Vector2Negate(normal), TileStickyness[tile]));
			chocoro.velocity = Vector2Add(chocoro.velocity, Vector2Scale(Vector2Negate(normal), TileStickyness[tile]));
			hitTileThisFrame = true;

			camera.trauma += Clamp(0.1f * (magnitude - 1.2f) * directness, 0, 0.4f);

			float impact = magnitude * directness;
			int numClouds = (int)(10 * (impact - 2));
			for (int i = 0; i < numClouds; ++i)
			{
				Cloud *cloud = ListAllocate(&clouds);
				if (not cloud)
					break;

				float radius = 0.5f + RandomFloat(0, TileDebrisSize[tile]);
				Vector2 offset = Vector2Add(
					Vector2Scale(normal, RandomFloat(0, 2)),
					Vector2Scale(tangent, RandomFloat(-1, +1))
				);
				cloud->frame = 0;
				cloud->lifetime = RandomInt(15, 25);
				cloud->position = Vector2Add(chocoro.position, offset);
				cloud->velocity = Vector2FromPolar(impact * RandomFloat(1, 4), RandomNormal(outAngle, PI / 4));
				cloud->radius = radius;

				cloud->color = RandomBool(0.2f) ? ORANGE : TileColors[tile];
				cloud->color = ColorBlend(cloud->color, BLACK, RandomFloat(0, 0.8f));
			}
		}
		else if (hitBalloon != INT_MIN)
		{
			Balloon *balloon = &balloons.items[hitBalloon];
			balloon->framesPopped++;

			float intensity = Clamp(Vector2Length(chocoro.velocity), 0, 6) / 6.0f;
			Vector2 direction = Vector2Normalize(chocoro.velocity);
			float angle = Atan2Vector(direction);

			chocoro.dashReady = true;
			chocoro.velocity.y = fminf(-BALLOON_BOOST, chocoro.velocity.y);
			chocoro.freezeFrames = MaxInt(chocoro.freezeFrames, 4);
			camera.trauma += 0.5f;

			for (int i = 0; i < 50; ++i)
			{
				Cloud *cloud = ListAllocate(&clouds);
				if (not cloud)
					break;

				cloud->frame = 0;
				cloud->lifetime = RandomInt(15, 30);
				cloud->position = balloon->position;
				cloud->position.x += RandomFloat(-2, +2);
				cloud->position.y += RandomFloat(-2, +2);
				cloud->velocity = Vector2FromPolar(3 + intensity * RandomFloat(20, 40), angle + RandomNormal(0, PI / 4));
				cloud->radius = 0.5f;
				cloud->color = ColorAlpha(ScaleBrightness(RandomBool(0.6f) ? RED : GRAY, RandomFloat(-0.5f, 0)), 0.3f);
			}

			for (int i = 0; i < 20; ++i)
			{
				DashLine *line = RingAllocate(&dashLines);
				if (not line)
					break;

				Vector2 out = Vector2FromPolar(1, RandomFloat(0, 2 * PI));

				line->position = Vector2Add(balloon->position, Vector2Scale(out, BALLOON_RADIUS + 1));
				line->velocity = Vector2Scale(out, RandomFloat(8, 12));
				line->frameStart = Time.frame + RandomInt(1, 3);
				line->frameEnd = line->frameStart + RandomInt(3, 6);
			}

			SetSoundPitch(popSound, 1.1f - 0.3f * intensity);
			SetSoundVolume(popSound, 0.8f + 0.5f * intensity);
			PlaySound(popSound);
			
			PlayPing();
		}
		else if (hitGaser != INT_MIN)
		{
			Gaser *gaser = &gasers.items[hitGaser];
			gaser->framesKilled = 1;
			chocoro.freezeFrames = MaxInt(chocoro.freezeFrames, 6);
			camera.trauma += 0.6f;

			Vector2 direction = Vector2Normalize(chocoro.velocity);
			float angle = Atan2Vector(direction);

			PlayPing();
			PlaySound(ghostSound);

			for (int i = 0; i < 25; ++i)
			{
				Cloud *cloud = ListAllocate(&clouds);
				if (not cloud)
					break;

				cloud->position = gaser->position;
				cloud->velocity = Vector2FromPolar(RandomFloat(20, 30), angle + RandomNormal(0, PI / 6));
				cloud->frame = RandomInt(-1, -3);
				cloud->lifetime = RandomInt(20, 30);
				cloud->radius = RandomFloat(1.0f, 3.0f);
				cloud->color = BLACK;
			}
		}
		else if (hitGunner != INT_MIN)
		{
			Gunner *gunner = &gunners.items[hitGunner];
			gunner->framesKilled = 1;
			chocoro.freezeFrames = MaxInt(chocoro.freezeFrames, 6);
			camera.trauma += 0.6f;

			Vector2 direction = Vector2Normalize(chocoro.velocity);
			float angle = Atan2Vector(direction);

			PlayPing();
			PlaySound(hitSound);

			int numFiringGunners = 0;
			int numCharginGunners = 0;
			for (int i = 0; i < gunners.count; ++i)
			{
				int frame = WrapInt(gunners.items[i].framesSeenChocoro, 0, GUNNER_CYCLE_FRAMES);
				if (frame >= GUNNER_FIRE_START)
					++numFiringGunners;
				if (frame >= GUNNER_AIM_FRAMES and frame < GUNNER_FIRE_START)
					++numCharginGunners;
			}

			int frame = WrapInt(gunner->framesSeenChocoro, 0, GUNNER_CYCLE_FRAMES);
			if (numFiringGunners == 0 or (numFiringGunners == 1 and frame >= GUNNER_FIRE_START))
				StopSound(laserBeamSound);
			if (numCharginGunners == 0 or (numCharginGunners == 1 and frame >= GUNNER_AIM_FRAMES and frame < GUNNER_FIRE_START))
				StopSound(laserChargeSound);

			for (int i = 0; i < 25; ++i)
			{
				Cloud *cloud = ListAllocate(&clouds);
				if (not cloud)
					break;

				cloud->position = gunner->position;
				cloud->velocity = Vector2FromPolar(RandomFloat(20, 30), angle + RandomNormal(0, PI / 6));
				cloud->frame = RandomInt(-1, -3);
				cloud->lifetime = RandomInt(20, 30);
				cloud->radius = RandomFloat(1.0f, 3.0f);
				cloud->color = RandomBool(0.5f) ? Grayscale(RandomFloat(0.2f, 0.5f)) : DARKGREEN;
			}
		}
		else if (hitBottle != INT_MIN)
		{
			Bottle bottle = bottles.items[hitBottle];
			BreakBottle(&bottle, chocoro.velocity, Vector2Normalize(chocoro.velocity));
			ListSwapRemove(&bottles, hitBottle);
			
			chocoro.velocity = Vector2Subtract(chocoro.velocity, bottle.velocity);
			remainingMotion = Vector2Subtract(chocoro.velocity, bottle.velocity);
			deathLine1 = "Chocoro suffered a bottle induced concussion.";
			deathLine2 = "He will never recover.";
			gameState = GAME_STATE_GameOver;
		}

		if (remainingMotion.x == 0 or remainingMotion.y == 0)
			break;
	}

	for (int i = 0; i < gunners.count; ++i)
	{
		Gunner *gunner = &gunners.items[i];
		if (gunner->framesKilled)
			continue;

		int frame = WrapInt(gunner->framesSeenChocoro, 0, GUNNER_CYCLE_FRAMES);
		if (frame <= GUNNER_FIRE_START)
			continue;

		float beamAngle = FlipAngle(gunner->aimAngle) * RAD2DEG;
		Rectangle beamRect = { gunner->position.x, gunner->position.y, 1000, BEAM_WIDTH };
		Vector2 beamOrigin = { -GUNNER_GUN_DISTANCE - 10, BEAM_RADIUS };
		if (CheckCollisionCircleRotatedRect(chocoro.position, CHOCORO_RADIUS, beamRect, beamOrigin, beamAngle))
		{
			deathLine1 = "Chocoro was blasted to bits.";
			deathLine2 = "There was nothing left.";
			gameState = GAME_STATE_GameOver;
			break;
		}
	}

	chocoro.hitTileLastFrame = hitTileThisFrame;
}
void UpdateClouds(void)
{
	for (int i = 0; i < clouds.count; ++i)
	{
		Cloud *cloud = &clouds.items[i];
		if (++cloud->frame > cloud->lifetime)
		{
			ListSwapRemove(&clouds, i);
			--i;
			continue;
		}
	}
}
void UpdateGas(void)
{
	RingForeach(i, gases)
	{
		Gas *gas = &gases.items[i];
		gas->frame++;
	}
}

// Draw

void DrawAlarm(Rectangle rect, int frame)
{
	float alarmT = frame / 120.0f;
	float alarmY0 = rect.y - 2;
	float alarmY1 = rect.y - 6;
	float alarmX = RectangleCenter(rect).x;
	float alarmY = Lerp(alarmY0, alarmY1, SineLerp(10 * alarmT));
	float sizeT = 2 - Smootherstep(alarmT, 0, 0.1f);
	DrawTextCenteredOffset(Fonts.game, "!", alarmX, alarmY, (float)(sizeT * Fonts.game.baseSize), RED, BLACK);
}
void DrawLogo(float alpha)
{
	float fontSize = 3 * (float)Fonts.game.baseSize;
	for (int i = 4; i >= 0; --i)
	{
		int x = SCREEN_CENTER + i;
		int y = SCREEN_CENTER - 60 + i;
		x += IntNoise1(Time.frame / 5, -1, +2, 2 * i + 0);
		y += IntNoise1(Time.frame / 5, -1, +2, 2 * i + 1);
		DrawTextCentered(Fonts.game, "Chocoro", (float)x, (float)y, fontSize, ColorAlpha(BLACK, alpha));
		DrawTextCentered(Fonts.game, "Chocoro", (float)x + 1, (float)y, fontSize, ColorAlpha(BLACK, alpha));
		DrawTextCentered(Fonts.game, "Chocoro", (float)x, (float)y + 1, fontSize, ColorAlpha(BLACK, alpha));
	}
	DrawTextCentered(Fonts.game, "Chocoro", SCREEN_CENTER, SCREEN_CENTER - 60, fontSize, ColorAlpha(ORANGE, alpha));
}
void DrawShutter(Vector2 center, float amountClosed)
{
	Rectangle screen = SCREEN_RECT;
	Vector2 topLeft = RectangleTopLeft(screen);
	Vector2 topRight = RectangleTopRight(screen);
	Vector2 bottomLeft = RectangleBottomLeft(screen);
	Vector2 bottomRight = RectangleBottomRight(screen);

	rlDrawRenderBatchActive();
	rlSetTexture(0);
	rlBegin(RL_QUADS);
	{
		rlColor(BLACK);

		rlVertex2fv(topLeft);
		rlVertex2fv(bottomLeft);
		rlVertex2fv(Vector2Lerp(bottomLeft, center, amountClosed));
		rlVertex2fv(Vector2Lerp(topLeft, center, amountClosed));

		rlVertex2fv(bottomLeft);
		rlVertex2fv(bottomRight);
		rlVertex2fv(Vector2Lerp(bottomRight, center, amountClosed));
		rlVertex2fv(Vector2Lerp(bottomLeft, center, amountClosed));

		rlVertex2fv(bottomRight);
		rlVertex2fv(topRight);
		rlVertex2fv(Vector2Lerp(topRight, center, amountClosed));
		rlVertex2fv(Vector2Lerp(bottomRight, center, amountClosed));

		rlVertex2fv(topRight);
		rlVertex2fv(topLeft);
		rlVertex2fv(Vector2Lerp(topLeft, center, amountClosed));
		rlVertex2fv(Vector2Lerp(topRight, center, amountClosed));
	}
	rlEnd();
	rlDrawRenderBatchActive();
}
void DrawCreditsLine(char *line, int y, float alpha, float margin)
{
	char *separator = strchr(line, ':');
	char *person = SlicePointer(line, separator);
	char *thing = Slice(separator, 1, -1);

	Font font = Fonts.raylib;
	float fontSize = (float)font.baseSize;
	float spacing = 1;

	float leftMargin = margin;
	float rightMargin = SCREEN_SIZE - margin;

	Color color = ColorAlpha(WHITE, alpha);
	Color highlight = ColorAlpha(BLACK, alpha);

	float x0 = leftMargin;
	DrawTextEx(font, person, (Vector2) { x0 + 1, (float)y }, fontSize, spacing, highlight);
	DrawTextEx(font, person, (Vector2) { x0, (float)y + 1 }, fontSize, spacing, highlight);
	DrawTextEx(font, person, (Vector2) { x0 + 1, (float)y + 1 }, fontSize, spacing, highlight);
	DrawTextEx(font, person, (Vector2) { x0, (float)y }, fontSize, spacing, color);

	Vector2 thingSize = MeasureTextEx(font, thing, fontSize, spacing);
	float x1 = rightMargin - thingSize.x;
	DrawTextEx(font, thing, (Vector2) { x1 + 1, (float)y }, fontSize, spacing, highlight);
	DrawTextEx(font, thing, (Vector2) { x1, (float)y + 1 }, fontSize, spacing, highlight);
	DrawTextEx(font, thing, (Vector2) { x1 + 1, (float)y + 1 }, fontSize, spacing, highlight);
	DrawTextEx(font, thing, (Vector2) { x1, (float)y }, fontSize, spacing, color);
}
void DrawScrollingDots(float t, float alpha)
{
	//const Color colors[] = {
	//	SKYBLUE,
	//	BLUE,
	//	{ 133, 127, 255, 255 },
	//};

	const Color colors[] = {
		ORANGE,
		//YELLOW,
		RED,
	};

	for (int i = 0; i < 400; ++i)
	{
		const float speed = 20;

		float x = FloatNoise1(__LINE__, 0, SCREEN_SIZE, i);
		float y = FloatNoise1(__LINE__, -20, SCREEN_SIZE + 20, i);
		float d = FloatNoise1(__LINE__, 1, 3, i);
		x += 20 * sinf(FloatNoise1(__LINE__, 0, 2 * PI, i) + t) / d;
		y = Wrap(y - t * (speed / d), -20, SCREEN_SIZE + 20);

		Color color = ColorAlpha(colors[IntNoise1(__LINE__, 0, countof(colors), i)], alpha / (1.5f * d));
		float size = 3 / d;
		DrawAntiAliasedRectangle(RectangleFromCenter(x, y, size, size), color);
	}
}
void DrawLight(float x, float y, float hr, float vr)
{
	int xi, yi;
	memcpy(&xi, &x, sizeof xi);
	memcpy(&yi, &y, sizeof yi);
	int seed = xi ^ yi;
	float time1 = (float)(Time.now * FloatNoise1(seed, 2, 3, 0xCA12));
	float time2 = (float)(Time.now * FloatNoise1(seed, 2, 3, 0xF334));
	hr *= PerlinNoise1(seed, 0.75f, 1.25f, time1);
	vr *= PerlinNoise1(~seed, 0.75f, 1.25f, time2);

	int numSegments = 16;
	rlBegin(RL_TRIANGLES);
	{
		for (int i = 0; i < numSegments; ++i)
		{
			float angle0 = 2 * PI * (((i + 0) % numSegments) / (float)numSegments);
			float angle1 = 2 * PI * (((i + 1) % numSegments) / (float)numSegments);
			float x0 = x + hr * cosf(angle0);
			float y0 = y + vr * sinf(angle0);
			float x1 = x + hr * cosf(angle1);
			float y1 = y + vr * sinf(angle1);

			rlColor4ub(240, 90, 82, 128);
			rlVertex2f(x, y);
			rlColor4f(0, 0, 0, 0);
			rlVertex2f(x0, y0);
			rlVertex2f(x1, y1);
		}
	}
	rlEnd();
}
void BeginChocoCamera(void)
{
	Camera2D shakyCam = camera.camera;
	camera.trauma = Clamp(camera.trauma, 0, 1.5f);
	float shake = Clamp01(camera.trauma);
	shake *= shake;
	shakyCam.offset.x += shake * PerlinNoise1(1, -6, +6, camera.shakyTime);
	shakyCam.offset.y += shake * PerlinNoise1(2, -6, +6, camera.shakyTime);
	camera.trauma = fmaxf(0, camera.trauma - 0.04f);

	BeginMode2D(shakyCam);
}
void EndChocoCamera(void)
{
	EndMode2D();
}
void DrawTile(int x, int y)
{
	float grayscale = ((x + y) & 1) ? 1 : 0.95f;
	grayscale += FloatNoise2(__LINE__, -0.025f, +0.025f, x, y);
	Tile tile = tiles[y][x];
	int variant = tileVariants[y][x];
	Texture texture = TileTextures[tile][variant];
	int tx = x * TILE_SIZE;
	int ty = y * TILE_SIZE;
	DrawTexture(texture, tx, ty, Grayscale(grayscale));
	if ((tile == TILE_ENTRANCE or tile == TILE_EXIT) and TileIsPassable[tile])
		DrawRectangle(tx, ty, TILE_SIZE, TILE_SIZE, ColorAlpha(BLACK, 0.5f));
}
void DrawOutOfViewTiles(void)
{
	int x0 = -1;
	int x1 = NUM_TILES;
	int y0 = -1;
	int y1 = NUM_TILES;
	int cx0 = 0;
	int cx1 = NUM_TILES - 1;
	int cy0 = 0;
	int cy1 = NUM_TILES - 1;
	for (int y = 0; y < NUM_TILES; ++y)
	{
		DrawTexture(TileTextures[tiles[y][cx0]][tileVariants[y][cx0]], x0 * TILE_SIZE, y * TILE_SIZE, WHITE);
		DrawTexture(TileTextures[tiles[y][cx1]][tileVariants[y][cx1]], x1 * TILE_SIZE, y * TILE_SIZE, WHITE);
	}
	for (int x = 0; x < NUM_TILES; ++x)
	{
		DrawTexture(TileTextures[tiles[cy0][x]][tileVariants[cy0][x]], x * TILE_SIZE, y0 * TILE_SIZE, WHITE);
		DrawTexture(TileTextures[tiles[cy1][x]][tileVariants[cy1][x]], x * TILE_SIZE, y1 * TILE_SIZE, WHITE);
	}
	DrawTexture(TileTextures[tiles[cy0][cx0]][tileVariants[cy0][cx0]], x0 * TILE_SIZE, y0 * TILE_SIZE, WHITE);
	DrawTexture(TileTextures[tiles[cy0][cx1]][tileVariants[cy0][cx1]], x1 * TILE_SIZE, y0 * TILE_SIZE, WHITE);
	DrawTexture(TileTextures[tiles[cy1][cx0]][tileVariants[cy1][cx0]], x0 * TILE_SIZE, y1 * TILE_SIZE, WHITE);
	DrawTexture(TileTextures[tiles[cy1][cx1]][tileVariants[cy1][cx1]], x1 * TILE_SIZE, y1 * TILE_SIZE, WHITE);
}
void DrawBackgroundTiles(void)
{
	for (int y = 0; y < NUM_TILES; ++y)
		for (int x = 0; x < NUM_TILES; ++x)
			if (TileIsPassable[tiles[y][x]])
				DrawTile(x, y);
}
void DrawForegroundTiles(void)
{
	for (int y = 0; y < NUM_TILES; ++y)
		for (int x = 0; x < NUM_TILES; ++x)
			if (not TileIsPassable[tiles[y][x]])
				DrawTile(x, y);
}
void DrawBalloons(void)
{
	for (int i = 0; i < balloons.count; ++i)
	{
		Balloon *balloon = &balloons.items[i];
		if (not balloon->framesPopped)
		{
			Vector2 position = balloon->position;
			if (gameState != GAME_STATE_Editor)
				position = Vector2Add(position, balloon->offset);
			DrawTextureCenteredV(balloonTexture, position, WHITE);
		}
	}
}
void DrawGasers(void)
{
	for (int i = 0; i < gasers.count; ++i)
	{
		Gaser *gaser = &gasers.items[i];

		Color tint = WHITE;
		if (gaser->framesKilled)
			tint = ColorAlpha(tint, Remap((float)gaser->framesKilled, 0, GASER_FADE_FRAMES, 1, 0));

		DrawTextureCenteredV(gaserTexture, gaser->position, tint);

		if (gaser->framesSeenChocoro and not gaser->framesKilled)
		{
			if (gaser->bottleCooldownFrames < GASER_BOTTLE_COOLDOWN_FRAMES / 2)
			{
				DrawTextureCenteredV(bottleTexture, gaser->position, WHITE);
			}

			Rectangle rect = RectangleFromCenterV(gaser->position, (Vector2) { GASER_WIDTH, GASER_HEIGHT });
			DrawAlarm(rect, gaser->framesSeenChocoro);
		}
	}
}
void DrawGunners(void)
{
	for (int i = 0; i < gunners.count; ++i)
	{
		Gunner *gunner = &gunners.items[i];

		Color tint = WHITE;
		if (gunner->framesKilled)
			tint = ColorAlpha(tint, Remap((float)gunner->framesKilled, 0, GUNNER_FADE_FRAMES, 1, 0));

		if (gunner->framesSeenChocoro)
		{
			float angle = gunner->aimAngle;
			bool flip = chocoro.position.x > gunner->position.x;
			if (flip)
				angle = FlipAngle(angle);

			DrawTextureFlippedV(gunnerAimingTexture, gunner->position, flip, false, tint);
			DrawTextureRotatedFlippedV(gunTexture, gunner->position, angle * RAD2DEG, flip, false, tint);
			if (not gunner->framesKilled)
			{
				Rectangle rect = RectangleFromCenterV(gunner->position, (Vector2) { GASER_WIDTH, GASER_HEIGHT });
				DrawAlarm(rect, gunner->framesSeenChocoro);
			}
		}
		else
		{
			DrawTextureCenteredV(gunnerIdleTexture, gunner->position, tint);
			DrawTextureCenteredV(gunTexture, gunner->position, tint);
		}
	}
}
void DrawLasers(void)
{
	for (int i = 0; i < gunners.count; ++i)
	{
		Gunner *gunner = &gunners.items[i];
		if (gunner->framesKilled)
			continue;

		int frame = WrapInt(gunner->framesSeenChocoro, 0, GUNNER_CYCLE_FRAMES);
		if (frame >= GUNNER_AIM_FRAMES and frame < GUNNER_FIRE_START)
		{
			int chargeFrame = frame - GUNNER_AIM_FRAMES;
			float t = Smoothstep01(1.5f * (float)chargeFrame / GUNNER_CHARGE_FRAMES);
			Vector2 gunPosition = GunnerGunPosition(gunner);
			DrawCircleV(gunPosition, 10 * t, ColorAlpha(RED, 0.8f * t));
		}
		if (frame > GUNNER_FIRE_START)
		{
			int fireFrame = frame - GUNNER_FIRE_START;
			float t = 1;
			if (fireFrame < 5)
				t = fireFrame / 5.0f;
			else if (fireFrame > GUNNER_FIRE_FRAMES - 5)
				t = (GUNNER_FIRE_FRAMES - fireFrame) / 5.0f;

			float beamAngle = FlipAngle(gunner->aimAngle) * RAD2DEG;
			Rectangle beamRect = { gunner->position.x, gunner->position.y, 1000, BEAM_WIDTH };
			Vector2 beamOrigin = { -GUNNER_GUN_DISTANCE, BEAM_RADIUS };
			Rectangle beamTextureRect = { -2.5f * Time.frame, 49, (float)beamTexture.width, 30 };
			DrawTexturePro(beamTexture, beamTextureRect, beamRect, beamOrigin, beamAngle, ColorAlpha(WHITE, t * 0.8f));

			Rectangle beamStartTextureRect = { 0, 0, (float)beamStartTexture.width, (float)beamStartTexture.height };
			Rectangle beamStartRect = { gunner->position.x, gunner->position.y, BEAM_WIDTH, BEAM_WIDTH };
			Vector2 beamStartOrigin = { -GUNNER_GUN_DISTANCE + 16, BEAM_RADIUS };
			DrawTexturePro(beamStartTexture, beamStartTextureRect, beamStartRect, beamStartOrigin, beamAngle, WHITE);
		}
	}
}
void DrawBottles(void)
{
	for (int i = 0; i < bottles.count; ++i)
	{
		Bottle *bottle = &bottles.items[i];
		float angle = 10.0f * bottle->frame;
		if (bottle->velocity.x < 0)
			angle = -angle;
		DrawTextureRotatedV(bottleTexture, bottle->position, angle, WHITE);
	}
}
void DrawChocoroTrail(void)
{
	Vector2 positions[countof(chocoro.trail.items)];
	int cursor = 0;
	RingForeach(i, chocoro.trail)
		positions[cursor++] = chocoro.trail.items[i];

	Texture texture =
		chocoro.zapFrames or chocoro.zapReady ? chocoroZappedTexture :
		chocoro.dashReady ? chocoroTexture :
		chocoroExhaustedTexture;
	for (int i = 0; i < cursor; ++i)
	{
		Vector2 position = positions[i];
		float power = (float)(cursor - i - 2);
		float alpha = powf(0.8f, power);
		Color tint = ColorAlpha(WHITE, alpha);
		DrawTextureFlippedV(texture, position, chocoro.velocity.x < 0, false, tint);
	}

	if (chocoro.zapFrames)
	{
		rlDrawRenderBatchActive();
		rlBegin(RL_QUADS);
		{
			rlSetTexture(zapTexture.id);

			Vector2 next = chocoro.position;
			float t = (float)(7.14967 * Time.now);
			float alpha = 1;
			for (int i = cursor - 1; i >= 0; --i)
			{
				Vector2 position = positions[i];
				Vector2 delta = Vector2Subtract(next, position);
				Vector2 direction = Vector2Normalize(delta);
				Vector2 left = RotateCounterClockwise(direction);
				Vector2 right = RotateClockwise(direction);
				Vector2 bottomLeft = Vector2Add(position, Vector2Scale(left, 6));
				Vector2 bottomRight = Vector2Add(position, Vector2Scale(right, 6));
				Vector2 topLeft = Vector2Add(next, Vector2Scale(left, 6));
				Vector2 topRight = Vector2Add(next, Vector2Scale(right, 6));

				float u0 = t + (float)i / cursor;
				float u1 = t + (float)(i + 1) / cursor;

				rlColor4f(1, 1, 1, alpha);
				rlTexCoord2f(u0, 0.45f); rlVertex2f(bottomLeft.x, bottomLeft.y);
				rlTexCoord2f(u0, 0.55f); rlVertex2f(bottomRight.x, bottomRight.y);
				rlTexCoord2f(u1, 0.55f); rlVertex2f(topRight.x, topRight.y);
				rlTexCoord2f(u1, 0.45f); rlVertex2f(topLeft.x, topLeft.y);

				alpha *= 0.92f;
				next = position;
			}
		}
		rlEnd();
		rlDrawRenderBatchActive();
	}
}
void DrawChocoro(void)
{
	Texture texture = 
		gameState == GAME_STATE_GameOver or previousGameState == GAME_STATE_GameOver ? chocoroDeadTexture :
		chocoro.zapFrames or chocoro.zapReady ? chocoroZappedTexture : 
		chocoro.dashReady ? chocoroTexture : 
		chocoroExhaustedTexture;
	DrawTextureFlippedV(texture, chocoro.position, chocoro.velocity.x < 0, false, WHITE);
	
	if (chocoro.zapFrames or chocoro.zapReady)
	{
		int seed = Time.frame / 2;
		int numSparks = IntNoise1(seed, 20, 40, -1);
		for (int i = 0; i < numSparks; ++i)
		{
			float distance = FloatNoise1(seed, 0, 3 * CHOCORO_RADIUS, i * 5 + 0);
			float angle = FloatNoise1(seed, 0, 2 * PI, i * 5 + 1);
			float alpha = FloatNoise1(seed, 0.1f, 0.5f, i * 5 + 2);
			Vector2 position = Vector2Add(chocoro.position, Vector2FromPolar(distance, angle));
			DrawPixelV(position, ColorAlpha(YELLOW, alpha));
		}
	}
}
void DrawDashIndicator(void)
{
	Vector2 playerToMouse = Vector2Subtract(Input.mouseUnclampedPosition, chocoro.position);
	Vector2 dashDirection = Vector2Normalize(playerToMouse);
	Vector2 dashVelocity = Vector2Scale(dashDirection, DASH_SPEED);
	Vector2 velocityAfterDash = Vector2Add(chocoro.velocity, dashVelocity);
	float length = Clamp(5 + 2.0f * Vector2DotProduct(dashDirection, velocityAfterDash), 5, 15);
	Vector2 tip = Vector2Add(chocoro.position, Vector2Scale(dashDirection, length));
	Vector2 leftNormal = RotateCounterClockwise(dashDirection);
	Vector2 rightNormal = RotateClockwise(dashDirection);
	Vector2 split = Vector2Add(chocoro.position, Vector2Scale(dashDirection, length - 5));
	Vector2 left = Vector2Add(split, Vector2Scale(leftNormal, 5));
	Vector2 right = Vector2Add(split, Vector2Scale(rightNormal, 5));
	bool recentlyDashed = Time.frame - chocoro.lastDashFrame < 4;
	float alpha = recentlyDashed or chocoro.zapReady ? 1.0f : chocoro.dashReady ? 0.5f : 0.05f;
	float thickness = recentlyDashed or chocoro.zapReady ? 2.0f : 1.0f;
	Color color = recentlyDashed or chocoro.zapReady ? YELLOW : ORANGE;
	//DrawLineEx(player.position, tip, thickness, ColorAlpha(DARKGRAY, alpha));
	DrawLineEx(tip, left, thickness, ColorAlpha(color, alpha));
	DrawLineEx(tip, right, thickness, ColorAlpha(color, alpha));
}
void DrawDashLines(void)
{
	for (int i = 0; i < countof(dashLines.items); ++i)
	{
		DashLine *line = &dashLines.items[i];
		float t = (Time.frame - line->frameStart) / (float)(line->frameEnd - line->frameStart);
		if (t < 0 or t > 1)
			continue;

		Vector2 start = line->position;
		Vector2 end = Vector2Add(start, Vector2Scale(line->velocity, t));
		DrawLineV(start, end, ColorAlpha(WHITE, 0.5f));
	}
}
void DrawClouds(void)
{
	for (int i = 0; i < clouds.count; ++i)
	{
		Cloud *cloud = &clouds.items[i];
		float t = (float)cloud->frame / cloud->lifetime;
		if (t > 0)
		{
			Vector2 position = Vector2Add(cloud->position, Vector2Scale(cloud->velocity, t));
			Color color = ColorAlpha(cloud->color, 1 - t);
			DrawCircleV(position, cloud->radius, color);
		}
	}
}
void DrawGas(void)
{
	RingForeach(i, gases)
	{
		Gas *gas = &gases.items[i];
		if (gas->frame >= 0 and gas->frame < GAS_LINGER_FRAMES)
		{
			float t = 0;
			if (gas->frame < GAS_FADE_IN_FRAMES)
				t = Smoothstep((float)gas->frame, 0, GAS_FADE_IN_FRAMES);
			else if (gas->frame > GAS_LINGER_FRAMES - GAS_FADE_OUT_FRAMES)
				t = Smoothstep((float)gas->frame, GAS_LINGER_FRAMES, GAS_LINGER_FRAMES - GAS_FADE_OUT_FRAMES);
			else
				t = 1;

			Vector2 position = gas->position;
			float range = Lerp(6, 2, gas->radius / 20);
			float now = 5 * (float)gas->frame / FPS;
			position.x += PerlinNoise1(0xFAAB, -range, +range, now / gas->radius);
			position.y += PerlinNoise1(0xCAAB, -range, +range, now / gas->radius);
			DrawCircleV(position, gas->radius, ColorAlpha(GREEN, 0.3f * t));
		}
	}
}
void DrawRestartIndicator(void)
{
	int delay = gameState == GAME_STATE_GameOver ? 0 : FPS;
	float t = Remap((float)mouseHoldFrames, (float)delay, RESTART_HOLD_FRAMES - (FPS - (float)delay), 0, 1);
	t = Clamp01(t);
	if (t <= 0)
		return;

	float r = 5 * t;
	float r0 = 5;
	float r1 = 5 + r;
	Color color = ColorAlpha(WHITE, Smoothstep01(t));
	DrawRing(Input.mousePosition, r0, r1, 0, t * 360, 20, color);

	Vector2 textPos = Input.mousePosition;
	textPos.x += 12;
	textPos.y -= 6;
	DrawTextEx(Fonts.game, "Restart", textPos, (float)Fonts.game.baseSize, 0, color);
}

// Splash

int splashScreenState;
int splashClickCount;
Coroutine splashRoutines[3];
void Splash_Init(void)
{
	StopMusicStream(music);
	SeekMusicStream(music, 0);
	SetSoundPitch(thunder0Sound, 0.5f);
	splashScreenState = 0;
	ZeroArray(splashRoutines);
	RingClear(&chocoro.trail);
	bestFramesPlaying = LoadIniInt("BestTimeInFrames", INT_MAX);
	splashClickCount = 0;
}
void Splash_Update(void)
{
	if (IsKeyPressed(KEY_SPACE) or IsKeyPressed(KEY_ESCAPE) or IsKeyPressed(KEY_R) or IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		if (splashScreenState == 0)
		{
			if (++splashClickCount >= 2)
			{
				StopAllSounds();
				splashScreenState = 1;
			}
		}
		else if (splashScreenState == 1)
		{
			StopAllSounds();
			splashScreenState = 2;
		}
	}
}
void Splash_Render(void)
{
	DrawTexture(castleTexture, 0, 0, WHITE);

	DrawLight(77, 200, 12, 16);
	DrawLight(56, 205, 8, 12);
	DrawLight(63, 190, 8, 12);
	DrawLight(94, 174, 12, 12);
	DrawLight(104, 160, 8, 12);
	DrawLight(105, 187, 8, 12);
	DrawLight(111, 173, 8, 12);
	DrawLight(120, 151, 8, 12);
	DrawLight(121, 201, 8, 12);
	DrawLight(125, 132, 16, 12);
	DrawLight(127, 175, 8, 12);
	DrawLight(129, 108, 14, 12);
	DrawLight(136, 149, 14, 14);
	DrawLight(137, 210, 8, 12);
	DrawLight(138, 189, 18, 16);
	DrawLight(140, 132, 16, 12);
	DrawLight(141, 167, 8, 12);
	DrawLight(150, 147, 8, 12);
	DrawLight(150, 160, 8, 12);
	DrawLight(156, 192, 8, 12);
	DrawLight(160, 205, 8, 12);
	DrawLight(170, 197, 8, 12);
	DrawLight(174, 212, 8, 12);
	DrawLight(176, 197, 8, 12);
	DrawLight(189, 209, 8, 12);
	DrawLight(190, 200, 8, 12);

	if (splashScreenState == 0)
	{
		if (splashClickCount == 1)
		{
			DrawTexture(mouseClickTexture, SCREEN_CENTER - 13, 2, WHITE);
			DrawTextCenteredOffset(Fonts.raylib, "Skip", SCREEN_CENTER + 9, 10, (float)Fonts.raylib.baseSize, WHITE, BLACK);
			//DrawText("Click to skip", 0, 0, Fonts.raylib.baseSize, WHITE);
		}

		coroutine(&splashRoutines[splashScreenState])
		{
			static int i;

			for (i = 0; i < 60; ++i)
			{
				float t = i / 60.0f;
				DrawRectangle(0, 0, SCREEN_SIZE, SCREEN_SIZE, ColorAlpha(BLACK, Smoothstep01(1 - t)));
				yield();
			}
			for (i = 0; i < 10; ++i)
			{
				yield();
			}

			for (i = 0; i < 5; ++i)
			{
				if (i == 0)
				{
					PlaySound(thunder1Sound);
				}
				float t = i / 5.0f;
				DrawTexture(lightningTexture0, 0, 0, ColorAlpha(WHITE, Smoothstep01(t)));
				yield();
			}
			for (i = 0; i < 5; ++i)
			{
				float t = i / 5.0f;
				DrawTexture(lightningTexture0, 0, 0, ColorAlpha(WHITE, Smoothstep01(1 - t)));
				yield();
			}
			for (i = 0; i < 20; ++i)
			{
				yield();
			}

			for (i = 0; i < 5; ++i)
			{
				float t = i / 5.0f;
				DrawTexture(lightningTexture2, 170, 0, ColorAlpha(WHITE, Smoothstep01(t)));
				yield();
			}
			for (i = 0; i < 5; ++i)
			{
				float t = i / 5.0f;
				DrawTexture(lightningTexture2, 170, 0, ColorAlpha(WHITE, Smoothstep01(1 - t)));
				yield();
			}
			for (i = 0; i < 20; ++i)
			{
				yield();
			}

			for (i = 0; i < 5; ++i)
			{
				float t = i / 5.0f;
				DrawTexture(lightningTexture0, 0, 0, ColorAlpha(WHITE, Smoothstep01(t)));
				yield();
			}
			for (i = 0; i < 5; ++i)
			{
				float t = i / 5.0f;
				DrawTexture(lightningTexture0, 0, 0, WHITE);
				DrawTexture(lightningTexture2, 170, 0, ColorAlpha(WHITE, Smoothstep01(t)));
				yield();
			}
			for (i = 0; i < 5; ++i)
			{
				float t = i / 5.0f;
				DrawTexture(lightningTexture0, 0, 0, WHITE);
				DrawTexture(lightningTexture2, 170, 0, WHITE);
				DrawTexture(lightningTexture1, 88, 0, ColorAlpha(WHITE, Smoothstep01(t)));
				yield();
			}
			for (i = 0; i < 8; ++i)
			{
				if (i == 0)
				{
					PlaySound(thunder0Sound);
				}
				float t = i / 5.0f;
				DrawTexture(lightningTexture0, 0, 0, ColorAlpha(WHITE, Smoothstep01(1 - t)));
				DrawTexture(lightningTexture2, 170, 0, ColorAlpha(WHITE, Smoothstep01(1 - t)));
				DrawTexture(lightningTexture1, 88, 0, WHITE);
				DrawRectangle(0, 0, SCREEN_SIZE, SCREEN_SIZE, ColorAlpha(YELLOW, Smoothstep01(t)));
				yield();
			}
			for (i = 0; i < 5; ++i)
			{
				float t = i / 5.0f;
				DrawTexture(lightningTexture1, 88, 0, ColorAlpha(WHITE, 1 - t));
				DrawTexture(chocoroTexture, 97, 127, WHITE);
				DrawRectangle(0, 0, SCREEN_SIZE, SCREEN_SIZE, ColorAlpha(YELLOW, Smoothstep01(1 - t)));
				yield();
			}
			for (i = 0; i < 40; ++i)
			{
				float t = i / 10.0f;
				DrawLogo(Smoothstep01(t));
				DrawTexture(chocoroTexture, 97, 127, WHITE);
				yield();
			}
			for (i = 0; i < 30; ++i)
			{
				float t = i / 10.0f;
				float alpha = Smoothstep01(t);
				DrawLogo(1);
				DrawTexture(chocoroTexture, 97, 127, WHITE);

				DrawTextCenteredOffset(Fonts.raylib, "Click to start", SCREEN_CENTER, SCREEN_SIZE - 25, (float)Fonts.raylib.baseSize,
					ColorAlpha(ORANGE, alpha),
					ColorAlpha(BLACK, alpha));
				if (bestFramesPlaying != INT_MAX)
				{
					char *time = String("Best time: %s", TimeStringFromFrames(bestFramesPlaying));
					DrawTextCenteredOffset(Fonts.raylib, time, SCREEN_CENTER, SCREEN_SIZE - 10, (float)Fonts.raylib.baseSize,
						ColorAlpha(ORANGE, 0.5f * alpha),
						ColorAlpha(BLACK, 0.5f * alpha));
				}
				yield();
			}
			splashScreenState = 1;
		}
	}

	if (splashScreenState == 1)
	{
		coroutine(&splashRoutines[splashScreenState])
		{
			static int i;
			for (i = 0;; ++i)
			{
				int frame = i % 30;
				float t = 1 - Sawtooth(frame / 30.0f);
				float alpha = Remap(t, 0, 1, 0.4f, 1);

				DrawLogo(1);
				DrawTexture(chocoroTexture, 97, 127, WHITE);
				DrawTextCenteredOffset(Fonts.raylib, "Click to start", SCREEN_CENTER, SCREEN_SIZE - 25, (float)Fonts.raylib.baseSize,
					ColorAlpha(ORANGE, alpha),
					ColorAlpha(BLACK, alpha));
				if (bestFramesPlaying != INT_MAX)
				{
					char *time = String("Best time: %s", TimeStringFromFrames(bestFramesPlaying));
					DrawTextCenteredOffset(Fonts.raylib, time, SCREEN_CENTER, SCREEN_SIZE - 10, (float)Fonts.raylib.baseSize,
						ColorAlpha(ORANGE, 0.5f),
						ColorAlpha(BLACK, 0.5f));
				}
				yield();
			}
		}
	}

	if (splashScreenState == 2)
	{
		PlayMusicStream(music);

		coroutine(&splashRoutines[splashScreenState])
		{
			static int i;

			for (i = 0; i < 10; ++i)
			{
				float t = i / 10.0f;
				Rectangle src = { 0, 0, (float)chocoroTexture.width, (float)chocoroTexture.height };
				Rectangle dst = { 97, 127, (float)chocoroTexture.width, (float)chocoroTexture.height };
				dst = ExpandRectanglePro(dst, 0, 0, -t * (float)dst.height / 2, 0);
				DrawTexturePro(chocoroTexture, src, dst, (Vector2) { 0 }, 0, WHITE);
				DrawLogo(1);
				yield();
			}
			for (i = 0; i < 40; ++i)
			{
				float t = i / 40.0f;
				Vector2 start = { 101, 131 };
				Vector2 control1 = { 101, 104 };
				Vector2 control2 = { 111, 98 };
				Vector2 end = { 132, 109 };
				Vector2 position = CubicBezierPoint(start, control1, control2, end, t);
				if (i == 0)
				{
					for (int j = 0; j < countof(chocoro.trail.items); ++j)
						chocoro.trail.items[i] = position;
				}
				BeginScissorMode(0, 0, 129, 256);
				{
					RingAdd(&chocoro.trail, position);
					DrawChocoroTrail();
					DrawTextureCenteredV(chocoroTexture, position, WHITE);
				}
				EndScissorMode();
				DrawLogo(1 - Smoothstep01(t));
				yield();
			}
			for (i = 0; i < 50; ++i)
			{
				float t = i / 50.0f;
				Vector2 center = { 132, 109 };
				BeginScissorMode(0, 0, 129, 256);
				{
					RingAdd(&chocoro.trail, center);
					DrawChocoroTrail();
					DrawChocoroTrail();
				}
				EndScissorMode();
				DrawShutter(center, Smoothstep01(t));
				yield();
			}
			levelIndex = 0;
			SerializeLevel(LOAD);
			void Playing_Render();
			for (i = 0; i < 50; ++i)
			{
				float t = i / 50.0f;
				Playing_Render();
				DrawShutter(chocoro.position, 1 - Smoothstep01(t));
				yield();
			}
			Playing_Render();
			gameState = GAME_STATE_Playing;
			framesPlaying = 0;
		}
	}
}

// Playing

void Playing_Init(void)
{
	StopAllSounds();
	mouseHoldFrames = 0;
}
void Playing_Update(void)
{
	UpdateEntranceAndExitPassable();

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) or (mouseHoldFrames and IsMouseButtonDown(MOUSE_BUTTON_LEFT)))
		++mouseHoldFrames;
	else
		mouseHoldFrames = 0;

	if (IsKeyPressed(KEY_SPACE) or IsKeyPressed(KEY_ESCAPE))
	{
		gameState = GAME_STATE_Paused;
		return;
	}
	if (IsKeyPressed(KEY_GRAVE) or IsKeyPressed(KEY_SLASH))
	{
		gameState = GAME_STATE_Editor;
		return;
	}
	if (IsKeyPressed(KEY_R) or mouseHoldFrames >= RESTART_HOLD_FRAMES + 15)
	{
		gameState = GAME_STATE_Restart;
		return;
	}

	if (chocoro.freezeFrames <= 0)
	{
		++framesPlaying;
		UpdateChocoro();
		UpdateBalloons();
		UpdateGasers();
		UpdateGunners();
		UpdateBottles();
		UpdateClouds();
		UpdateGas();
		UpdateChocoCamera();
	}
	chocoro.freezeFrames = MaxInt(0, chocoro.freezeFrames - 1);

	if (IsMusicStreamPlaying(zapSound))
		UpdateMusicStream(zapSound);

	if (not CheckCollisionCircleRec(chocoro.position, 0.5f, SCREEN_RECT))
		gameState = GAME_STATE_LevelTransition;
}
void Playing_Render(void)
{
	BeginChocoCamera();
	{
		DrawOutOfViewTiles();
		DrawBackgroundTiles();
		DrawClouds();
		DrawChocoroTrail();
		DrawDashLines();
		DrawForegroundTiles();
		DrawGasers();
		DrawGunners();
		DrawLasers();
		DrawBalloons();
		DrawDashIndicator();
		DrawChocoro();
		DrawBottles();
		DrawGas();
		DrawRestartIndicator();
	}
	EndChocoCamera();
}

// Paused

void Paused_Init(void)
{

}
void Paused_Update(void)
{
	if (IsKeyPressed(KEY_SPACE) or IsKeyPressed(KEY_ESCAPE))
	{
		gameState = GAME_STATE_Playing;
		return;
	}
	if (IsKeyPressed(KEY_R) or mouseHoldFrames >= RESTART_HOLD_FRAMES + 15)
	{
		gameState = GAME_STATE_Restart;
		return;
	}
}
void Paused_Render(void)
{
	BeginChocoCamera();
	{
		DrawBackgroundTiles();
		DrawClouds();
		DrawChocoroTrail();
		DrawDashLines();
		DrawForegroundTiles();
		DrawGasers();
		DrawGunners();
		DrawLasers();
		DrawBalloons();
		//DrawDashIndicator();
		DrawChocoro();
		DrawBottles();
		DrawGas();
		DrawRestartIndicator();

		DrawRectangle(0, 0, SCREEN_SIZE, SCREEN_SIZE, ColorAlpha(BLACK, 0.5f));
		DrawTextCenteredOffset(Fonts.game, "Paused", 0.5f * SCREEN_SIZE, 0.5f * SCREEN_SIZE - 20, 2 * (float)Fonts.game.baseSize, ORANGE, BLACK);
	}
	EndChocoCamera();
}

// Game over

void GameOver_Init(void)
{
	chocoro.zapFrames = false;
	chocoro.zapReady = false;
	StopMusicStream(zapSound);
}
void GameOver_Update(void)
{
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) or (mouseHoldFrames and IsMouseButtonDown(MOUSE_BUTTON_LEFT)))
		++mouseHoldFrames;
	else
		mouseHoldFrames = 0;

	if (IsKeyPressed(KEY_R) or mouseHoldFrames >= RESTART_HOLD_FRAMES - FPS)
	{
		gameState = GAME_STATE_Restart;
		return;
	}

	UpdateBalloons();
	UpdateGasers();
	UpdateGunners();
	UpdateBottles();
	UpdateClouds();
	UpdateGas();
	UpdateChocoCamera();
}
void GameOver_Render(void)
{
	BeginChocoCamera();
	{
		DrawOutOfViewTiles();
		DrawBackgroundTiles();
		DrawClouds();
		//DrawChocoroTrail();
		DrawDashLines();
		DrawForegroundTiles();
		DrawGasers();
		DrawGunners();
		DrawLasers();
		DrawBalloons();
		//DrawDashIndicator();
		DrawChocoro();
		DrawBottles();
		DrawGas();
	}
	EndChocoCamera();

	float centerX = 0.5f * SCREEN_SIZE;
	float centerY = 0.5f * SCREEN_SIZE;
	float offset = -20;
	DrawRectangle(0, 0, SCREEN_SIZE, SCREEN_SIZE, ColorAlpha(BLACK, 0.5f));
	DrawTextCenteredOffset(Fonts.game, "You're dead", centerX, centerY + offset, 2 * (float)Fonts.game.baseSize, ORANGE, BLACK);

	int frame = gameStateFrame;
	if (frame >= 30)
	{
		float t = Smoothstep01((frame - 30) / 60.0f);
		DrawTextCenteredOffset(Fonts.raylib, deathLine1, centerX, centerY + offset + 30, (float)Fonts.raylib.baseSize, ColorAlpha(Darker(ORANGE), t), ColorAlpha(BLACK, t));
	}
	if (frame >= 90)
	{
		float t = Smoothstep01((frame - 90) / 60.0f);
		DrawTextCenteredOffset(Fonts.raylib, deathLine2, centerX, centerY + offset + 45, (float)Fonts.raylib.baseSize, ColorAlpha(Darker(ORANGE), t), ColorAlpha(BLACK, t));
	}
	if (frame >= 150)
	{
		float t = Smoothstep01((frame - 150) / 60.0f);
		DrawTexture(mouseHoldTexture, SCREEN_CENTER - 38, SCREEN_CENTER + (int)offset + 80, ColorAlpha(ORANGE, t));
		DrawTextCenteredOffset(Fonts.raylib, "Restart", centerX + 18, centerY + offset + 88, (float)Fonts.raylib.baseSize, ColorAlpha(ORANGE, t), ColorAlpha(BLACK, t));
	}
	
	DrawRestartIndicator();
}

// Level transition

void LevelTransition_Init(void)
{
	StopAllSounds();
}
void LevelTransition_Update(void)
{
	if (gameStateFrame == LEVEL_TRANSITION_DURATION_FRAMES / 2)
	{
		float closestDistance = INFINITY;
		Tile closestEntranceOrExit = TILE_EXIT;
		for (int y = 0; y < NUM_TILES; ++y)
		{
			for (int x = 0; x < NUM_TILES; ++x)
			{
				Tile tile = tiles[y][x];
				if (tile == TILE_ENTRANCE or tile == TILE_EXIT)
				{
					float distance = Vector2DistanceSqr(chocoro.position, (Vector2) { (float)x * TILE_SIZE, (float)y * TILE_SIZE });
					if (distance < closestDistance)
					{
						closestDistance = distance;
						closestEntranceOrExit = tile;
					}
				}
			}
		}

		if (closestEntranceOrExit == TILE_ENTRANCE)
			--levelIndex;
		else
			++levelIndex;

		if (levelIndex < 0 or levelIndex >= numLevels)
		{
			levelIndex = 0;
			gameState = GAME_STATE_Credits;
			if (framesPlaying < bestFramesPlaying)
				SaveIniInt("BestTimeInFrames", framesPlaying);
			return;
		}

		Vector2 position = chocoro.position;
		Vector2 velocity = chocoro.velocity;
		if (position.y <= 0)
			velocity.y = fminf(velocity.y, -5);
		bool dashReady = chocoro.dashReady;
		int zapFrames = chocoro.zapFrames;
		SerializeLevel(LOAD);
		chocoro.position.x = Wrap(position.x, 0, SCREEN_SIZE);
		chocoro.position.y = Wrap(position.y, 0, SCREEN_SIZE);
		chocoro.velocity = velocity;
		chocoro.dashReady = dashReady;
		chocoro.zapFrames = zapFrames;
		TileIsPassable[TILE_ENTRANCE] = true;
		TileIsPassable[TILE_EXIT] = true;
	}
	if (gameStateFrame >= LEVEL_TRANSITION_DURATION_FRAMES)
	{
		gameState = GAME_STATE_Playing;
	}
}
void LevelTransition_Render(void)
{
	Playing_Render();

	float alpha = 0;
	int halfway = RESTART_DURATION_FRAMES / 2;
	if (gameStateFrame < halfway)
		alpha = (float)gameStateFrame / halfway;
	else
		alpha = 1 - (float)(gameStateFrame - halfway) / halfway;

	DrawRectangle(0, 0, SCREEN_SIZE, SCREEN_SIZE, ColorAlpha(BLACK, alpha));
}

// Restart

void Restart_Init(void)
{
	mouseHoldFrames = 0;
}
void Restart_Update(void)
{
	if (gameStateFrame == RESTART_DURATION_FRAMES / 2)
	{
		StopAllSounds();
		SerializeLevel(LOAD);
	}
	if (gameStateFrame >= RESTART_DURATION_FRAMES)
		gameState = GAME_STATE_Playing;
}
void Restart_Render(void)
{
	int halfway = RESTART_DURATION_FRAMES / 2;
	if (gameStateFrame <= halfway)
	{
		float time = (float)gameStateFrame / halfway;
		float t = Smoothstep01(time);
		if (previousGameState == GAME_STATE_Paused)
			Paused_Render();
		else if (previousGameState == GAME_STATE_GameOver)
			GameOver_Render();
		else
			Playing_Render();
		DrawShutter(chocoro.position, t);
	}
	else
	{
		float time = (float)(gameStateFrame - halfway) / halfway;
		float t = Smoothstep01(1 - time);
		Playing_Render();
		DrawShutter(chocoro.position, t);
	}
}

// Credits

int creditsState;
int creditsClickCount;
Coroutine creditsRoutines[3];
void Credits_Init(void)
{
	creditsState = 0;
	creditsClickCount = 0;
	ZeroArray(creditsRoutines);
}
void Credits_Update(void)
{
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		if (creditsState == 0)
		{
			if (++creditsClickCount >= 2)
				creditsState = 1;
		}
		else if (creditsState == 1)
			creditsState = 2;
	}
}
void Credits_Render(void)
{
	static char *const madeBy[] = {
		"Blat Blatnik:programming",
		"Olga Wazny:art",
	};
	static char *const musicBy[] = {
		"10270511@Pixabay:Computer Love",
	};
	static char *const soundsBy[] = {
		"muel2002@freesound:pop3",
		"edsward@freesound:Ping",
		"ddragonpearl@freesound:hit 1",
		"magnuswalker@freesound:Flame Burst",
		"pengo_au@freesound:steam_burst",
		"ejfortin@freesound:4 laser bursts",
		"InspectorJ@freesound:Ballons, Popping, 02-01",
		"Lukeo135@freesound:Slime",
		"Lukeo135@freesound:Slime 3",
		"Breviceps@freesound:Trampoline / Jump",
		"AlexMurphy53@freesound:Footsteps - Carpet",
		"Wakerone@freesound:Electric Arc Sparks",
		"mazk1985@freesound:laser_active_big",
		"LegoLunatic@freesound:Charged laser",
		"dasebr@freesound:Breaking a glass bottle",
		"wobesound@freesound:PoisonGasRelease",
		"BlueDelta@freesound:Heavy Thunder Strike",
		"straget@freesound:Thunder",
	};
	static char *const fontsBy[] = {
		"FontEndDev:Alkhemikal",
		"Christian Robertson:Roboto",
	};
	static char *const librariesBy[] = {
		"rxi@github:MicroUI",
		"Ramon Santamaria:Raylib",
	};

	static float backgroundAlpha;

	ClearBackground(BLACK);
	DrawScrollingDots(gameStateFrame / 60.0f, backgroundAlpha);
	Color orangeHighlight = ScaleBrightness(ORANGE, -0.8f);

	if (creditsState == 0)
	{
		if (creditsClickCount == 1)
		{
			DrawTexture(mouseClickTexture, SCREEN_CENTER - 13, 2, WHITE);
			DrawTextCenteredOffset(Fonts.raylib, "Skip", SCREEN_CENTER + 9, 10, (float)Fonts.raylib.baseSize, WHITE, BLACK);
			//DrawTextCenteredOffset(Fonts.raylib, "Click to skip", SCREEN_CENTER, SCREEN_SIZE - 10, (float)Fonts.raylib.baseSize, WHITE, BLACK);
			//DrawText("Click to skip", 0, 0, Fonts.raylib.baseSize, WHITE);
		}

		coroutine(&creditsRoutines[creditsState])
		{
			static int i;
			for (i = 0; i < 40; ++i)
			{
				float t = (i + 1) / 40.0f;
				backgroundAlpha = Smoothstep01(t);
				yield();
			}
			for (i = 0; i < 40; ++i)
			{
				float t = i / 40.0f;
				float alpha = Smoothstep01(t);
				DrawTextCenteredOffset(Fonts.game, "The end", SCREEN_CENTER, SCREEN_CENTER - 50, 2 * (float)Fonts.game.baseSize,
					ColorAlpha(ORANGE, alpha),
					ColorAlpha(orangeHighlight, alpha));
				yield();
			}
			for (i = 0; i < 10; ++i)
			{
				DrawTextCenteredOffset(Fonts.game, "The end", SCREEN_CENTER, SCREEN_CENTER - 50, 2 * (float)Fonts.game.baseSize,
					ORANGE,
					orangeHighlight);
				yield();
			}
			for (i = 0; i < 40; ++i)
			{
				float t = i / 40.0f;
				float alpha = Smoothstep01(t);
				DrawTextCenteredOffset(Fonts.game, "The end", SCREEN_CENTER, SCREEN_CENTER - 50, 2 * (float)Fonts.game.baseSize,
					ORANGE,
					orangeHighlight);
				DrawTextCenteredOffset(Fonts.game, "(for now)", SCREEN_CENTER, SCREEN_CENTER - 30, (float)Fonts.game.baseSize,
					ColorAlpha(Darker(ORANGE), alpha),
					ColorAlpha(Darker(orangeHighlight), alpha));
				yield();
			}
			for (i = 0; i < 60; ++i)
			{
				DrawTextCenteredOffset(Fonts.game, "The end", SCREEN_CENTER, SCREEN_CENTER - 50, 2 * (float)Fonts.game.baseSize,
					ORANGE,
					orangeHighlight);
				DrawTextCenteredOffset(Fonts.game, "(for now)", SCREEN_CENTER, SCREEN_CENTER - 30, (float)Fonts.game.baseSize,
					Darker(ORANGE),
					Darker(orangeHighlight));
				yield();
			}
			for (i = 0; i < 40; ++i)
			{
				float t = i / 40.0f;
				float alpha = 1 - Smoothstep01(t);
				DrawTextCenteredOffset(Fonts.game, "The end", SCREEN_CENTER, SCREEN_CENTER - 50, 2 * (float)Fonts.game.baseSize,
					ColorAlpha(ORANGE, alpha),
					ColorAlpha(orangeHighlight, alpha));
				DrawTextCenteredOffset(Fonts.game, "(for now)", SCREEN_CENTER, SCREEN_CENTER - 30, (float)Fonts.game.baseSize,
					ColorAlpha(Darker(ORANGE), alpha),
					ColorAlpha(Darker(orangeHighlight), alpha));
				yield();
			}

			// Made by
			{
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Made by =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Made by =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(madeBy); ++j)
						DrawCreditsLine(madeBy[j], SCREEN_CENTER + j * 20, alpha, 40);
					yield();
				}
				for (i = 0; i < 90; ++i)
				{
					DrawTextCenteredOffset(Fonts.game, "= Made by =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(madeBy); ++j)
						DrawCreditsLine(madeBy[j], SCREEN_CENTER + j * 20, 1, 40);
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = 1 - Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Made by =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					for (int j = 0; j < countof(madeBy); ++j)
						DrawCreditsLine(madeBy[j], SCREEN_CENTER + j * 20, alpha, 40);
					yield();
				}
			}

			// Music
			{
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Music =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Music =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(musicBy); ++j)
						DrawCreditsLine(musicBy[j], SCREEN_CENTER + j * 20, alpha, 40);
					yield();
				}
				for (i = 0; i < 90; ++i)
				{
					DrawTextCenteredOffset(Fonts.game, "= Music =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(musicBy); ++j)
						DrawCreditsLine(musicBy[j], SCREEN_CENTER + j * 20, 1, 40);
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = 1 - Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Music =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					for (int j = 0; j < countof(musicBy); ++j)
						DrawCreditsLine(musicBy[j], SCREEN_CENTER + j * 20, alpha, 40);
					yield();
				}
			}

			// Sounds
			{
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Sounds =", SCREEN_CENTER, 30, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Sounds =", SCREEN_CENTER, 30, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(soundsBy) / 2; ++j)
						DrawCreditsLine(soundsBy[j], 50 + j * 20, alpha, 10);
					yield();
				}
				for (i = 0; i < 190; ++i)
				{
					DrawTextCenteredOffset(Fonts.game, "= Sounds =", SCREEN_CENTER, 30, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(soundsBy) / 2; ++j)
						DrawCreditsLine(soundsBy[j], 50 + j * 20, 1, 10);
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = 1 - Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Sounds =", SCREEN_CENTER, 30, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(soundsBy) / 2; ++j)
						DrawCreditsLine(soundsBy[j], 50 + j * 20, alpha, 10);
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Sounds =", SCREEN_CENTER, 30, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = countof(soundsBy) / 2; j < countof(soundsBy); ++j)
						DrawCreditsLine(soundsBy[j], 50 + (j - countof(soundsBy) / 2) * 20, alpha, 10);
					yield();
				}
				for (i = 0; i < 190; ++i)
				{
					DrawTextCenteredOffset(Fonts.game, "= Sounds =", SCREEN_CENTER, 30, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = countof(soundsBy) / 2; j < countof(soundsBy); ++j)
						DrawCreditsLine(soundsBy[j], 50 + (j - countof(soundsBy) / 2) * 20, 1, 10);
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = 1 - Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Sounds =", SCREEN_CENTER, 30, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					for (int j = countof(soundsBy) / 2; j < countof(soundsBy); ++j)
						DrawCreditsLine(soundsBy[j], 50 + (j - countof(soundsBy) / 2) * 20, alpha, 10);
					yield();
				}
			}

			// Fonts
			{
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Fonts =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Fonts =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(fontsBy); ++j)
						DrawCreditsLine(fontsBy[j], SCREEN_CENTER + j * 20, alpha, 40);
					yield();
				}
				for (i = 0; i < 90; ++i)
				{
					DrawTextCenteredOffset(Fonts.game, "= Fonts =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(fontsBy); ++j)
						DrawCreditsLine(fontsBy[j], SCREEN_CENTER + j * 20, 1, 40);
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = 1 - Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Fonts =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					for (int j = 0; j < countof(fontsBy); ++j)
						DrawCreditsLine(fontsBy[j], SCREEN_CENTER + j * 20, alpha, 40);
					yield();
				}
			}

			// Libraries
			{
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Libraries =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Libraries =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(librariesBy); ++j)
						DrawCreditsLine(librariesBy[j], SCREEN_CENTER + j * 20, alpha, 40);
					yield();
				}
				for (i = 0; i < 90; ++i)
				{
					DrawTextCenteredOffset(Fonts.game, "= Libraries =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ORANGE,
						orangeHighlight);
					for (int j = 0; j < countof(librariesBy); ++j)
						DrawCreditsLine(librariesBy[j], SCREEN_CENTER + j * 20, 1, 40);
					yield();
				}
				for (i = 0; i < 30; ++i)
				{
					float t = i / 30.0f;
					float alpha = 1 - Smoothstep01(t);
					DrawTextCenteredOffset(Fonts.game, "= Libraries =", SCREEN_CENTER, SCREEN_CENTER - 50, (float)Fonts.game.baseSize,
						ColorAlpha(ORANGE, alpha),
						ColorAlpha(orangeHighlight, alpha));
					for (int j = 0; j < countof(librariesBy); ++j)
						DrawCreditsLine(librariesBy[j], SCREEN_CENTER + j * 20, alpha, 40);
					yield();
				}
			}

			for (i = 0; i < 40; ++i)
			{
				float t = i / 40.0f;
				float alpha = Smoothstep01(t);
				DrawTextCenteredOffset(Fonts.game, "Thanks for playing!", SCREEN_CENTER, SCREEN_CENTER - 50, 2 * (float)Fonts.game.baseSize,
					ColorAlpha(ORANGE, alpha),
					ColorAlpha(orangeHighlight, alpha));
				yield();
			}
			for (i = 0; i < 60; ++i)
			{
				DrawTextCenteredOffset(Fonts.game, "Thanks for playing!", SCREEN_CENTER, SCREEN_CENTER - 50, 2 * (float)Fonts.game.baseSize,
					ORANGE,
					orangeHighlight);
				yield();
			}
			for (i = 0; i < 40; ++i)
			{
				float t = (i + 1) / 40.0f;
				float alpha = 1 - Smoothstep01(t);
				DrawTextCenteredOffset(Fonts.game, "Thanks for playing!", SCREEN_CENTER, SCREEN_CENTER - 50, 2 * (float)Fonts.game.baseSize,
					ColorAlpha(ORANGE, alpha),
					ColorAlpha(orangeHighlight, alpha));
				yield();
			}
			for (i = 0; i < 30; ++i)
			{
				yield();
			}
		}

		creditsState = 1;
	}

	bool isBestTime = framesPlaying < bestFramesPlaying;
	char *timeText = String("Time %s", TimeStringFromFrames(framesPlaying));
	char *bestText = isBestTime ? "New best!" : String("Best %s", TimeStringFromFrames(bestFramesPlaying));

	Rectangle scoreboardRect = RectangleFromCenter(SCREEN_CENTER, SCREEN_CENTER, 100, 60);
	//Color scoreboardColor = ScaleBrightness(ORANGE, -0.8f);
	Color scoreboardColor = BLACK;
	Color scoreboardBorderColor = ORANGE;
	float scoreboardRoundness = 0.2f;
	Vector2 scoreboardCenter = RectangleCenter(scoreboardRect);
	Vector2 timePos = { scoreboardCenter.x, scoreboardCenter.y - 10 };
	Vector2 bestPos = { scoreboardCenter.x, scoreboardCenter.y + 10 };

	if (creditsState == 1)
	{
		backgroundAlpha = 1;
		coroutine(&creditsRoutines[creditsState])
		{
			static int i;

			for (i = 0; i < 10; ++i)
			{
				float t = i / 10.0f;
				float alpha = Smoothstep01(t);
				DrawRectangleRounded(ExpandRectangle(scoreboardRect, 1), scoreboardRoundness, 10, ColorAlpha(scoreboardColor, alpha));
				DrawRectangleRoundedLines(scoreboardRect, scoreboardRoundness, 10, 1, ColorAlpha(scoreboardBorderColor, alpha));
				yield();
			}
			for (i = 0; i < 30; ++i)
			{
				float t = i / 30.0f;
				float alpha = Smoothstep01(t);
				DrawRectangleRounded(ExpandRectangle(scoreboardRect, 1), scoreboardRoundness, 10, scoreboardColor);
				DrawRectangleRoundedLines(scoreboardRect, scoreboardRoundness, 10, 1, scoreboardBorderColor);
				DrawTextCenteredV(Fonts.raylib, timeText, timePos, (float)Fonts.raylib.baseSize, ColorAlpha(WHITE, alpha));
				DrawTextCenteredV(Fonts.raylib, bestText, bestPos, (float)Fonts.raylib.baseSize, ColorAlpha(WHITE, alpha));
				yield();
			}
			for (i = 0;; ++i)
			{
				int frame = i % 30;
				float alpha = isBestTime ? 1.0f - Sawtooth(frame / 30.0f) : 1.0f;
				DrawRectangleRounded(ExpandRectangle(scoreboardRect, 1), scoreboardRoundness, 10, scoreboardColor);
				DrawRectangleRoundedLines(scoreboardRect, scoreboardRoundness, 10, 1, scoreboardBorderColor);
				DrawTextCenteredV(Fonts.raylib, timeText, timePos, (float)Fonts.raylib.baseSize, WHITE);
				DrawTextCenteredV(Fonts.raylib, bestText, bestPos, (float)Fonts.raylib.baseSize, ColorAlpha(WHITE, 0.5f + 0.5f * alpha));
				yield();
			}
		}
	}

	if (creditsState == 2)
	{
		coroutine(&creditsRoutines[creditsState])
		{
			static int i = 0;
			for (i = 0; i < 60; ++i)
			{
				float t = i / 60.0f;
				float alpha = 1 - Smoothstep01(t);
				DrawRectangleRounded(ExpandRectangle(scoreboardRect, 1), scoreboardRoundness, 10, ColorAlpha(scoreboardColor, alpha));
				DrawRectangleRoundedLines(scoreboardRect, scoreboardRoundness, 10, 1, ColorAlpha(scoreboardBorderColor, alpha));
				DrawTextCenteredV(Fonts.raylib, timeText, timePos, (float)Fonts.raylib.baseSize, ColorAlpha(WHITE, alpha));
				DrawTextCenteredV(Fonts.raylib, bestText, bestPos, (float)Fonts.raylib.baseSize, ColorAlpha(WHITE, alpha));
				backgroundAlpha = alpha;
				yield();
			}
			for (i = 0; i < 30; ++i)
			{
				yield();
			}
		}
	}

	gameState = GAME_STATE_Splash;
}

// Editor

void Editor_Init(void)
{
	SerializeLevel(LOAD);
	StopAllSounds();
}
void Editor_Update(void)
{
	if (IsKeyPressed(KEY_GRAVE) or IsKeyPressed(KEY_SLASH))
	{
		EditorDeselect();
		SerializeLevel(LOAD);
		gameState = GAME_STATE_Playing;
		return;
	}
	if (IsKeyPressed(KEY_ESCAPE) or IsKeyPressed(KEY_D))
		EditorDeselect();
	if (editorSelectedTile != -1)
	{
		if (GetMouseWheelMove() > 0)
			editorTileBrushRadius += 0.5f;
		else if (GetMouseWheelMove() < 0)
			editorTileBrushRadius -= 0.5f;
		editorTileBrushRadius = Clamp(editorTileBrushRadius, 0, NUM_TILES);
	}

	if ((editorSelectedBalloon or editorSelectedGaser or editorSelectedGunner or editorSelectedChocoro) and IsMouseButtonUp(MOUSE_BUTTON_LEFT))
		EditorDeselect();

	if (IsKeyPressed(KEY_F))
		editorGridAlign = GRID_ALIGN_FREEFORM;
	if (IsKeyPressed(KEY_G))
		editorGridAlign = GRID_ALIGN_FULL;
	if (IsKeyPressed(KEY_H))
		editorGridAlign = GRID_ALIGN_HALF;

	if (IsKeyPressed(KEY_PERIOD) or IsKeyPressed(KEY_COMMA))
	{
		if (editorSelectedBalloon or editorSelectedGaser or editorSelectedGunner or editorSelectedChocoro)
			EditorDeselect();
		if (IsKeyPressed(KEY_PERIOD))
			++levelIndex;
		if (IsKeyPressed(KEY_COMMA))
			--levelIndex;
		SerializeLevel(LOAD);
	}

	for (Tile tile = 0; tile < TILE_ENUM_COUNT; ++tile)
	{
		if (IsKeyPressed(KEY_ONE + tile))
		{
			int base = tile * MAX_TILE_VARIANTS;
			int selected = editorSelectedTile;
			EditorDeselect();
			if (selected - base >= 0 and selected - base < TileNumVariants[tile])
				editorSelectedTile = WrapInt(selected + 1, base, base + TileNumVariants[tile]);
			else
				editorSelectedTile = base;
		}
	}
	if (Input.mouseIsInGameWindow and IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
	{
		int x = Input.mouseX / TILE_SIZE;
		int y = Input.mouseY / TILE_SIZE;
		EditorDeselect();
		Tile tile = tiles[y][x];
		int variant = tileVariants[y][x];
		editorSelectedTile = tile * MAX_TILE_VARIANTS + variant;
	}

	float multiple =
		(editorGridAlign == GRID_ALIGN_FULL) ? 1.0f :
		(editorGridAlign == GRID_ALIGN_HALF) ? 0.5f :
		0.01f;

	if (editorSelectedBalloon)
		editorSelectedBalloon->position = RoundToTileEx(Vector2Add(Input.mouseUnclampedPosition, editorDragOffset), multiple);
	if (editorSelectedGaser)
		editorSelectedGaser->position = RoundToTileEx(Vector2Add(Input.mouseUnclampedPosition, editorDragOffset), multiple);
	if (editorSelectedGunner)
		editorSelectedGunner->position = RoundToTileEx(Vector2Add(Input.mouseUnclampedPosition, editorDragOffset), multiple);
	if (editorSelectedChocoro)
		editorSelectedChocoro->position = RoundToTileEx(Vector2Add(Input.mouseUnclampedPosition, editorDragOffset), multiple);

	if (editorSelectedTile != -1 and Input.mouseIsInGameWindow and (IsMouseButtonDown(MOUSE_BUTTON_LEFT) or IsMouseButtonDown(MOUSE_BUTTON_RIGHT)))
	{
		Tile tile = 0;
		int variant = 0;
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
		{
			tile = editorSelectedTile / MAX_TILE_VARIANTS;
			variant = editorSelectedTile % MAX_TILE_VARIANTS;
		}
		
		editorSaveNeeded = true;
		if (IsKeyDown(KEY_F) and IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		{
			int x0 = (int)((Input.mousePosition.x) / TILE_SIZE);
			int y0 = (int)((Input.mousePosition.y) / TILE_SIZE);
			Tile targetTile = tiles[y0][x0];
			int targetVariant = tileVariants[y0][x0];
			if (tile != targetTile or variant != targetVariant)
			{
				List(int, 4 * NUM_TILES * NUM_TILES) *stack = Allocate(1, sizeof * stack);
				ListAdd(stack, y0 * NUM_TILES + x0);
				while (stack->count > 0)
				{
					int index = ListPop(stack);
					int x = index % NUM_TILES;
					int y = index / NUM_TILES;
					if (x >= 0 and x < NUM_TILES and y >= 0 and y < NUM_TILES and tiles[y][x] == targetTile and tileVariants[y][x] == targetVariant)
					{
						tiles[y][x] = tile;
						tileVariants[y][x] = variant;
						int l = ClampInt(x - 1, 0, NUM_TILES - 1);
						int r = ClampInt(x + 1, 0, NUM_TILES - 1);
						int u = ClampInt(y - 1, 0, NUM_TILES - 1);
						int d = ClampInt(y + 1, 0, NUM_TILES - 1);
						if (tiles[y][l] == targetTile and tileVariants[y][l] == targetVariant) ListAdd(stack, y * NUM_TILES + l);
						if (tiles[y][r] == targetTile and tileVariants[y][r] == targetVariant) ListAdd(stack, y * NUM_TILES + r);
						if (tiles[u][x] == targetTile and tileVariants[u][x] == targetVariant) ListAdd(stack, u * NUM_TILES + x);
						if (tiles[d][x] == targetTile and tileVariants[d][x] == targetVariant) ListAdd(stack, d * NUM_TILES + x);
					}
				}
			}
		}
		else
		{
			int x0 = (int)((Input.mousePosition.x) / TILE_SIZE);
			int y0 = (int)((Input.mousePosition.y) / TILE_SIZE);
			int x1 = (int)((Input.mousePosition.x - Input.mouseDelta.x) / TILE_SIZE);
			int y1 = (int)((Input.mousePosition.y - Input.mouseDelta.y) / TILE_SIZE);

			// https://stackoverflow.com/a/12934943
			x0 = ClampInt(x0, 0, NUM_TILES - 1);
			y0 = ClampInt(y0, 0, NUM_TILES - 1);
			x1 = ClampInt(x1, 0, NUM_TILES - 1);
			y1 = ClampInt(y1, 0, NUM_TILES - 1);
			int dx = +abs(x1 - x0);
			int dy = -abs(y1 - y0);
			int sx = x0 < x1 ? +1 : -1;
			int sy = y0 < y1 ? +1 : -1;

			int x = x0;
			int y = y0;
			int error = dx + dy;
			for (;;)
			{
				int rx0 = (int)(x - editorTileBrushRadius);
				int ry0 = (int)(y - editorTileBrushRadius);
				int rx1 = (int)(x + editorTileBrushRadius);
				int ry1 = (int)(y + editorTileBrushRadius);
				FillTilesRect(rx0, ry0, rx1, ry1, tile, variant);
				if (x == x1 and y == y1)
					return;

				if (2 * error > dy)
				{
					error += dy;
					x += sx;
				}
				else if (2 * error < dx)
				{
					error += dx;
					y += sy;
				}
			}
		}
	}

	Balloon *balloonUnderCursor = NULL;
	for (int i = 0; i < balloons.count; ++i)
	{
		Balloon *balloon = &balloons.items[i];
		if (CheckCollisionPointCircle(Input.mouseUnclampedPosition, balloon->position, BALLOON_RADIUS))
			balloonUnderCursor = balloon;
	}

	Gaser *gaserUnderCursor = NULL;
	for (int i = 0; i < gasers.count; ++i)
	{
		Gaser *gaser = &gasers.items[i];
		Rectangle rect = RectangleFromCenterV(gaser->position, (Vector2) { GASER_WIDTH, GASER_HEIGHT });
		if (CheckCollisionPointRec(Input.mouseUnclampedPosition, rect))
			gaserUnderCursor = gaser;
	}

	Gunner *gunnerUnderCursor = NULL;
	for (int i = 0; i < gunners.count; ++i)
	{
		Gunner *gunner = &gunners.items[i];
		Rectangle rect = RectangleFromCenterV(gunner->position, (Vector2) { GUNNER_WIDTH, GUNNER_HEIGHT });
		if (CheckCollisionPointRec(Input.mouseUnclampedPosition, rect))
			gunnerUnderCursor = gunner;
	}

	Chocoro *chokoroUnderCursor = NULL;
	if (CheckCollisionPointCircle(Input.mouseUnclampedPosition, chocoro.position, CHOCORO_RADIUS))
		chokoroUnderCursor = &chocoro;

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		if (chokoroUnderCursor)
		{
			editorSelectedChocoro = chokoroUnderCursor;
			editorDragOffset = Vector2Subtract(chokoroUnderCursor->position, Input.mouseUnclampedPosition);
		}
		else if (gaserUnderCursor)
		{
			editorSelectedGaser = gaserUnderCursor;
			editorDragOffset = Vector2Subtract(gaserUnderCursor->position, Input.mouseUnclampedPosition);
		}
		else if (gunnerUnderCursor)
		{
			editorSelectedGunner = gunnerUnderCursor;
			editorDragOffset = Vector2Subtract(editorSelectedGunner->position, Input.mouseUnclampedPosition);
		}
		else if (balloonUnderCursor)
		{
			editorSelectedBalloon = balloonUnderCursor;
			editorDragOffset = Vector2Subtract(editorSelectedBalloon->position, Input.mouseUnclampedPosition);
		}
	}

	if (IsKeyPressed(KEY_DELETE) or IsKeyPressed(KEY_BACKSPACE))
	{
		if (editorSelectedBalloon or balloonUnderCursor)
		{
			Balloon *balloon = editorSelectedBalloon ? editorSelectedBalloon : balloonUnderCursor;
			int index = (int)(balloon - &balloons.items[0]);
			ListSwapRemove(&balloons, index);
			editorSelectedBalloon = NULL;
			editorSaveNeeded = true;
		}
		else if (editorSelectedGaser or gaserUnderCursor)
		{
			Gaser *gaser = editorSelectedGaser ? editorSelectedGaser : gaserUnderCursor;
			int index = (int)(gaser - &gasers.items[0]);
			ListSwapRemove(&gasers, index);
			editorSelectedGaser = NULL;
			editorSaveNeeded = true;
		}
		else if (editorSelectedGunner or gunnerUnderCursor)
		{
			Gunner *gunner = editorSelectedGunner ? editorSelectedGunner : gunnerUnderCursor;
			int index = (int)(gunner - &gunners.items[0]);
			ListSwapRemove(&gunners, index);
			editorSelectedGunner = NULL;
			editorSaveNeeded = true;
		}
	}

	if (editorSaveNeeded)
	{
		if (levelIndex == numLevels)
			++numLevels;
		editorSaveNeeded = false;
		SerializeLevel(SAVE);
	}
}
void Editor_Render(void)
{
	BeginChocoCamera();
	{
		DrawBackgroundTiles();
		//DrawClouds();
		//DrawChocoroTrail();
		//DrawDashLines();
		DrawForegroundTiles();
		DrawGasers();
		DrawGunners();
		//DrawLasers();
		DrawBalloons();
		//DrawDashIndicator();
		DrawChocoro();
		//DrawBottles();
		//DrawGas();
	}
	EndChocoCamera();

	if (editorSelectedTile != -1 and Input.mouseIsInGameWindow)
	{
		Color color = ColorAlpha(YELLOW, 0.05f + 0.05f * SineLerp((float)(3 * Time.now)));
		int x0 = (int)((Input.mousePosition.x) / TILE_SIZE);
		int y0 = (int)((Input.mousePosition.y) / TILE_SIZE);

		if (IsKeyDown(KEY_F))
		{
			Tile tile = tiles[y0][x0];
			int variant = tileVariants[y0][x0];
			bool visited[NUM_TILES][NUM_TILES] = { false };
			List(int, 4 * NUM_TILES * NUM_TILES) *stack = Allocate(1, sizeof * stack);
			ListAdd(stack, y0 * NUM_TILES + x0);
			while (stack->count > 0)
			{
				int index = ListPop(stack);
				int x = index % NUM_TILES;
				int y = index / NUM_TILES;
				if (not visited[y][x])
					DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, color);
				visited[y][x] = true;
				if (x >= 0 and x < NUM_TILES and y >= 0 and y < NUM_TILES and tiles[y][x] == tile and tileVariants[y][x] == variant)
				{
					int l = ClampInt(x - 1, 0, NUM_TILES - 1);
					int r = ClampInt(x + 1, 0, NUM_TILES - 1);
					int u = ClampInt(y - 1, 0, NUM_TILES - 1);
					int d = ClampInt(y + 1, 0, NUM_TILES - 1);
					if (tiles[y][l] == tile and tileVariants[y][l] == variant and not visited[y][l]) ListAdd(stack, y * NUM_TILES + l);
					if (tiles[y][r] == tile and tileVariants[y][r] == variant and not visited[y][r]) ListAdd(stack, y * NUM_TILES + r);
					if (tiles[u][x] == tile and tileVariants[u][x] == variant and not visited[u][x]) ListAdd(stack, u * NUM_TILES + x);
					if (tiles[d][x] == tile and tileVariants[d][x] == variant and not visited[d][x]) ListAdd(stack, d * NUM_TILES + x);
				}
			}
		}
		else
		{
			float rx0 = TILE_SIZE * (float)ClampInt((int)(x0 - editorTileBrushRadius), 0, NUM_TILES - 1);
			float ry0 = TILE_SIZE * (float)ClampInt((int)(y0 - editorTileBrushRadius), 0, NUM_TILES - 1);
			float rx1 = TILE_SIZE * (float)ClampInt((int)(x0 + editorTileBrushRadius) + 1, 0, NUM_TILES);
			float ry1 = TILE_SIZE * (float)ClampInt((int)(y0 + editorTileBrushRadius) + 1, 0, NUM_TILES);
			Rectangle rect = { rx0, ry0, rx1 - rx0, ry1 - ry0 };
			DrawRectangleRec(rect, color);
		}
	}
}

// Runtime

void Initialize(void)
{
	for (int i = 0;; ++i)
	{
		if (not CanSerializeLoadFrom(String("level%d.bin", i)))
			break;
		++numLevels;
	}

	SetAudioStreamBufferSizeDefault(2 * 48000 * sizeof(float)); // 2 seconds worth of audio. Needed for the web otherwise it sounds terrible..

	chocoroTexture = LoadTexture("chocoro.png");
	chocoroZappedTexture = LoadTexture("chocoro-zapped.png");
	chocoroExhaustedTexture = LoadTexture("chocoro-exhausted.png");
	chocoroDeadTexture = LoadTexture("chocoro-dead.png");
	zapTexture = LoadTexture("zap.png");
	rlTextureParameters(zapTexture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
	rlTextureParameters(zapTexture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_REPEAT);
	balloonTexture = LoadTexture("balloon.png");
	gaserTexture = LoadTexture("gaser.png");
	bottleTexture = LoadTexture("bottle.png");
	gunnerIdleTexture = LoadTexture("gunner-idle.png");
	gunnerAimingTexture = LoadTexture("gunner-aiming.png");
	gunTexture = LoadTexture("gun.png");
	beamTexture = LoadTexture("beam.png");
	rlTextureParameters(beamTexture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
	rlTextureParameters(beamTexture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_REPEAT);
	beamStartTexture = LoadTexture("beam-start.png");
	lightningTexture0 = LoadTexture("lightning0.png");
	lightningTexture1 = LoadTexture("lightning1.png");
	lightningTexture2 = LoadTexture("lightning2.png");
	castleTexture = LoadTexture("castle.png");
	mouseClickTexture = LoadTexture("mouse-click.png");
	mouseHoldTexture = LoadTexture("mouse-hold.png");
	for (Tile tile = 0; tile < TILE_ENUM_COUNT; ++tile)
	{
		for (int i = 0; i < MAX_TILE_VARIANTS; ++i)
		{
			char *filename = String("%s%d.png", TileNames[tile], i);
			if (not FileExists(filename))
				break;
	
			TileTextures[tile][TileNumVariants[tile]++] = LoadTexture(filename);
		}
	}
	
	pingSound = LoadSound("ping.wav");
	popSound = LoadSound("pop.wav");
	dashSound = LoadSound("dash.wav");
	slimeSounds[0] = LoadSound("slime0.wav");
	SetSoundVolume(slimeSounds[0], 2.0f);
	SetSoundPitch(slimeSounds[0], 1.3f);
	slimeSounds[1] = LoadSound("slime1.wav");
	SetSoundVolume(slimeSounds[1], 2.0f);
	SetSoundPitch(slimeSounds[1], 1.3f);
	trampolineSound = LoadSound("trampoline.wav");
	wallSound = LoadSound("wall.wav");
	zapSound = LoadMusicStream("zap.wav");
	zapSound.looping = true;
	laserChargeSound = LoadSound("laser-charge.wav");
	laserBeamSound = LoadSound("laser-beam.wav");
	bottleSound = LoadSound("bottle.wav");
	gasSound = LoadSound("gas.wav");
	SetSoundVolume(gasSound, 0.5f);
	hitSound = LoadSound("hit.wav");
	SetSoundVolume(hitSound, 0.5f);
	ghostSound = LoadSound("ghost.wav");
	music = LoadMusicStream("computer-love.mp3");
	music.looping = true;
	SetMusicVolume(music, 0.3f);
	thunder0Sound = LoadSound("thunder0.wav");
	thunder1Sound = LoadSound("thunder1.wav");
	SetSoundVolume(thunder1Sound, 0.5f);

	//DisableCursor();

	if (Debug.on)
	{
		SerializeLevel(LOAD);
		gameState = GAME_STATE_Playing;
	}
	else
		gameState = GAME_STATE_Splash;
}
void Update(void)
{
	#define GAMESTATE(name) [GAME_STATE_##name] = { name##_Init, name##_Update, name##_Render }
	static const struct { void (*init)(void), (*update)(void), (*render)(void); } Functions[GAME_STATE_ENUM_COUNT] = {
		GAMESTATE(Splash),
		GAMESTATE(Playing),
		GAMESTATE(Restart),
		GAMESTATE(Paused),
		GAMESTATE(GameOver),
		GAMESTATE(LevelTransition),
		GAMESTATE(Credits),
		GAMESTATE(Editor),
	};

	if (IsKeyPressed(KEY_F3))
		break();

	if (IsMusicStreamPlaying(music))
		UpdateMusicStream(music);

	static GameState lastState = -1;
	ClearBackground(MAGENTA);
	if (gameState != lastState)
	{
		gameStateFrame = 0;
		previousGameState = lastState;
		lastState = gameState;
		Functions[gameState].init();
	}
	Functions[gameState].update();
	++gameStateFrame;

	if (gameState != lastState)
	{
		gameStateFrame = 0;
		previousGameState = lastState;
		lastState = gameState;
		Functions[gameState].init();
	}
	Functions[gameState].render();
}
void UpdateUi(void)
{
	if (gameState != GAME_STATE_Editor)
		return;

	int screenWidth = GetScreenWidth();
	int screenHeight = GetScreenHeight();
	UiConsole((Rectangle) { 0, 0, (float)screenWidth, 200 });

	if (UiBeginWindow("Toolbox", (Rectangle) { 0, 200, 400, (float)(screenHeight - 200) }))
	{
		UiLayout(30, 30, 30, 30);
		UiRadio("F", &editorGridAlign, GRID_ALIGN_FREEFORM);
		UiTooltip("Freeform positioning.");
		UiRadio("G", &editorGridAlign, GRID_ALIGN_FULL);
		UiTooltip("Align to grid.");
		UiRadio("H", &editorGridAlign, GRID_ALIGN_HALF);
		UiTooltip("Align to half grid.");

		int columnWidths[MAX_TILE_VARIANTS];
		for (int i = 0; i < MAX_TILE_VARIANTS; ++i)
			columnWidths[i] = 50;

		int windowWidth = UiWindowWidth();
		int maxItemsPerRow = ClampInt((windowWidth - 20) / 50, 1, MAX_TILE_VARIANTS);
		for (Tile tile = 0; tile < TILE_ENUM_COUNT; ++tile)
		{
			char *name = (char *)TextToPascal(TileNames[tile]);
			if (UiHeader(name))
			{
				int numVariants = TileNumVariants[tile];
				UiLayoutEx(50, columnWidths, MinInt(maxItemsPerRow, numVariants));
				for (int variant = 0; variant < numVariants; ++variant)
				{
					int index = tile * MAX_TILE_VARIANTS + variant;
					UiTextureRadio(&editorSelectedTile, index, TileTextures[tile][variant]);
					UiTooltip("%s %d", name, variant);
				}
			}
		}

		if (UiHeader("Enemies"))
		{
			UiLayoutEx(50, columnWidths, MinInt(maxItemsPerRow, 3));

			if (UiTextureButton("Balloon", balloonTexture))
			{
				EditorDeselect();
				Balloon *balloon = ListAllocate(&balloons);
				if (balloon)
				{
					ZeroStruct(balloon);
					balloon->position = Input.mouseUnclampedPosition;
					editorSelectedBalloon = balloon;
				}
			}
			UiTooltip("Balloon");

			if (UiTextureButton("Gaser", gaserTexture))
			{
				EditorDeselect();
				Gaser *gaser = ListAllocate(&gasers);
				if (gaser)
				{
					ZeroStruct(gaser);
					gaser->position = Input.mouseUnclampedPosition;
					editorSelectedGaser = gaser;
				}
			}
			UiTooltip("Gaser");

			if (UiTextureButton("Gunner", gunnerIdleTexture))
			{
				EditorDeselect();
				Gunner *gunner = ListAllocate(&gunners);
				if (gunner)
				{
					ZeroStruct(gunner);
					gunner->position = Input.mouseUnclampedPosition;
					editorSelectedGunner = gunner;
				}
			}
			UiTooltip("Gunner");
		}

		UiEndWindow();
	}
	if (UiBeginWindow("Properties", (Rectangle) { (float)(screenWidth - 400), 200, 400, (float)(screenHeight - 200) }))
	{
		if (editorSelectedBalloon)
		{
			int index = (int)(editorSelectedBalloon - &balloons.items[0]);
			UiLayout(0, 80);
			UiLabel("Balloon %d", index);

			UiLayout(0, 80, 200);
			UiLabel("Position");
			UiSliderVector2(&editorSelectedBalloon->position, 0, SCREEN_SIZE);
		}
		else if (editorSelectedGaser)
		{
			int index = (int)(editorSelectedGaser - &gasers.items[0]);
			UiLayout(0, 80);
			UiLabel("Gaser %d", index);

			UiLayout(0, 80, 200);
			UiLabel("Position");
			UiSliderVector2(&editorSelectedGaser->position, 0, SCREEN_SIZE);
		}
		else if (editorSelectedGunner)
		{
			int index = (int)(editorSelectedGunner - &gunners.items[0]);
			UiLayout(0, 80);
			UiLabel("Gunner %d", index);

			UiLayout(0, 80, 200);
			UiLabel("Position");
			UiSliderVector2(&editorSelectedGunner->position, 0, SCREEN_SIZE);
		}
		else if (editorSelectedChocoro)
		{
			UiLayout(0, 80);
			UiLabel("Chocoro");
		
			UiLayout(0, 80, 200);
			UiLabel("Position");
			UiSliderVector2(&editorSelectedChocoro->position, 0, SCREEN_SIZE);
		}
		else
		{
			UiLayout(0, 50, 200);

			UiLabel("Level");
			UiTooltip("Change the level number using the slider.");
			if (UiSliderInt(&levelIndex, 0, numLevels))
				SerializeLevel(LOAD);

			UiLayout(0, 125, 125);
			if (UiButton("Stop music"))
				StopMusicStream(music);
			if (UiButton("Play music"))
				PlayMusicStream(music);
		}

		UiEndWindow();
	}
}
