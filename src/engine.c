#include "engine.h"
#include "microui.h"

#ifdef _WIN32
#define CP_UTF8 65001
#define CSIDL_APPDATA 26
#define SHGFP_TYPE_CURRENT 0
#define CLSCTX_ALL 23
#define FOS_PICKFOLDERS 0x20
#define FOS_PATHMUSTEXIST 0x800
#define FOS_FILEMUSTEXIST 0x1000
#define SIGDN_FILESYSPATH 0x80058000
typedef struct GUID {
	uint32_t a;
	uint16_t b;
	uint16_t c;
	uint8_t d[8];
} GUID;
typedef struct IShellItem { struct IShellItemVtbl *vtable; } IShellItem;
typedef struct IShellItemVtbl {
	int(__stdcall *QueryInterface)(IShellItem *This, const GUID *riid, void **ppvObject);
	unsigned(__stdcall *AddRef)(IShellItem *This);
	unsigned(__stdcall *Release)(IShellItem *This);
	int(__stdcall *BindToHandler)(IShellItem *This, void *pbc, GUID *bhid, GUID *riid, void **ppv);
	int(__stdcall *GetParent)(IShellItem *This, IShellItem **ppsi);
	int(__stdcall *GetDisplayName)(IShellItem *This, int sigdnName, wchar_t **ppszName);
	int(__stdcall *GetAttributes)(IShellItem *This, unsigned sfgaoMask, unsigned *psfgaoAttribs);
	int(__stdcall *Compare)(IShellItem *This, IShellItem *psi, unsigned hint, int *piOrder);
} IShellItemVtbl;
typedef struct IFileDialog { struct IFileDialogVtbl *vtable; } IFileDialog;
typedef struct IFileDialogVtbl {
	int(__stdcall *QueryInterface)(IFileDialog *This, const GUID *riid, void **ppvObject);
	unsigned(__stdcall *AddRef)(IFileDialog *This);
	unsigned(__stdcall *Release)(IFileDialog *This);
	int(__stdcall *Show)(IFileDialog *This, void *hwndOwner);
	int(__stdcall *SetFileTypes)(IFileDialog *This, unsigned cFileTypes, void *rgFilterSpec);
	int(__stdcall *SetFileTypeIndex)(IFileDialog *This, unsigned iFileType);
	int(__stdcall *GetFileTypeIndex)(IFileDialog *This, unsigned *piFileType);
	int(__stdcall *Advise)(IFileDialog *This, void *pfde, unsigned *pdwCookie);
	int(__stdcall *Unadvise)(IFileDialog *This, unsigned dwCookie);
	int(__stdcall *SetOptions)(IFileDialog *This, unsigned fos);
	int(__stdcall *GetOptions)(IFileDialog *This, unsigned *pfos);
	int(__stdcall *SetDefaultFolder)(IFileDialog *This, IShellItem *psi);
	int(__stdcall *SetFolder)(IFileDialog *This, IShellItem *psi);
	int(__stdcall *GetFolder)(IFileDialog *This, IShellItem **ppsi);
	int(__stdcall *GetCurrentSelection)(IFileDialog *This, IShellItem **ppsi);
	int(__stdcall *SetFileName)(IFileDialog *This, wchar_t *pszName);
	int(__stdcall *GetFileName)(IFileDialog *This, wchar_t **pszName);
	int(__stdcall *SetTitle)(IFileDialog *This, wchar_t *pszTitle);
	int(__stdcall *SetOkButtonLabel)(IFileDialog *This, wchar_t *pszText);
	int(__stdcall *SetFileNameLabel)(IFileDialog *This, wchar_t *pszLabel);
	int(__stdcall *GetResult)(IFileDialog *This, IShellItem **ppsi);
	int(__stdcall *AddPlace)(IFileDialog *This, IShellItem *psi, int fdap);
	int(__stdcall *SetDefaultExtension)(IFileDialog *This, wchar_t *pszDefaultExtension);
	int(__stdcall *Close)(IFileDialog *This, int hr);
	int(__stdcall *SetClientGuid)(IFileDialog *This, const GUID *guid);
	int(__stdcall *ClearClientData)(IFileDialog *This);
	int(__stdcall *SetFilter)(IFileDialog *This, void *pFilter);
} IFileDialogVtbl;
__declspec(dllimport) void __stdcall OutputDebugStringA(char *output);
__declspec(dllimport) int __stdcall CoCreateInstance(const GUID *rclsid, void *pUnkOuter, unsigned dwClsContext, const GUID *riid, void **ppv);
__declspec(dllimport) int __stdcall IsDebuggerPresent(void);
__declspec(dllimport) int __stdcall SHGetFolderPathW(void *hwnd, int csidl, void *token, unsigned flags, wchar_t path[260]);
__declspec(dllimport) int __stdcall CreateDirectoryW(wchar_t *path, void *securityAttributes);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned codepage, unsigned flags, wchar_t *stringW, int length, char *string, int capacity, char *defaultChar, int *usedDefaultChar);
static const GUID CLSID_FileOpenDialog = { 0xdc1c5a9c, 0xe88a, 0x4dde, 0xa5, 0xa1, 0x60, 0xf8, 0x2a, 0x20, 0xae, 0xf7 };
static const GUID IID_IFileDialog = { 0x42f85136, 0xdb7e, 0x439c, 0x85, 0xf1, 0xe4, 0x07, 0x5d, 0x13, 0x5f, 0xc8 };
#endif

// Compatibility

#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#endif
#ifdef _WIN32
// Run on a dedicated GPU if both a dedicated and integrated one are avaliable.
// See: https://stackoverflow.com/a/39047129
__declspec(dllexport) int NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

// Constants

#define KB 1024
#define MB 1048576
#define MAX_INI_ENTRIES 200
#define MAX_LOG_ENTRIES 500
#define FRAME_ALLOCATOR_CAPACITY (1 * MB)
#define UI_FONT_SIZE 16

// Datatypes

typedef struct IniEntry {
	char key[64];
	char value[256];
} IniEntry;
typedef struct LogEntry {
	TraceLogLevel level;
	char message[512];
} LogEntry;
typedef struct Command {
	char *syntax;
	bool(*callback)(char *args[], int count);
} Command;
typedef struct ParsedCommand {
	char *name;
	char *help;
	List(char *, 10) args;
	int count;
} ParsedCommand;

// Globals

static struct {
	char savePath[512];
	bool needsSync; // For emscripten: set this flag to do a FS.syncfs at the end of the frame.
	bool syncAlreadyRunning; // For emscripten: this flag is set during a syncfs. 
} Filesystem;
static struct {
	List(IniEntry, MAX_INI_ENTRIES) entries;
	bool wasModified;
} Ini;
static struct {
	int cursor;
} Random;
static struct {
	char memory[FRAME_ALLOCATOR_CAPACITY];
	int cursor;
} Allocator;
static struct {
	Ring(LogEntry, MAX_LOG_ENTRIES) log;
	List(Command, 100) commands;
	char *failureReason;
} Console;
static struct {
	mu_Context context;
} Ui;

struct Debug Debug;
struct Time Time;
struct Input Input;
struct Fonts Fonts;
struct Coroutines Coroutines;

// Bytes

void ZeroBytes(void *bytes, int count)
{
	memset(bytes, 0, (size_t)count);
}
bool BytesAreZero(void *bytes, int count)
{
	char *x = bytes;
	for (int i = 0; i < count; ++i)
		if (x[i])
			return false;
	return true;
}
bool BytesEqual(void *a, void *b, int count)
{
	return memcmp(a, b, (size_t)count) == 0;
}
void SwapBytes(void *a, void *b, int count)
{
	char *A = a;
	char *B = b;
	for (int i = 0; i < count; ++i)
	{
		char temp = A[i];
		A[i] = B[i];
		B[i] = temp;
	}
}

// String

int CopyStringEx(char *to, int capacity, char *from)
{
	int cursor;
	for (cursor = 0; cursor < capacity - 1 and from[cursor]; ++cursor)
		to[cursor] = from[cursor];
	if (capacity > 0)
		to[cursor] = 0;
	return cursor;
}
bool StringsEqual(char *a, char *b)
{
	return strcmp(a, b) == 0;
}
char *Slice(char *string, int start, int end)
{
	int length = (int)strlen(string);
	start = ClampInt((start < 0 ? length + start : start), 0, length - 1);
	end = ClampInt((end < 0 ? length + end : end), 0, length - 1);
	return SlicePointer(string + start, string + end + 1);
}
char *SlicePointer(char *start, char *endExclusive)
{
	int chars = (int)(endExclusive - start) + 1;
	char *slice = AllocateNonZero(chars + 1, 1);
	CopyStringEx(slice, chars, start);
	slice[chars] = 0;
	return slice;
}
char *String(_Printf_format_string_ char *format, ...)
{
	va_list args;
	va_start(args, format);
	char *result = StringArgs(format, args);
	va_end(args);
	return result;
}
char *StringArgs(char *format, va_list args)
{
	char *buffer = Allocator.memory + Allocator.cursor;
	int capacity = sizeof Allocator.memory - Allocator.cursor;
	int chars = vsnprintf(buffer, (size_t)capacity, format, args);
	int needed = chars + 1;
	WarnIf(needed > capacity);
	Allocator.cursor += MinInt(needed, capacity);
	return buffer;
}
char *SkipWhitespace(char *string)
{
	while (isspace(*string))
		++string;
	return string;
}

// Math

float Sign(float x)
{
	return (float)((x > 0) - (x < 0));
}
float Clamp01(float x)
{
	return Clamp(x, 0, 1);
}
float Wrap01(float x)
{
	return Wrap(x, 0, 1);
}
float WrapAngle(float angle)
{
	return Wrap(angle, 0, 2 * PI);
}
float FlipAngle(float angle)
{
	return Wrap(angle + PI, 0, 2 * PI);
}
float Smoothstart01(float x)
{
	x = Clamp01(x);
	return x * x;
}
float Smoothstart(float x, float edge0, float edge1)
{
	return Smoothstart01((x - edge0) / (edge1 - edge0));
}
float Smoothstop01(float x)
{
	x = 1 - Clamp01(x);
	return 1 - x * x;
}
float Smoothstop(float x, float edge0, float edge1)
{
	return Smoothstop01((x - edge0) / (edge1 - edge0));
}
float Smoothstep01(float x)
{
	// https://en.wikipedia.org/wiki/Smoothstep
	x = Clamp01(x);
	return x * x * (3 - 2 * x);
}
float Smoothstep(float x, float edge0, float edge1)
{
	return Smoothstep01((x - edge0) / (edge1 - edge0));
}
float Smootherstep01(float x)
{
	// https://en.wikipedia.org/wiki/Smoothstep#Variations
	x = Clamp01(x);
	return x * x * x * (x * (x * 6 - 15) + 10);
}
float Smootherstep(float x, float edge0, float edge1)
{
	return Smootherstep01((x - edge0) / (edge1 - edge0));
}
float Smoothbounce01(float x)
{
	float t = Clamp01(x);
	float n = 1 - 2 * t;
	float p = 2 * t - 1;
	if (t <= 0.5f)
		return 0.5f - 0.5f * n * n;
	else
		return 0.5f + 0.5f * p * p;

	// Cubic
	//x = Clamp01(x);
	//x = 2 * x - 1;
	//return 0.5f + 0.5f * x * x * x;
}
float SineLerp(float t)
{
	float s = sinf(0.5f * PI * t);
	return s * s;
}
float Sawtooth(float x)
{
	x = 2 * Clamp01(x);
	return x < 1 ? (x) : (2 - x);
}
float PrevFloat(float x)
{
	return nextafterf(x, -INFINITY);
}
float NextFloat(float x)
{
	return nextafterf(x, +INFINITY);
}
float ShortestAngleDifference(float from, float to)
{
	return Wrap(to - from, -PI, +PI);
}
float Atan2Vector(Vector2 xy)
{
	return atan2f(xy.y, xy.x);
}
int IntSign(int x)
{
	return (x > 0) - (x < 0);
}
int MinInt(int a, int b)
{
	return a < b ? a : b;
}
int MaxInt(int a, int b)
{
	return a > b ? a : b;
}
int ClampInt(int x, int min, int max)
{
	x = MinInt(x, max);
	x = MaxInt(x, min);
	return x;
}
int WrapInt(int x, int min, int max)
{
	// https://stackoverflow.com/a/707426
	int range = max - min;
	if (x < min)
		x += range * (1 + (min - x) / range);
	return min + (x - min) % range;
}
int RemapInt(int x, int fromMin, int fromMax, int toMin, int toMax)
{
	long long n = x;
	long long l = fromMin;
	long long h = fromMax;
	long long L = toMin;
	long long H = toMax;
	long long r = L + (n - l) * (H - L) / (h - l);
	return (int)r;
}

// Vector

Vector2 RotateClockwise(Vector2 v)
{
	return (Vector2) { -v.y, +v.x };
}
Vector2 RotateCounterClockwise(Vector2 v)
{
	return (Vector2) { +v.y, -v.x };
}
Vector2 Vector2FromPolar(float length, float angle)
{
	float x = length * cosf(angle);
	float y = length * sinf(angle);
	return (Vector2) { x, y };
}
Vector2 UnitVector2WithAngle(float angle)
{
	return Vector2FromPolar(1, angle);
}
Vector2 Vector2Floor(Vector2 v)
{
	return (Vector2) { floorf(v.x), floorf(v.y) };
}
Vector2 Vector2Ceil(Vector2 v)
{
	return (Vector2) { ceilf(v.x), ceilf(v.y) };
}
Vector2 Vector2LerpAngle(Vector2 v1, Vector2 v2, float amount)
{
	float length1 = Vector2Length(v1);
	float length2 = Vector2Length(v2);
	float angle1 = Atan2Vector(v1);
	float angle2 = Atan2Vector(v2);
	float length = Lerp(length1, length2, amount);
	float angle = Lerp(angle1, angle2, amount);
	return Vector2FromPolar(length, angle);
}

// Rectangle

float RectangleRight(Rectangle rect)
{
	return rect.x + rect.width;
}
float RectangleBottom(Rectangle rect)
{
	return rect.y + rect.height;
}
Vector2 RectangleMin(Rectangle rect)
{
	return (Vector2) { rect.x, rect.y };
}
Vector2 RectangleMax(Rectangle rect)
{
	return RemapToRectangle(rect, 1, 1);
}
Vector2 RemapToRectangle(Rectangle rect, float u, float v)
{
	float rx = rect.x + u * rect.width;
	float ry = rect.y + v * rect.height;
	return (Vector2) { rx, ry };
}
Vector2 RemapToRectangleV(Rectangle rect, Vector2 uv)
{
	return RemapToRectangle(rect, uv.x, uv.y);
}
Vector2 RectangleTopLeft(Rectangle rect)
{
	return RemapToRectangle(rect, 0.0f, 0.0f);
}
Vector2 RectangleTopMiddle(Rectangle rect)
{
	return RemapToRectangle(rect, 0.5f, 0.0f);
}
Vector2 RectangleTopRight(Rectangle rect)
{
	return RemapToRectangle(rect, 1.0f, 0.0f);
}
Vector2 RectangleCenterLeft(Rectangle rect)
{
	return RemapToRectangle(rect, 0.0f, 0.5f);
}
Vector2 RectangleCenter(Rectangle rect)
{
	return RemapToRectangle(rect, 0.5f, 0.5f);
}
Vector2 RectangleCenterRight(Rectangle rect)
{
	return RemapToRectangle(rect, 1.0f, 0.5f);
}
Vector2 RectangleBottomLeft(Rectangle rect)
{
	return RemapToRectangle(rect, 0.0f, 1.0f);
}
Vector2 RectangleBottomMiddle(Rectangle rect)
{
	return RemapToRectangle(rect, 0.5f, 1.0f);
}
Vector2 RectangleBottomRight(Rectangle rect)
{
	return RemapToRectangle(rect, 1.0f, 1.0f);
}
Vector2 RectangleExtents(Rectangle rect)
{
	float x = 0.5f * rect.width;
	float y = 0.5f * rect.height;
	return (Vector2) { x, y };
}
Rectangle RectangleFromExtents(float centerX, float centerY, float extentX, float extentY)
{
	float width = 2 * extentX;
	float height = 2 * extentY;
	float x = centerX - extentX;
	float y = centerY - extentY;
	return (Rectangle) { x, y, width, height };
}
Rectangle RectangleFromExtentsV(Vector2 center, Vector2 extent)
{
	return RectangleFromExtents(center.x, center.y, extent.x, extent.y);
}
Rectangle RectangleFromCenter(float centerX, float centerY, float width, float height)
{
	float extentX = 0.5f * width;
	float extentY = 0.5f * height;
	float x = centerX - extentX;
	float y = centerY - extentY;
	return (Rectangle) { x, y, width, height };
}
Rectangle RectangleFromCenterV(Vector2 center, Vector2 size)
{
	return RectangleFromCenter(center.x, center.y, size.x, size.y);
}
Rectangle RectangleFromMinMax(float minX, float minY, float maxX, float maxY)
{
	float width = maxX - minX;
	float height = maxY - minY;
	return (Rectangle) { minX, minY, width, height };
}
Rectangle RectangleFromMinMaxV(Vector2 min, Vector2 max)
{
	return RectangleFromMinMax(min.x, min.y, max.x, max.y);
}
Rectangle ExpandRectangle(Rectangle rect, float amount)
{
	return ExpandRectanglePro(rect, amount, amount, amount, amount);
}
Rectangle ExpandRectangleEx(Rectangle rect, float horizontal, float vertical)
{
	return ExpandRectanglePro(rect, horizontal, horizontal, vertical, vertical);
}
Rectangle ExpandRectanglePro(Rectangle rect, float left, float right, float top, float bottom)
{
	Rectangle result = {
		rect.x - left,
		rect.y - top,
		fmaxf(0, rect.width + left + right),
		fmaxf(0, rect.height + top + bottom),
	};
	return result;
}
Rectangle MoveRectangle(Rectangle rect, float dx, float dy)
{
	rect.x += dx;
	rect.y += dy;
	return rect;
}

// Collision

bool CheckCollisionCircleRotatedRect(Vector2 position, float radius, Rectangle rect, Vector2 origin, float angle)
{
	float x = position.x - rect.x;
	float y = position.y - rect.y;
	float theta = -angle * DEG2RAD;
	Vector2 center = Vector2Rotate((Vector2) { x, y }, theta);
	center.x += origin.x;
	center.y += origin.y;
	return CheckCollisionCircleRec(center, radius, (Rectangle) { 0, 0, rect.width, rect.height });
}
bool ResolveCollisionPointRect(Vector2 *position, Rectangle rect)
{
	// http://noonat.github.io/intersect/#aabb-vs-point

	float rw = 0.5f * rect.width;
	float rx = rect.x + rw;
	float dx = position->x - rx;
	float px = rw - fabsf(dx);
	if (px <= 0)
		return false;

	float rh = 0.5f * rect.height;
	float ry = rect.y + rh;
	float dy = position->y - ry;
	float py = rh - fabsf(dy);
	if (py <= 0)
		return false;

	if (px < py)
		position->x = rx + rw * Sign(dx);
	else
		position->y = ry + rh * Sign(dy);
	return true;
}
bool ResolveCollisionCircleRect(Vector2 *position, float radius, Rectangle rect)
{
	rect = ExpandRectangle(rect, radius);
	return ResolveCollisionPointRect(position, rect);
}
Sweep SweepPointRect(Vector2 position, Vector2 velocity, Rectangle rect)
{
	Sweep sweep = { INFINITY };
	float rw = 0.5f * rect.width;
	float rh = 0.5f * rect.height;
	float rx = rect.x + rw;
	float ry = rect.y + rh;

	if (velocity.x == 0 and velocity.y == 0)
	{
		// http://noonat.github.io/intersect/#aabb-vs-point
		float dx = position.x - rx;
		float dy = position.y - ry;
		float px = rw - fabsf(dx);
		float py = rh - fabsf(dy);
		if (px <= 0 or py <= 0)
			return sweep;

		sweep.t = 0;
		if (px < py)
			sweep.normal.x = Sign(dx);
		else
			sweep.normal.y = Sign(dy);
		return sweep;
	}

	// http://noonat.github.io/intersect/#aabb-vs-segment
	float paddingX = 0;
	float paddingY = 0;
	float scaleX = 1.0f / velocity.x;
	float scaleY = 1.0f / velocity.y;
	float signX = Sign(scaleX);
	float signY = Sign(scaleY);
	float nearTimeX = (rx - signX * (rw + paddingX) - position.x) * scaleX;
	float nearTimeY = (ry - signY * (rh + paddingY) - position.y) * scaleY;
	float farTimeX = (rx + signX * (rw + paddingX) - position.x) * scaleX;
	float farTimeY = (ry + signY * (rh + paddingY) - position.y) * scaleY;

	if (nearTimeX > farTimeY or nearTimeY > farTimeX)
		return sweep;

	float nearTime = fmaxf(nearTimeX, nearTimeY);
	float farTime = fminf(farTimeX, farTimeY);

	if (nearTime >= 1 or farTime <= 0)
		return sweep;

	sweep.t = Clamp01(nearTime);
	if (nearTimeX > nearTimeY)
	{
		sweep.normal.x = -signX;
		sweep.normal.y = 0;
	}
	else
	{
		sweep.normal.x = 0;
		sweep.normal.y = -signY;
	}
	return sweep;

	// This just didn't work out. Too many hacks, and I still kept getting random physics explosions.
	//Ray ray = { { position.x, position.y }, { velocity.x, velocity.y } };
	//BoundingBox box = { { rect.x, rect.y, -1 }, { rect.x + rect.width, rect.y + rect.height, +1 } };
	//RayCollision collision = GetRayCollisionBox(ray, box);
	//
	//Sweep result = { INFINITY };
	//if (collision.hit and collision.distance >= 0 and collision.distance <= 1)
	//{
	//	result.t = collision.distance;
	//	result.normal.x = collision.normal.x;
	//	result.normal.y = collision.normal.y;
	//
	//	// HACK I think I've seen Raylib return an unnormalized collision normal, like [9999,-9999].
	//	//       So just in case...
	//	result.normal = Vector2Normalize(result.normal);
	//
	//	// HACK Sometimes we have a corner collision, and the normal points away from a corner.
	//	//       But the game assumes the normal only points out from the sides, so we have to make sure that's actually the case.
	//	if (result.normal.x != 0 and result.normal.y != 0)
	//	{
	//		float absX = fabsf(result.normal.x);
	//		float absY = fabsf(result.normal.y);
	//		float signX = Sign(result.normal.x);
	//		float signY = Sign(result.normal.y);
	//		if (absX > absY)
	//		{
	//			result.normal.x = signX;
	//			result.normal.y = 0;
	//		}
	//		else if (absX < absY)
	//		{
	//			result.normal.x = 0;
	//			result.normal.y = signY;
	//		}
	//		else
	//		{
	//			// HACK At the exact corner, you might get stuck if you always pick a vertical or horizontal normal.
	//			// I guess the best thing to do is just to pick a random one?
	//			result.normal.x = Time.frame % 2 ? signX : 0;
	//			result.normal.y = Time.frame % 2 ? 0 : signY;
	//		}
	//	}
	//}
	//return result;
}
Sweep SweepPointCircle(Vector2 position, Vector2 velocity, Vector2 center, float radius)
{
	Ray ray = { { position.x, position.y }, { velocity.x, velocity.y } };
	Vector3 sphereCenter = { center.x, center.y };
	RayCollision collision = GetRayCollisionSphere(ray, sphereCenter, radius);
	float t = collision.distance;

	Sweep result = { INFINITY };
	if (collision.hit and t >= 0 and t <= 1)
	{
		result.t = t;
		result.normal.x = collision.normal.x;
		result.normal.y = collision.normal.y;
	}
	return result;
}
Sweep SweepCircleRect(Vector2 position, Vector2 velocity, float radius, Rectangle rect)
{
	rect = ExpandRectangle(rect, radius);
	return SweepPointRect(position, velocity, rect);
}
Sweep SweepCircleCircle(Vector2 position, Vector2 velocity, float radius, Vector2 center, float circleRadius)
{
	return SweepPointCircle(position, velocity, center, circleRadius + radius);
}

// Color

Color ScaleBrightness(Color color, float amount)
{
	Color result = color;

	if (amount > 1.0f) amount = 1.0f;
	else if (amount < -1.0f) amount = -1.0f;

	float red = (float)color.r;
	float green = (float)color.g;
	float blue = (float)color.b;

	if (amount < 0.0f)
	{
		amount = 1.0f + amount;
		red *= amount;
		green *= amount;
		blue *= amount;
	}
	else
	{
		red = (255 - red) * amount + red;
		green = (255 - green) * amount + green;
		blue = (255 - blue) * amount + blue;
	}

	result.r = (unsigned char)red;
	result.g = (unsigned char)green;
	result.b = (unsigned char)blue;

	return result;
}
Color Brighter(Color color)
{
	return ScaleBrightness(color, +0.2f);
}
Color Darker(Color color)
{
	return ScaleBrightness(color, -0.2f);
}
Vector4 ColorToHsva(Color color)
{
	Vector3 hsv = ColorToHSV(color);
	float alpha = color.a / 255.5f;
	return (Vector4) { hsv.x, hsv.y, hsv.z, alpha };
}
Color ColorFromHSVA(Vector4 hsva)
{
	Color color = ColorFromHSV(hsva.x, hsva.y, hsva.z);
	color.a = (unsigned char)(hsva.w * 255.5f);
	return color;
}
Color Hue(float hue)
{
	return ColorFromHSV(hue, 1, 1);
}
Color Grayscale(float intensity)
{
	unsigned char g = (unsigned char)(255.5f * Clamp01(intensity));
	return (Color) { g, g, g, 255 };
}
Color GrayscaleAlpha(float intensity, float alpha)
{
	unsigned char g = (unsigned char)(255.5f * Clamp01(intensity));
	unsigned char a = (unsigned char)(255.5f * Clamp01(alpha));
	return (Color) { g, g, g, a };
}
float Luminance(Color color)
{
	// https://en.wikipedia.org/wiki/Grayscale#Luma_coding_in_video_systems
	float yr = color.r * 0.2126f / 255.0f;
	float yg = color.g * 0.7152f / 255.0f;
	float yb = color.b * 0.0722f / 255.0f;
	return Clamp01(yr + yg + yb);
}
Color ContrastingColor(Color color)
{
	float luma = Luminance(color);
	Vector3 hsv = ColorToHSV(color);

	float saturation = hsv.y;
	if (saturation <= 0.5f)
		return luma > 0.5f ? BLACK : WHITE;

	float hue = Wrap(hsv.x + 180, 0, 360);
	return ColorFromHSV(hue, 1, 1);
}
Color ContrastingColorBlackOrWhite(Color color)
{
	float luma = Luminance(color);
	return luma > 0.5f ? BLACK : WHITE;
}
Color ColorBlend(Color from, Color to, float amount)
{
	Color result;
	result.r = (unsigned char)Clamp(from.r + amount * (to.r - from.r), 0, 255.5f);
	result.g = (unsigned char)Clamp(from.g + amount * (to.g - from.g), 0, 255.5f);
	result.b = (unsigned char)Clamp(from.b + amount * (to.b - from.b), 0, 255.5f);
	result.a = (unsigned char)Clamp(from.a + amount * (to.a - from.a), 0, 255.5f);
	return result;
}
Color ColorAlphaMultiply(Color color, float alpha)
{
	color.a = (unsigned char)Clamp(color.a * alpha, 0, 255);
	return color;
}

// Noise

unsigned BitNoise3(int seed, int x, int y, int z)
{
	// https://youtu.be/LWFzPP8ZbdU?t=2800.
	// https://nullprogram.com/blog/2018/07/31/
	// http://jonkagstrom.com/tuning-bit-mixers/index.html
	// I randomly cobbled together the magic numbers for this bit mixer.
	// They generate random looking 2D perlin noise, which means they're good enough in my book.
	unsigned v = (unsigned)seed;
	v *= 0xB5297A4Du;
	v ^= v >> 16;
	v += (unsigned)x;
	v *= 0x68E31DA4u;
	v ^= v >> 16;
	v += (unsigned)y;
	v *= 0x1B56C4E9u;
	v ^= v >> 16;
	v += (unsigned)z;
	v *= 0x7FEB352Du;
	v ^= v >> 16;
	v *= 0x846CA68Bu;
	v ^= v >> 16;
	return v;
}
unsigned BitNoise2(int seed, int x, int y)
{
	return BitNoise3(seed, x, y, 0);
}
unsigned BitNoise1(int seed, int x)
{
	return BitNoise3(seed, x, 0, 0);
}
bool BoolNoise2(int seed, int x, int y)
{
	unsigned bits = BitNoise2(seed, x, y);
	return (bits & 1) != 0;
}
bool BoolNoise1(int seed, int x)
{
	return BoolNoise2(seed, x, 0);
}
int IntNoise2(int seed, int min, int max, int x, int y)
{
	unsigned bits1 = BitNoise3(seed, x, y, 0xAAAAAAAA);
	unsigned bits2 = BitNoise3(seed, x, y, 0x55555555);

	// https://github.com/apple/swift/pull/39143
	uint64_t range = (uint64_t)max - (uint64_t)min;
	uint64_t a = range * bits1;
	uint64_t b = range * bits2;
	b >>= 32;
	b += (uint32_t)a;
	a >>= 32;
	b >>= 32;

	uint32_t v = (uint32_t)min + (uint32_t)a + (uint32_t)b;
	return (int)v;
}
int IntNoise1(int seed, int min, int max, int x)
{
	return IntNoise2(seed, min, max, x, 0);
}
float FloatNoise2(int seed, float min, float max, int x, int y)
{
	unsigned bits = BitNoise2(seed, x, y);

	// https://prng.di.unimi.it/#remarks
	float v = (bits >> 8) * 0x1.0p-24f;
	return Clamp(min + v * (max - min), min, max);
}
float FloatNoise1(int seed, float min, float max, int x)
{
	return FloatNoise2(seed, min, max, x, 0);
}
float PerlinNoise2(int seed, float min, float max, float x, float y)
{
	int x0 = (int)floorf(x);
	int y0 = (int)floorf(y);
	int x1 = x0 + 1;
	int y1 = y0 + 1;

	float dx0 = x - (float)x0;
	float dx1 = x - (float)x1;
	float dy0 = y - (float)y0;
	float dy1 = y - (float)y1;

	Vector2 v00 = UnitVector2WithAngle(2 * PI * FloatNoise2(seed, 0, 1, x0, y0));
	Vector2 v01 = UnitVector2WithAngle(2 * PI * FloatNoise2(seed, 0, 1, x0, y1));
	Vector2 v10 = UnitVector2WithAngle(2 * PI * FloatNoise2(seed, 0, 1, x1, y0));
	Vector2 v11 = UnitVector2WithAngle(2 * PI * FloatNoise2(seed, 0, 1, x1, y1));

	float d00 = (dx0 * v00.x) + (dy0 * v00.y);
	float d01 = (dx0 * v01.x) + (dy1 * v01.y);
	float d10 = (dx1 * v10.x) + (dy0 * v10.y);
	float d11 = (dx1 * v11.x) + (dy1 * v11.y);

	float weightY = Smootherstep01(dy0);
	float weightX = Smootherstep01(dx0);

	float d0 = d00 + (d01 - d00) * weightY;
	float d1 = d10 + (d11 - d10) * weightY;
	float d = d0 + (d1 - d0) * weightX;

	float v = d * 0.5f + 0.5f;
	return Clamp(min + v * (max - min), min, max);
}
float PerlinNoise1(int seed, float min, float max, float x)
{
	int x0 = (int)floorf(x);
	int x1 = x0 + 1;

	float dx0 = x - (float)x0;
	float dx1 = x - (float)x1;

	float v0 = FloatNoise1(seed, -1, +1, x0);
	float v1 = FloatNoise1(seed, -1, +1, x1);

	float weightX = Smootherstep01(dx0);

	float d0 = dx0 * v0;
	float d1 = dx1 * v1;
	float d = d0 + (d1 - d0) * weightX;

	float v = d + 0.5f; // No idea why, but we get [-0.5, +0.5] instead of [-1, +1] 
	return Clamp(min + v * (max - min), min, max);
}

// Random

int RandomInt(int min, int max)
{
	return IntNoise1(42, min, max, Random.cursor++);
}
float RandomFloat(float min, float max)
{
	return FloatNoise1(42, min, max, Random.cursor++);
}
float RandomNormal(float mean, float sdev)
{
	// https://en.wikipedia.org/wiki/Marsaglia_polar_method
	float x, y, r;
	do
	{
		x = RandomFloat(-1, +1);
		y = RandomFloat(-1, +1);
		r = x * x + y * y;
	} while (r >= 1 or r == 0);
	float v = x * sqrtf(-2 * logf(r) / r);
	return mean + sdev * v;
}
Vector2 RandomVectorInCircle(float radius)
{
	float x, y, r;
	do
	{
		x = RandomFloat(-1, +1);
		y = RandomFloat(-1, +1);
		r = x * x + y * y;
	} while (r > 1);
	x *= radius;
	y *= radius;
	return (Vector2) { x, y };
}
bool RandomBool(float probability)
{
	return RandomFloat(0, 1) < probability;
}
int RandomSelect(float weights[], int count)
{
	float sum = 0;
	for (int i = 0; i < count; ++i)
		sum += weights[i];

	float x = RandomFloat(0, sum);
	sum = 0;
	for (int i = 0; i < count; ++i)
	{
		sum += weights[i];
		if (x < sum)
			return i;
	}

	// We can reach here due to floating point error. There's sometimes a little bit of leftover probability.
	// I think the best thing to do in that case is to just pick a random index.
	return RandomInt(0, count);
}
void RandomShuffleEx(void *items, int count, int size)
{
	// https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#The_modern_algorithm
	for (int i = 0; i < count - 1; ++i)
	{
		int j = RandomInt(i, count);
		SwapBytes((char *)items + i * size, (char *)items + j * size, size);
	}
}

// Bezier

Vector2 CubicBezierPoint(Vector2 start, Vector2 control1, Vector2 control2, Vector2 end, float t)
{
	t = Clamp01(t);
	float n = 1 - t;
	float x = (n * n * n) * start.x + (3 * n * n * t) * control1.x + (3 * n * t * t) * control2.x + (t * t * t) * end.x;
	float y = (n * n * n) * start.y + (3 * n * n * t) * control1.y + (3 * n * t * t) * control2.y + (t * t * t) * end.y;
	return (Vector2) { x, y };
}

// Allocator

void *Allocate(int count, int size)
{
	void *ptr = AllocateNonZero(count, size);
	ZeroBytes(ptr, count * size);
	return ptr;
}
void *AllocateNonZero(int count, int size)
{
	uintptr_t alignment = 8;
	uintptr_t mask = alignment - 1;
	uintptr_t top = (uintptr_t)(Allocator.memory + Allocator.cursor);
	uintptr_t aligned = (top + mask) & (~mask);

	Allocator.cursor += count * size + (int)(aligned - top);
	WarnIf(Allocator.cursor > sizeof Allocator.memory);

	return (void *)aligned;
}

// Ini

static void LoadIniFile(void)
{
	TraceLog(LOG_INFO, "Loading settings from ini file.");
	char *path = String("%s/settings.ini", Filesystem.savePath);
	FILE *file = fopen(path, "rt");
	if (not file)
		return;

	Ini.entries.count = 0;

	char line[1024];
	while (fgets(line, sizeof line, file))
	{
		if (line[0] == '[' or line[0] == '#')
			continue;

		char *separator = strchr(line, '=');
		if (not separator)
			continue;
		*separator = 0;

		struct IniEntry *entry = ListAllocate(&Ini.entries);
		if (not entry)
			break;

		char *key = line;
		char *value = separator + 1; // Note: the value has a newline at the end from fgets. We need to remove that.
		char *newline = strchr(value, '\n');
		if (newline)
			*newline = 0;

		CopyString(entry->key, key);
		CopyString(entry->value, value);
	}

	fclose(file);
}
static void SaveIniFile(void)
{
	if (not Ini.wasModified)
		return;

	char *path = String("%s/settings.ini", Filesystem.savePath);
	FILE *file = fopen(path, "wt");
	if (not file)
		return;

	fprintf(file, "[Settings]\n");
	for (int i = 0; i < Ini.entries.count; ++i)
		fprintf(file, "%s=%s\n", Ini.entries.items[i].key, Ini.entries.items[i].value);

	fclose(file);
	Ini.wasModified = false;
	Filesystem.needsSync = true;
}
static IniEntry *FindIniEntry(char *key)
{
	for (int i = 0; i < Ini.entries.count; ++i)
		if (StringsEqual(Ini.entries.items[i].key, key))
			return &Ini.entries.items[i];
	return NULL;
}

int LoadIniInt(char *key, int defaultValue)
{
	char *string = LoadIniString(key, "");
	char *end = string;
	long loaded = strtol(string, &end, 10);

	if (end == string)
	{
		SaveIniInt(key, defaultValue);
		return defaultValue;
	}
	if (loaded > INT_MAX)
		return INT_MAX;
	if (loaded < INT_MIN)
		return INT_MIN;
	return (int)loaded;
}
bool LoadIniBool(char *key, bool defaultValue)
{
	char *string = LoadIniString(key, "");
	if (StringsEqual(string, "1"))
		return true;
	if (StringsEqual(string, "0"))
		return false;
	return defaultValue;
}
float LoadIniFloat(char *key, float defaultValue)
{
	char *string = LoadIniString(key, "");
	char *end = string;
	float loaded = strtof(string, &end);

	if (end == string)
	{
		SaveIniFloat(key, defaultValue);
		return defaultValue;
	}
	return loaded;
}
char *LoadIniString(char *key, char *defaultValue)
{
	IniEntry *entry = FindIniEntry(key);
	if (not entry)
		return defaultValue;

	return entry->value;
}
void SaveIniInt(char *key, int value)
{
	char string[64];
	FormatString(string, "%d", value);
	SaveIniString(key, string);
}
void SaveIniBool(char *key, bool value)
{
	SaveIniString(key, value ? "1" : "0");
}
void SaveIniFloat(char *key, float value)
{
	char string[64];
	FormatString(string, "%g", value);
	SaveIniString(key, string);
}
void SaveIniString(char *key, char *value)
{
	IniEntry *entry = FindIniEntry(key);
	if (not entry)
	{
		entry = ListAllocate(&Ini.entries);
		WarnReturnIf(not entry);
	}

	if (not StringsEqual(entry->key, key) or not StringsEqual(entry->value, value))
	{
		CopyString(entry->key, key);
		CopyString(entry->value, value);
		Ini.wasModified = true;
	}
}

// Serialization

bool CanSerializeLoadFrom(char *path)
{
	char *resPath = String("%s", path);
	char *savePath = String("%s/%s", Filesystem.savePath, path);
	return FileExists(resPath) or FileExists(savePath);
}
FILE *BeginSerialize(SerialOp op, char *path)
{
	// If saving: first try "res/path", then "/save/path".
	// If loading: first try "/save/path", then "res/path".
	// UNLESS! *sigh* we are on the web, where we always first try /save/path and then /res/path
	char *resPath = String("%s", path);
	char *savePath = String("%s/%s", Filesystem.savePath, path);
	char *path1 = NULL;
	char *path2 = NULL;

	#ifdef __EMSCRIPTEN__
	{
		path1 = savePath;
		path2 = resPath;
	}
	#else
	{
		path1 = (op == SAVE) ? resPath : savePath;
		path2 = (op == SAVE) ? savePath : resPath;
	}
	#endif
	
	FILE *file = NULL;
	file = fopen(path1, op == SAVE ? "wb" : "rb");
	if (file)
		return file;
	file = fopen(path2, op == SAVE ? "wb" : "rb");
	if (file)
		return file;

	char *directionString = op == SAVE ? "SAVE" : "LOAD";
	char *reason = FileExists(path) ? " The file doesn't exist." : "";
	TraceLog(LOG_WARNING, "%s(%s, '%s') failed.%s", __func__, directionString, path, reason);
	return NULL;
}
void EndSerialize(SerialOp op, FILE **file)
{
	if (not *file)
		return;

	fclose(*file);
	*file = NULL;

	if (op == SAVE)
		Filesystem.needsSync = true;
}
bool SerializeBytes(SerialOp op, FILE *file, void *bytes, int count)
{
	if (op == SAVE)
	{
		if (not file)
			return false;

		int written = (int)fwrite(bytes, (size_t)count, 1, file);
		if (written)
			return true;

		Warn(not written);
		return false;
	}
	else if (op == LOAD)
	{
		if (not file)
		{
			ZeroBytes(bytes, count);
			return false;
		}

		int read = (int)fread(bytes, (size_t)count, 1, file);
		if (read)
			return true;

		Warn(not read);
		ZeroBytes(bytes, count);
		return false;
	}
	Warn(op);
	return true;
}

// Rendering

void DrawTextureCentered(Texture texture, int x, int y, Color tint)
{
	DrawTextureCenteredV(texture, (Vector2) { (float)x, (float)y }, tint);
}
void DrawTextureCenteredV(Texture texture, Vector2 center, Color tint)
{
	Vector2 pos = center;
	pos.x -= 0.5f * texture.width;
	pos.y -= 0.5f * texture.height;
	DrawTextureV(texture, pos, tint);
}
void DrawTextureFlipped(Texture texture, int x, int y, bool flipX, bool flipY, Color tint)
{
	DrawTextureFlippedV(texture, (Vector2) { (float)x, (float)y }, flipX, flipY, tint);
}
void DrawTextureFlippedV(Texture texture, Vector2 position, bool flipX, bool flipY, Color tint)
{
	int sx = flipX ? -1 : +1;
	int sy = flipY ? -1 : +1;
	Rectangle src = { 0, 0, (float)texture.width * sx, (float)texture.height * sy };
	Rectangle dst = RectangleFromCenter(position.x, position.y, (float)texture.width, (float)texture.height);
	DrawTexturePro(texture, src, dst, (Vector2) { 0 }, 0, tint);
}
void DrawTextureRotatedV(Texture texture, Vector2 position, float angle, Color tint)
{
	float w = (float)texture.width;
	float h = (float)texture.height;
	Rectangle src = { 0, 0, w, h };
	Rectangle dst = { position.x, position.y, w, h }; // Don't need to center this because the `origin` will already do the centering.
	DrawTexturePro(texture, src, dst, (Vector2) { w / 2, h / 2 }, angle, tint);
}
void DrawTextureRotatedFlippedV(Texture texture, Vector2 position, float angle, bool flipX, bool flipY, Color tint)
{
	float sx = flipX ? -1.0f : +1.0f;
	float sy = flipY ? -1.0f : +1.0f;
	float w = (float)texture.width;
	float h = (float)texture.height;
	Rectangle src = { 0, 0, w * sx, h * sy };
	Rectangle dst = { position.x, position.y, w, h }; // Don't need to center this because the `origin` will already do the centering.
	DrawTexturePro(texture, src, dst, (Vector2) { w / 2, h / 2 }, angle, tint);
}
void DrawAntiAliasedRectangle(Rectangle rect, Color color)
{
	float x0 = rect.x;
	float y0 = rect.y;
	float x1 = rect.x + rect.width;
	float y1 = rect.y + rect.height;
	int fullx0 = (int)ceilf(x0);
	int fully0 = (int)ceilf(y0);
	int fullx1 = (int)floorf(x1);
	int fully1 = (int)floorf(y1);
	int fullw = 1 + fullx1 - fullx0;
	int fullh = 1 + fully1 - fully0;
	float ledge = (float)fullx0 - x0;
	float tedge = (float)fully0 - y0;
	float redge = x1 - (float)fullx1;
	float bedge = y1 - (float)fully1;
	float tlcorner = tedge * ledge;
	float trcorner = tedge * redge;
	float blcorner = bedge * ledge;
	float brcorner = bedge * redge;
	Color lcolor = ColorAlphaMultiply(color, ledge);
	Color tcolor = ColorAlphaMultiply(color, tedge);
	Color rcolor = ColorAlphaMultiply(color, redge);
	Color bcolor = ColorAlphaMultiply(color, bedge);
	Color tlcolor = ColorAlphaMultiply(color, tlcorner);
	Color trcolor = ColorAlphaMultiply(color, trcorner);
	Color blcolor = ColorAlphaMultiply(color, blcorner);
	Color brcolor = ColorAlphaMultiply(color, brcorner);
	DrawRectangle(fullx0 - 1, fully0 - 1, 1, 1, tlcolor);
	DrawRectangle(fullx0, fully0 - 1, fullw, 1, tcolor);
	DrawRectangle(fullx1 + 1, fully0 - 1, 1, 1, trcolor);
	DrawRectangle(fullx0 - 1, fully0, 1, fullh, lcolor);
	DrawRectangle(fullx0, fully0, fullw, fullh, color);
	DrawRectangle(fullx1 + 1, fully0, 1, fullh, rcolor);
	DrawRectangle(fullx0 - 1, fully1 + 1, 1, 1, blcolor);
	DrawRectangle(fullx0, fully1 + 1, fullw, 1, bcolor);
	DrawRectangle(fullx1 + 1, fully1 + 1, 1, 1, brcolor);
}
void rlColor(Color color)
{
	rlColor4ub(color.r, color.g, color.b, color.a);
}
void rlVertex2fv(Vector2 xy)
{
	rlVertex2f(xy.x, xy.y);
}

// Text

Vector2 MeasureTextSize(Font font, const char *text, float fontSize)
{
	fontSize = fmaxf(fontSize, (float)font.baseSize);
	return MeasureTextEx(font, text, fontSize, 1);
}
void DrawTextCentered(Font font, char *text, float x, float y, float fontSize, Color color)
{
	DrawTextCenteredV(font, text, (Vector2) { x, y }, fontSize, color);
}
void DrawTextCenteredV(Font font, char *text, Vector2 pos, float fontSize, Color color)
{
	Vector2 size = MeasureTextSize(font, text, fontSize);
	pos.x = floorf(pos.x - 0.5f * size.x);
	pos.y = floorf(pos.y - 0.5f * size.y);
	DrawTextEx(font, text, pos, fontSize, 1, color);
}
void DrawTextCenteredOffset(Font font, char *text, float x, float y, float fontSize, Color color1, Color color2)
{
	DrawTextCentered(font, text, x + 1, y, fontSize, color2);
	DrawTextCentered(font, text, x, y + 1, fontSize, color2);
	DrawTextCentered(font, text, x + 1, y + 1, fontSize, color2);
	DrawTextCentered(font, text, x, y, fontSize, color1);
}

// Console

static char *SplitByWhitespace(char **inoutString)
{
	char *start = SkipWhitespace(*inoutString);
	char *end = start;
	while (*end and not isspace(*end))
		++end;

	char *result = SlicePointer(start, end);
	*inoutString = end;
	return result;
}
static ParsedCommand ParseCommand(char *command)
{
	ParsedCommand result = { 0 };

	result.name = SplitByWhitespace(&command);
	for (;;)
	{
		char *arg = SplitByWhitespace(&command);
		if (not arg[0] or arg[0] == '#')
			break;

		if (result.args.count < countof(result.args.items))
			ListAdd(&result.args, arg);
	}
	result.help = Slice(command, 0, -1);
	return result;
}
static int CompareCommands(const void *lhs, const void *rhs)
{
	const char *const *l = lhs;
	const char *const *r = rhs;
	return strcmp(*l, *r);
}
static bool HelpCommand(char *args[], int count)
{
	char *name = (count ? args[0] : NULL);

	List(char *, countof(Console.commands.items)) match = { 0 };
	for (int i = 0; i < Console.commands.count; ++i)
	{
		Command command = Console.commands.items[i];
		ParsedCommand parsed = ParseCommand(command.syntax);
		if (not name or StringsEqual(name, parsed.name))
			ListAdd(&match, command.syntax);
	}

	if (match.count == 0)
		TraceLog(LOG_INFO, "There are no commands with name '%s'", name);
	else
	{
		qsort(match.items, (size_t)match.count, sizeof match.items[0], CompareCommands);
		for (int i = 0; i < match.count; ++i)
			TraceLog(LOG_INFO, "  %s", match.items[i]);
	}
	return true;
}
static bool ClearCommand(char *args[], int count)
{
	unused(args); unused(count);
	Console.log.head = 0;
	Console.log.tail = 0;
	return true;
}

void AddCommand(char *syntaxLiteral, bool(*callback)(char *args[], int count))
{
	Command entry = { syntaxLiteral, callback };
	ListAdd(&Console.commands, entry);
}
void ExecuteCommand(char *command)
{
	ParsedCommand parsedCommand = ParseCommand(command);

	bool matchedOne = false;
	for (int i = 0; i < Console.commands.count; ++i)
	{
		Command match = Console.commands.items[i];
		ParsedCommand parsedMatch = ParseCommand(match.syntax);

		if (StringsEqual(parsedCommand.name, parsedMatch.name))
		{
			matchedOne = true;
			if (parsedCommand.args.count == parsedMatch.args.count)
			{
				if (not match.callback(parsedCommand.args.items, parsedCommand.args.count))
					TraceLog(LOG_ERROR, "%s failed: %s", parsedMatch.name, Console.failureReason);
				return;
			}
		}
	}

	if (matchedOne)
	{
		TraceLog(LOG_ERROR, "Unmatched command '%s' with %d arguments.", parsedCommand.name, parsedCommand.args.count);
		TraceLog(LOG_INFO, "Maybe you meant to use one of these commands:");
		ExecuteCommand(String("help %s", parsedCommand.name));
	}
	else
		TraceLog(LOG_ERROR, "Unknown command '%s'. Enter 'help' to list all commands.", parsedCommand.name);
}
bool FailCommand(_Printf_format_string_ char *reason, ...)
{
	va_list args;
	va_start(args, reason);
	Console.failureReason = StringArgs(reason, args);
	va_end(args);
	return false;
}

// Ui

static int MuTextWidth(mu_Font font, const char *str, int len)
{
	unused(font); unused(len);
	return (int)MeasureTextEx(Fonts.ui, str, (float)Fonts.ui.baseSize, 1).x;
}
static int MuTextHeight(mu_Font font)
{
	unused(font);
	return Fonts.ui.baseSize;
}
static Color MuColorToRayColor(mu_Color color)
{
	return (Color) { color.r, color.g, color.b, color.a };
}
static mu_Color RayColorToMuColor(Color color)
{
	return (mu_Color) { color.r, color.g, color.b, color.a };
}
static mu_Rect RayRectToMuRect(Rectangle rect)
{
	return (mu_Rect) { (int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height };
}

void UiPushIdInt(int id)
{
	mu_push_id(&Ui.context, &id, sizeof id);
}
void UiPushIdString(char *id)
{
	mu_push_id(&Ui.context, id, (int)strlen(id));
}
void UiPushIdPointer(void *id)
{
	mu_push_id(&Ui.context, &id, sizeof id);
}
void UiPopId(void)
{
	mu_pop_id(&Ui.context);
}
bool UiBeginWindow(char *title, Rectangle bounds)
{
	if (bounds.width < 0)
		bounds.width = GetScreenWidth() - bounds.width + 1;
	if (bounds.height < 0)
		bounds.height = GetScreenHeight() - bounds.height + 1;

	mu_Rect rect = RayRectToMuRect(bounds);
	mu_Container *container = mu_get_container(&Ui.context, title);
	if (container)
	{
		bool firstTime = container->rect.w == 0 and container->rect.h == 0;
		if (container->open and firstTime)
		{
			rect.x = LoadIniInt(String("%s.x", title), rect.x);
			rect.y = LoadIniInt(String("%s.y", title), rect.y);
			rect.w = LoadIniInt(String("%s.w", title), rect.w);
			rect.h = LoadIniInt(String("%s.h", title), rect.h);

			// Make sure the window doesn't end up out of bounds.
			int screenW = GetScreenWidth();
			int screenH = GetScreenHeight();
			rect.w = ClampInt(rect.w, 0, screenW); // Make sure window isn't too large for screen.
			rect.h = ClampInt(rect.h, 0, screenH);
			rect.x = MinInt(rect.x, screenW - rect.w); // Clamp the window against the bottom-right edge.
			rect.y = MinInt(rect.y, screenH - rect.h);
			rect.x = MaxInt(rect.x, 0); // Clamp the window against the top-left edge.
			rect.y = MaxInt(rect.y, 0);
		}
		else if (container->open and not firstTime)
		{
			SaveIniInt(String("%s.x", title), container->rect.x);
			SaveIniInt(String("%s.y", title), container->rect.y);
			SaveIniInt(String("%s.w", title), container->rect.w);
			SaveIniInt(String("%s.h", title), container->rect.h);
		}
	}

	int result = mu_begin_window_ex(&Ui.context, title, rect, MU_OPT_NOCLOSE);
	return result != 0;
}
void UiEndWindow(void)
{
	mu_end_window(&Ui.context);
}
void UiBeginGroup(void)
{
	mu_layout_begin_column(&Ui.context);
}
void UiEndGroup(void)
{
	mu_layout_end_column(&Ui.context);
}
bool UiBeginChild(char *title)
{
	mu_begin_panel_ex(&Ui.context, title, 0);
	return true;
}
void UiEndChild(void)
{
	mu_end_panel(&Ui.context);
}
bool UiBeginTreeNode(char *label)
{
	int result = mu_begin_treenode_ex(&Ui.context, label, 0);
	return result != 0;
}
void UiEndTreeNode(void)
{
	mu_end_treenode(&Ui.context);
}
bool UiBeginTooltip(void)
{
	bool hovered = UiIsItemHovered();
	if (hovered)
	{
		mu_Container *container = mu_get_container(&Ui.context, "Tooltip");
		container->rect.x = Ui.context.mouse_pos.x + 20;
		container->rect.y = Ui.context.mouse_pos.y + 20;
		mu_bring_to_front(&Ui.context, container);

		int opt = MU_OPT_AUTOSIZE | MU_OPT_NORESIZE | MU_OPT_NOSCROLL | MU_OPT_NOTITLE | MU_OPT_NOCLOSE | MU_OPT_CLOSED;
		mu_begin_window_ex(&Ui.context, "Tooltip", (mu_Rect) { 0 }, opt);
	}

	return hovered;
}
void UiEndTooltip(void)
{
	mu_end_window(&Ui.context);
}
bool UiHeader(char *label)
{
	int result = mu_header_ex(&Ui.context, label, MU_OPT_EXPANDED);
	return result != 0;
}
void UiLayoutEx(int height, int widths[], int count)
{
	mu_layout_row(&Ui.context, count, widths, height);
}
void UiSkip(void)
{
	mu_layout_next(&Ui.context);
}
int UiAvailableWidth(void)
{
	mu_Rect rect = mu_layout_next(&Ui.context);
	mu_layout_set_next(&Ui.context, rect, false);
	return rect.w;
}
int UiAvailableHeight(void)
{
	mu_Rect rect = mu_layout_next(&Ui.context);
	mu_layout_set_next(&Ui.context, rect, false);
	return rect.h;
}
int UiWindowWidth(void)
{
	mu_Container *container = mu_get_current_container(&Ui.context);
	return container->rect.w;
}
int UiWindowHeight(void)
{
	mu_Container *container = mu_get_current_container(&Ui.context);
	return container->rect.h;
}
bool UiIsItemHovered(void)
{
	return mu_mouse_over(&Ui.context, Ui.context.last_rect) != 0;
}
void UiTooltip(_Printf_format_string_ char *format, ...)
{
	if (UiBeginTooltip())
	{
		char text[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof text, format, args);
		va_end(args);
		
		int width = Ui.context.text_width(Ui.context.style->font, text, (int)strlen(text));
		UiLayout(0, width + 2 * Ui.context.style->padding);
		mu_label(&Ui.context, text);

		UiEndTooltip();
	}
}
void UiLabel(_Printf_format_string_ char *format, ...)
{
	va_list args;
	va_start(args, format);
	UiLabelArgs(format, args);
	va_end(args);
}
void UiLabelArgs(char *format, va_list args)
{
	char text[1024];
	vsnprintf(text, sizeof text, format, args);
	mu_label(&Ui.context, text);
}
void UiColorLabel(Color color, _Printf_format_string_ char *format, ...)
{
	va_list args;
	va_start(args, format);
	UiColorLabelArgs(color, format, args);
	va_end(args);
}
void UiColorLabelArgs(Color color, char *format, va_list args)
{
	mu_Color backup = Ui.context.style->colors[MU_COLOR_TEXT];
	Ui.context.style->colors[MU_COLOR_TEXT] = RayColorToMuColor(color);
	UiLabelArgs(format, args);
	Ui.context.style->colors[MU_COLOR_TEXT] = backup;
}
void UiTexture(Texture tex)
{
	mu_Rect rect = mu_layout_next(&Ui.context);
	mu_draw_icon(&Ui.context, tex.id + MU_ICON_MAX, rect, (mu_Color) { 255, 255, 255, 255 });
}
bool UiButton(char *label)
{
	int result = mu_button_ex(&Ui.context, label, 0, MU_OPT_ALIGNCENTER);
	return result != 0;
}
bool UiRadio(char *label, int *value, int index)
{
	bool wasActive = *value == index;
	UiPushIdInt(index);
	{
		mu_Id id = mu_get_id(&Ui.context, &value, sizeof value);
		mu_Rect r = mu_layout_next(&Ui.context);
		mu_update_control(&Ui.context, id, r, 0);

		if (Ui.context.mouse_pressed == MU_MOUSE_LEFT and Ui.context.focus == id)
			*value = index;

		int colorId = MU_COLOR_BUTTON;
		if (*value == index)
			colorId += 2;
		else if (Ui.context.hover == id)
			colorId += 1;
		Ui.context.draw_frame(&Ui.context, r, colorId);
		mu_draw_control_text(&Ui.context, label, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
	}
	UiPopId();
	bool active = *value == index;
	return active and not wasActive;
}
bool UiColorButton(char *label, Color color)
{
	mu_Id id = mu_get_id(&Ui.context, label, (int)strlen(label));
	mu_Rect rect = mu_layout_next(&Ui.context);
	mu_update_control(&Ui.context, id, rect, 0);
	bool buttonPressed = (Ui.context.mouse_pressed == MU_MOUSE_LEFT and Ui.context.focus == id);

	Color hoverColor = Brighter(color);
	Color focusColor = Darker(color);
	mu_Color backup = Ui.context.style->colors[MU_COLOR_BUTTON];
	mu_Color hoverBackup = Ui.context.style->colors[MU_COLOR_BUTTONHOVER];
	mu_Color focusBackup = Ui.context.style->colors[MU_COLOR_BUTTONFOCUS];
	Ui.context.style->colors[MU_COLOR_BUTTON] = RayColorToMuColor(color);
	Ui.context.style->colors[MU_COLOR_BUTTONHOVER] = RayColorToMuColor(hoverColor);
	Ui.context.style->colors[MU_COLOR_BUTTONFOCUS] = RayColorToMuColor(focusColor);
	{
		mu_draw_control_frame(&Ui.context, id, rect, MU_COLOR_BUTTON, 0);
	}
	Ui.context.style->colors[MU_COLOR_BUTTON] = backup;
	Ui.context.style->colors[MU_COLOR_BUTTONHOVER] = hoverBackup;
	Ui.context.style->colors[MU_COLOR_BUTTONFOCUS] = focusBackup;

	Color textColor = ContrastingColorBlackOrWhite(color);
	mu_Color textBackup = Ui.context.style->colors[MU_COLOR_TEXT];
	Ui.context.style->colors[MU_COLOR_TEXT] = RayColorToMuColor(textColor);
	{
		mu_draw_control_text(&Ui.context, label, rect, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
	}
	Ui.context.style->colors[MU_COLOR_TEXT] = textBackup;

	return buttonPressed;
}
bool UiTextureButton(char *label, Texture tex)
{
	bool pressed = false;
	mu_Id id = mu_get_id(&Ui.context, label, (int)strlen(label));
	mu_Rect r = mu_layout_next(&Ui.context);
	mu_update_control(&Ui.context, id, r, 0);
	
	if (Ui.context.mouse_pressed == MU_MOUSE_LEFT and Ui.context.focus == id)
		pressed = true;
	
	mu_draw_control_frame(&Ui.context, id, r, MU_COLOR_BUTTON, 0);
	mu_draw_icon(&Ui.context, tex.id + MU_ICON_MAX, r, (mu_Color) { 255, 255, 255, 255 });
	return pressed;
}
bool UiTextureRadio(int *value, int index, Texture tex)
{
	bool wasActive = *value == index;
	UiPushIdInt(index);
	{
		mu_Id id = mu_get_id(&Ui.context, &value, sizeof value);
		mu_Rect r = mu_layout_next(&Ui.context);
		mu_update_control(&Ui.context, id, r, 0);

		if (Ui.context.mouse_pressed == MU_MOUSE_LEFT and Ui.context.focus == id)
			*value = index;

		int colorId = MU_COLOR_BUTTON;
		if (*value == index)
			colorId += 2;
		else if (Ui.context.hover == id)
			colorId += 1;
		Ui.context.draw_frame(&Ui.context, r, colorId);
		mu_draw_icon(&Ui.context, tex.id + MU_ICON_MAX, r, (mu_Color) { 255, 255, 255, 255 });
	}
	UiPopId();
	bool active = *value == index;
	return active and not wasActive;
}
bool UiCheckbox(bool *value)
{
	int result;
	UiPushIdPointer(value);
	{
		int state = *value ? 1 : 0;
		result = mu_checkbox(&Ui.context, "", &state);
		*value = state != 0;
	}
	UiPopId();
	return result != 0;
}
bool UiSlider(float *value, float min, float max)
{
	int result = mu_slider_ex(&Ui.context, value, min, max, 0, "%.2f", MU_OPT_ALIGNCENTER);
	return result != 0;
}
bool UiSliderInt(int *value, int min, int max)
{
	int initial = *value;
	int available = UiAvailableWidth();
	UiPushIdPointer(value);
	UiBeginGroup();
	{
		const int buttonSize = 20;
		int spacing = Ui.context.style->spacing;
		int size = MaxInt(0, available - 2 * (buttonSize + spacing));

		UiLayout(0, buttonSize, size, buttonSize);

		static float x;
		x = (float)*value;

		float increment = (Ui.context.key_down & MU_KEY_SHIFT) ? 5.0f : 1.0f;
		x -= UiButton("-") ? increment : 0;
		mu_slider_ex(&Ui.context, &x, (float)min, (float)max, 1, "%.0f", MU_OPT_ALIGNCENTER);
		x += UiButton("+") ? increment : 0;

		*value = ClampInt((int)roundf(x), min, max);
	}
	UiEndGroup();
	UiPopId();
	return *value != initial;
}
bool UiSliderVector2(Vector2 *value, float min, float max)
{
	return UiSliderVector2Ex(value, (Vector2) { min, min }, (Vector2) { max, max });
}
bool UiSliderVector2Ex(Vector2 *value, Vector2 min, Vector2 max)
{
	int avail = UiAvailableWidth() - Ui.context.style->spacing;
	int result = 0;
	UiBeginGroup();
	{
		int width = MaxInt(1, avail / 2);
		int remaining = avail & 1;
		UiLayout(0, width, width + remaining);
		result |= UiSlider(&value->x, min.x, max.x);
		result |= UiSlider(&value->y, min.y, max.y);
	}
	UiEndGroup();
	return result != 0;
}
bool UiColorPicker(Color *value)
{
	bool changed = false;
	mu_push_id(&Ui.context, &value, sizeof value);
	{
		static bool prevPopupOpen;
		static mu_Id revertId;
		static Color revertColor;
		mu_Id id = Ui.context.id_stack.items[Ui.context.id_stack.idx - 1];

		if (UiColorButton("", *value))
			mu_open_popup(&Ui.context, "Color Picker");

		if (mu_begin_popup(&Ui.context, "Color Picker"))
		{
			if (not prevPopupOpen or id != revertId)
			{
				revertColor = *value;
				revertId = id;
			}
			prevPopupOpen = true;

			static Vector3 hsv;
			Color hsvColor = ColorFromHSV(hsv.x, hsv.y, hsv.z);
			if (hsvColor.r != value->r or hsvColor.g != value->g or hsvColor.b != value->b)
				hsv = ColorToHSV(*value);

			UiLayout(0, 80, 80);
			{
				Color hColor = ColorFromHSV(hsv.x, 1, 1);
				Color sColor = Grayscale(hsv.y);
				Color vColor = Grayscale(hsv.z);

				int sliderRes = 0;
				UiColorLabel(hColor, "Hue");
				sliderRes |= mu_slider_ex(&Ui.context, &hsv.x, 0, 360, 1, "%.0f", MU_OPT_ALIGNCENTER);
				UiColorLabel(sColor, "Saturation");
				sliderRes |= mu_slider_ex(&Ui.context, &hsv.y, 0, 1, 0, "%.2f", MU_OPT_ALIGNCENTER);
				UiColorLabel(vColor, "Value");
				sliderRes |= mu_slider_ex(&Ui.context, &hsv.z, 0, 1, 0, "%.2f", MU_OPT_ALIGNCENTER);

				if (sliderRes)
				{
					changed = true;
					Color hsvValue = ColorFromHSV(hsv.x, hsv.y, hsv.z);
					value->r = hsvValue.r;
					value->g = hsvValue.g;
					value->b = hsvValue.b;
				}
			}

			float r = value->r;
			float g = value->g;
			float b = value->b;
			float a = value->a;
			{
				Color rColor = { value->r, 0, 0, 255 };
				Color gColor = { 0, value->g, 0, 255 };
				Color bColor = { 0, 0, value->b, 255 };
				Color aColor = { value->a, value->a, value->a, 255 };

				int sliderRes = 0;
				UiColorLabel(rColor, "Red");
				sliderRes |= mu_slider_ex(&Ui.context, &r, 0, 255, 1, "%.0f", MU_OPT_ALIGNCENTER);
				UiColorLabel(gColor, "Green");
				sliderRes |= mu_slider_ex(&Ui.context, &g, 0, 255, 1, "%.0f", MU_OPT_ALIGNCENTER);
				UiColorLabel(bColor, "Blue");
				sliderRes |= mu_slider_ex(&Ui.context, &b, 0, 255, 1, "%.0f", MU_OPT_ALIGNCENTER);
				UiColorLabel(aColor, "Alpha");
				sliderRes |= mu_slider_ex(&Ui.context, &a, 0, 255, 1, "%.0f", MU_OPT_ALIGNCENTER);

				changed |= sliderRes != 0;
			}
			value->r = (unsigned char)Clamp(roundf(r), 0, 255);
			value->g = (unsigned char)Clamp(roundf(g), 0, 255);
			value->b = (unsigned char)Clamp(roundf(b), 0, 255);
			value->a = (unsigned char)Clamp(roundf(a), 0, 255);

			UiSkip();
			if (UiColorButton("Revert", revertColor))
			{
				*value = revertColor;
				changed = true;
			}

			mu_end_popup(&Ui.context);
		}
		else prevPopupOpen = false;
	}
	mu_pop_id(&Ui.context);
	return changed;
}
bool UiTextBoxEx(char text[], int capacity)
{
	int result = mu_textbox_ex(&Ui.context, text, capacity, 0);
	return result == MU_RES_SUBMIT;
}
bool UiFilePickerEx(char buffer[], int capacity)
{
	bool changed = false;
	UiPushIdPointer(buffer);
	UiBeginGroup();
	{
		UiLayout(0, -40, -1);
		
		mu_Color backup = Ui.context.style->colors[MU_COLOR_TEXT];
		Ui.context.style->colors[MU_COLOR_TEXT] = FileExists(buffer) ? (mu_Color) { 230, 230, 230, 255 } : (mu_Color) { 255, 128, 128, 255 };
		changed |= UiTextBoxEx(buffer, capacity);
		Ui.context.style->colors[MU_COLOR_TEXT] = backup;
		
		if (UiButton("..."))
		{
			#ifdef _WIN32
			{
				IFileDialog *dialog = NULL;
				CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_ALL, &IID_IFileDialog, (void **)&dialog);
				unsigned options = 0;
				dialog->vtable->GetOptions(dialog, &options);
				options |= FOS_FILEMUSTEXIST;
				dialog->vtable->SetOptions(dialog, options);
				dialog->vtable->Show(dialog, NULL);
				IShellItem *dir = NULL;
				bool somethingWasPicked = dialog->vtable->GetResult(dialog, &dir) >= 0;
				if (somethingWasPicked)
				{
					wchar_t *pathW = NULL;
					dir->vtable->GetDisplayName(dir, SIGDN_FILESYSPATH, &pathW);
					WideCharToMultiByte(CP_UTF8, 0, pathW, -1, buffer, capacity, NULL, NULL);
					dir->vtable->Release(dir);
					changed = true;
				}
				dialog->vtable->Release(dialog);
			}
			#endif
		}
	}
	UiEndGroup();
	UiPopId();
	return changed;
}
bool UiDirectoryPickerEx(char buffer[], int capacity)
{
	bool changed = false;
	UiPushIdPointer(buffer);
	UiBeginGroup();
	{
		UiLayout(0, -40, -1);

		mu_Color backup = Ui.context.style->colors[MU_COLOR_TEXT];
		Ui.context.style->colors[MU_COLOR_TEXT] = DirectoryExists(buffer) ? (mu_Color) { 230, 230, 230, 255 } : (mu_Color) { 255, 128, 128, 255 };
		changed |= UiTextBoxEx(buffer, capacity);
		Ui.context.style->colors[MU_COLOR_TEXT] = backup;
		
		if (UiButton("..."))
		{
			#ifdef _WIN32
			{
				IFileDialog *dialog = NULL;
				CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_ALL, &IID_IFileDialog, (void **)&dialog);
				unsigned options = 0;
				dialog->vtable->GetOptions(dialog, &options);
				options |= FOS_PICKFOLDERS;
				options |= FOS_PATHMUSTEXIST;
				dialog->vtable->SetOptions(dialog, options);
				dialog->vtable->Show(dialog, NULL);
				IShellItem *dir = NULL;
				bool somethingWasPicked = dialog->vtable->GetResult(dialog, &dir) >= 0;
				if (somethingWasPicked)
				{
					wchar_t *pathW = NULL;
					dir->vtable->GetDisplayName(dir, SIGDN_FILESYSPATH, &pathW);
					WideCharToMultiByte(CP_UTF8, 0, pathW, -1, buffer, capacity, NULL, NULL);
					dir->vtable->Release(dir);
					changed = true;
				}
				dialog->vtable->Release(dialog);
			}
			#endif
		}
	}
	UiEndGroup();
	UiPopId();
	return changed;
}
bool UiConsole(Rectangle bounds)
{
	bool open = UiBeginWindow("Console", bounds);
	if (open)
	{
		mu_Container *panel = NULL;
		UiLayout(-25, -1);
		if (UiBeginChild("Output"))
		{
			panel = mu_get_current_container(&Ui.context);
			UiLayout(0, -1);
			mu_Color backup = Ui.context.style->colors[MU_COLOR_TEXT];
			for (int i = Console.log.tail; i != Console.log.head; i = (i + 1) % MAX_LOG_ENTRIES)
			{
				static const mu_Color table[LOG_NONE] = {
					[LOG_TRACE] = { 150, 150, 150, 255 },
					[LOG_DEBUG] = { 150, 150, 150, 255 },
					[LOG_INFO] = { 230, 230, 230, 255 },
					[LOG_WARNING] = { 230, 128, 0, 255 },
					[LOG_ERROR] = { 230, 0, 0, 255 },
					[LOG_FATAL] = { 255, 0, 0, 255 },
					[LOG_ALL] = { 255, 0, 0, 255 },
				};

				LogEntry *entry = &Console.log.items[i];
				Ui.context.style->colors[MU_COLOR_TEXT] = table[entry->level];
				mu_label(&Ui.context, entry->message);
			}
			Ui.context.style->colors[MU_COLOR_TEXT] = backup;
			UiEndChild();

			static int lastHead;
			static int lastTail;
			if (lastHead != Console.log.head or lastTail != Console.log.tail)
			{
				panel->scroll.y = panel->content_size.y;
				lastHead = Console.log.head;
				lastTail = Console.log.tail;
			}
		}

		bool execute = false;
		static char input[256];
		UiLayout(0, -70, -1);
		UiBeginGroup();
		{
			UiLayout(0, 20, -1);
			UiLabel(">");
			if (UiTextBox(input))
			{
				mu_set_focus(&Ui.context, Ui.context.last_id);
				execute = true;
			}
		}
		UiEndGroup();
		if (UiButton("Enter"))
			execute = true;

		if (execute)
		{
			TraceLog(LOG_INFO, "> %s", input);
			ExecuteCommand(input);
			input[0] = 0;
		}

		UiEndWindow();
	}
	return open;
}

// Runtime

static RenderTexture framebuffer;

static void LogCallback(int logLevel, const char *text, va_list args)
{
	LogEntry *entry = RingAllocate(&Console.log);
	entry->level = logLevel;
	vsnprintf(entry->message, sizeof entry->message, text, args);

	#ifdef _WIN32
	{
		OutputDebugStringA(entry->message);
		OutputDebugStringA("\n");
	}
	#else
	{
		fputs(entry->message, logLevel >= LOG_WARNING ? stderr : stdout);
	}
	#endif
}

void OneGameLoopIteration(void)
{
	if (IsKeyPressed(KEY_F2))
	{
		// Don't go fullscreen on web. It works weirdly and is confusing.
		#ifdef __EMSCRIPTEN__
		{
			EM_ASM(Module.requestFullscreen(false, true););
		}
		#else
		{
			ToggleFullscreen();
		}
		#endif
	}

	int width = GetScreenWidth();
	int height = GetScreenHeight();
	#ifndef __EMSCRIPTEN__
	{
		// Don't store this stuff for web. Always start with a clean slate there, because it's just too messy otherwise.
		if (IsWindowResized())
		{
			SaveIniInt("Width", width);
			SaveIniInt("Height", height);
			SaveIniBool("Maximized", IsWindowMaximized());
			SaveIniBool("Fullscreen", IsWindowFullscreen());
		}
	}
	#endif
	int scaleX = MaxInt(1, width / SCREEN_SIZE);
	int scaleY = MaxInt(1, height / SCREEN_SIZE);
	int scale = MinInt(scaleX, scaleY);
	int size = SCREEN_SIZE * scale;
	int offsetX = (width - size) / 2;
	int offsetY = (height - size) / 2;

	Vector2 mousePosition = GetMousePosition();
	Input.mouseUnclampedPosition.x = (mousePosition.x - offsetX) / scale;
	Input.mouseUnclampedPosition.y = (mousePosition.y - offsetY) / scale;
	Input.mousePosition.x = Clamp(Input.mouseUnclampedPosition.x, 0, PrevFloat(SCREEN_SIZE));
	Input.mousePosition.y = Clamp(Input.mouseUnclampedPosition.y, 0, PrevFloat(SCREEN_SIZE));
	Input.mouseUnclampedX = (int)Input.mouseUnclampedPosition.x;
	Input.mouseUnclampedY = (int)Input.mouseUnclampedPosition.y;
	Input.mouseX = (int)Input.mousePosition.x;
	Input.mouseY = (int)Input.mousePosition.y;
	
	Vector2 mouseDelta = GetMouseDelta();
	Input.mouseDelta.x = mouseDelta.x / scale;
	Input.mouseDelta.y = mouseDelta.y / scale;
	Input.mouseDx = (int)Input.mouseDelta.x;
	Input.mouseDy = (int)Input.mouseDelta.y;

	Input.mouseIsInGameWindow = 
		Input.mouseUnclampedX >= 0 and Input.mouseUnclampedX < SCREEN_SIZE and 
		Input.mouseUnclampedY >= 0 and Input.mouseUnclampedY < SCREEN_SIZE;

	Input.control = IsKeyDown(KEY_LEFT_CONTROL) or IsKeyDown(KEY_RIGHT_CONTROL);
	Input.shift = IsKeyDown(KEY_LEFT_SHIFT) or IsKeyDown(KEY_RIGHT_SHIFT);
	Input.alt = IsKeyDown(KEY_LEFT_ALT) or IsKeyDown(KEY_RIGHT_ALT);

	for (int cursor = 0; cursor < countof(Input.text) - 1; ++cursor)
	{
		int c = GetCharPressed();
		Input.text[cursor + 0] = (char)c;
		Input.text[cursor + 1] = 0;
		if (not c)
			break;
	}

	Allocator.cursor = 0;
	Time.now = GetTime();
	BeginTextureMode(framebuffer);
	{
		Update();
	}
	EndTextureMode();

	// Stretch the framebuffer over the available window area.
	BeginDrawing();
	{
		ClearBackground(BLACK);

		// For some reason we need to Y-flip the framebuffer.
		Rectangle src = { 0, 0, SCREEN_SIZE, -1 * SCREEN_SIZE };
		Rectangle dst = { (float)offsetX, (float)offsetY, (float)size, (float)size };
		rlDrawRenderBatchActive();
		rlSetBlendMode(RL_BLEND_ALPHA_PREMULTIPLY);
		DrawTexturePro(framebuffer.texture, src, dst, (Vector2) { 0 }, 0, WHITE);
		rlDrawRenderBatchActive();
		rlSetBlendMode(RL_BLEND_ALPHA);

		static const int UiKeymap[][2] = {
			{ KEY_LEFT_CONTROL, MU_KEY_CTRL },
			{ KEY_RIGHT_CONTROL, MU_KEY_CTRL },
			{ KEY_LEFT_SHIFT, MU_KEY_SHIFT },
			{ KEY_RIGHT_SHIFT, MU_KEY_SHIFT },
			{ KEY_LEFT_ALT, MU_KEY_ALT },
			{ KEY_RIGHT_ALT, MU_KEY_ALT },
			{ KEY_BACKSPACE, MU_KEY_BACKSPACE },
			{ KEY_ENTER, MU_KEY_RETURN },
			{ KEY_DELETE, MU_KEY_DELETE },
		};
		for (int keyIdx = 0; keyIdx < countof(UiKeymap); ++keyIdx)
		{
			int rayKey = UiKeymap[keyIdx][0];
			int muKey = UiKeymap[keyIdx][1];
			if (IsKeyPressed(rayKey))
				mu_input_keydown(&Ui.context, muKey);
			if (IsKeyReleased(rayKey))
				mu_input_keyup(&Ui.context, muKey);
		}

		static const int UiButtonmap[][2] = {
			{ MOUSE_BUTTON_LEFT, MU_MOUSE_LEFT },
			{ MOUSE_BUTTON_RIGHT, MU_MOUSE_RIGHT },
			{ MOUSE_BUTTON_MIDDLE, MU_MOUSE_MIDDLE },
		};
		for (int buttonIdx = 0; buttonIdx < countof(UiButtonmap); ++buttonIdx)
		{
			int rayButton = UiButtonmap[buttonIdx][0];
			int muButton = UiButtonmap[buttonIdx][1];
			if (IsMouseButtonPressed(rayButton))
				mu_input_mousedown(&Ui.context, GetMouseX(), GetMouseY(), muButton);
			if (IsMouseButtonReleased(rayButton))
				mu_input_mouseup(&Ui.context, GetMouseX(), GetMouseY(), muButton);
		}

		mu_input_mousemove(&Ui.context, GetMouseX(), GetMouseY());
		Vector2 wheel = GetMouseWheelMoveV();
		mu_input_scroll(&Ui.context, (int)Sign(wheel.x), -(int)(30 * wheel.y));
		mu_input_text(&Ui.context, Input.text);

		Time.now = GetTime();
		mu_begin(&Ui.context);
		{
			UpdateUi();
		}
		mu_end(&Ui.context);

		BeginScissorMode(0, 0, width, height);
		{
			mu_Command *command = NULL;
			while (mu_next_command(&Ui.context, &command))
			{
				if (command->type == MU_COMMAND_TEXT)
				{
					mu_Vec2 pos = command->text.pos;
					mu_Color color = command->text.color;
					DrawTextEx(Fonts.ui, command->text.str, (Vector2) { (float)pos.x, (float)pos.y }, (float)Fonts.ui.baseSize, 1, (Color) { color.r, color.g, color.b, color.a });
				}
				else if (command->type == MU_COMMAND_RECT)
				{
					mu_Rect rect = command->rect.rect;
					mu_Color color = command->rect.color;
					DrawRectangle(rect.x, rect.y, rect.w, rect.h, (Color) { color.r, color.g, color.b, color.a });
				}
				else if (command->type == MU_COMMAND_ICON)
				{
					int id = command->icon.id;
					mu_Rect muRect = command->icon.rect;
					mu_Color muColor = command->icon.color;
					Rectangle rect = { (float)muRect.x, (float)muRect.y, (float)muRect.w, (float)muRect.h };
					Color color = { muColor.r, muColor.g, muColor.b, muColor.a };
					Rectangle fill = ExpandRectangle(rect, -6);
					if (id == MU_ICON_CLOSE)
						DrawTextCenteredV(Fonts.ui, "X", RectangleCenter(rect), UI_FONT_SIZE, color);
					else if (id == MU_ICON_COLLAPSED)
					{
						Vector2 topLeft = RectangleTopLeft(fill);
						Vector2 bottomLeft = RectangleBottomLeft(fill);
						Vector2 right = RectangleCenterRight(fill);
						DrawTriangle(topLeft, bottomLeft, right, color);
					}
					else if (id == MU_ICON_EXPANDED)
					{
						Vector2 topLeft = RectangleTopLeft(fill);
						Vector2 topRight = RectangleTopRight(fill);
						Vector2 bottom = RectangleBottomMiddle(fill);
						DrawTriangle(topRight, topLeft, bottom, color);
					}
					else if (id == MU_ICON_RESIZE)
					{
						Vector2 bottomLeft = RectangleBottomLeft(fill);
						Vector2 bottomRight = RectangleBottomRight(fill);
						Vector2 topRight = RectangleTopRight(fill);
						DrawTriangle(bottomLeft, bottomRight, topRight, color);
					}
					else if (id == MU_ICON_CHECK)
					{
						DrawRectangleRec(ExpandRectangle(fill, 3), color);
					}
					else if (id >= MU_ICON_MAX)
					{
						Texture texture = {
							.id = command->icon.id - MU_ICON_MAX,
							.width = 1,
							.height = 1,
							.mipmaps = 1,
							.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
						};
						DrawTexturePro(texture, (Rectangle) { 0, 0, 1, 1 }, fill, (Vector2) { 0 }, 0, color);
					}
				}
				else if (command->type == MU_COMMAND_CLIP)
				{
					mu_Rect rect = command->icon.rect;
					EndScissorMode();
					BeginScissorMode(rect.x, rect.y, rect.w, rect.h);;
				}
			}
		}
		EndScissorMode();
	}
	EndDrawing();

	++Time.frame;
	if (Ini.wasModified)
		SaveIniFile();
	if (Filesystem.needsSync and not Filesystem.syncAlreadyRunning)
	{
		#ifdef __EMSCRIPTEN__
		{
			Filesystem.syncAlreadyRunning = true;
			EM_ASM(FS.syncfs(false, function(error) { Module._SyncFinished(); }));
		}
		#endif
		Filesystem.needsSync = false;
	}
}
void StartGameLoop(void)
{
	LoadIniFile();

	int width = LoadIniInt("Width", 3 * SCREEN_SIZE);
	int height = LoadIniInt("Height", 3 * SCREEN_SIZE);
	bool maximized = LoadIniBool("Maximized", false);
	bool fullscreen = LoadIniBool("Fullscreen", false);
	
	//@HACK Windows freaks out and gets confused if I initialize the window with the maximized width/height.
	//      It thinks that it's supposed to be a fullscreen window, but then we maximize it later and it creates
	//      a bizare afterimage of the window. It also doesn't happen on all computers. This seems to fix it.
	if (maximized)
		--width;

	SetTraceLogCallback(LogCallback);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(width, height, "Chocoro");
	SetWindowMinSize(SCREEN_SIZE, SCREEN_SIZE);
	SetTargetFPS(FPS);
	SetExitKey(KEY_F1);
	InitAudioDevice();
	rlDisableDepthTest();
	rlDisableBackfaceCulling();
	if (fullscreen)
		ToggleFullscreen();
	else if (maximized)
		MaximizeWindow(); // FLAG_WINDOW_MAXIMIZED doesn't change anything. We need to manually maximize.

	framebuffer = LoadRenderTexture(SCREEN_SIZE, SCREEN_SIZE);
	Fonts.game = LoadFontEx("alkhemikal.ttf", 16, NULL, 95); // https://fontenddev.com/fonts/alkhemikal/
	Fonts.ui = LoadFontEx("roboto.ttf", UI_FONT_SIZE, NULL, 95); // https://fonts.google.com/specimen/Roboto
	Fonts.raylib = GetFontDefault();

	mu_init(&Ui.context);
	Ui.context.text_width = MuTextWidth;
	Ui.context.text_height = MuTextHeight;

	AddCommand("help # List all commands.", HelpCommand);
	AddCommand("help command # Show help for the specified command.", HelpCommand);
	AddCommand("clear # Clear the console screen.", ClearCommand);

	Allocator.cursor = 0;
	Time.now = GetTime();
	Initialize();

	// On the web, the browser wants to drive the main loop. On other platforms, we drive it.
	// See: https://emscripten.org/docs/porting/emscripten-runtime-environment.html#browser-main-loop
	#ifdef __EMSCRIPTEN__
	{
		emscripten_set_main_loop(OneGameLoopIteration, 0, false);
	}
	#else
	{
		while (not WindowShouldClose())
			OneGameLoopIteration();
	}
	#endif
}
void SyncFinished(void)
{
	// This function is exclusively called from the syncfs callback.
	Filesystem.syncAlreadyRunning = false;
}

int main(void)
{
	// On Windows, we store all saved data in user/AppData/Roaming/Chocoro.
	// On the web, we store it to an IndexDB filesystem mounted at /save.
	#if defined _WIN32
	{
		Debug.on = IsDebuggerPresent() != 0;

		wchar_t pathW[512];
		SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, pathW);

		for (int i = 0;; ++i)
		{
			if (pathW[i] == L'\\')
				pathW[i] = L'/';
			else if (not pathW[i])
			{
				memcpy(&pathW[i], L"/Chocoro", sizeof L"/Chocoro");
				break;
			}
		}

		CreateDirectoryW(pathW, NULL);
		WarnIf(not WideCharToMultiByte(CP_UTF8, 0, pathW, -1, Filesystem.savePath, sizeof Filesystem.savePath, NULL, NULL));
	}
	#elif defined __EMSCRIPTEN__
	{
		CopyString(Filesystem.savePath, "/save");
	}
	#endif

	char *appDir = (char *)GetApplicationDirectory();
	char *resDir = String("%s/res", appDir);
	WarnIf(not ChangeDirectory(resDir));

	#if defined __EMSCRIPTEN__
	{
		// This is very unfortunate. In order to access anything from the IndexDB filesystem
		// we first need to synchronize it. But this is an asynchronous operation. I can't seem to
		// find any way at all to block execution here until the sync completes (which is quite stupid).
		// So instead we need to pause execution here and then resume it later when the sync completes.
		// Right now, this requires us to call a C function from javascript, which means the emscripten
		// build script now needs to know the exact name of the function we want to call. Very unfortunate.
		// It also causes emscripten to throw an exception after main finishes for some reason.
		// There's nothing wrong per-se with the exception, but it does show up in the logs as an error...
		// https://emscripten.org/docs/api_reference/Filesystem-API.html#FS.syncfs
		// https://github.com/emscripten-core/emscripten/issues/3772
		Filesystem.syncAlreadyRunning = true;
		EM_ASM(
			FS.mkdir('/save');
			FS.mount(IDBFS, {}, '/save');
			FS.syncfs(true, function(error) { Module._SyncFinished(); Module._StartGameLoop(); });
		);
	}
	#else
	{
		StartGameLoop();
	}
	#endif
}
#ifdef _WIN32
int __stdcall WinMain(void *instance, void *prevInstance, char *commandLine, int show)
{
	unused(instance); unused(prevInstance); unused(commandLine); unused(show);
	return main();
}
#endif
