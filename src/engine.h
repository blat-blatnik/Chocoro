#pragma once

#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <iso646.h>
#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

// Compatibility

#ifndef _MSC_VER
#	define __debugbreak() ((void)0)
#	define _Printf_format_string_
#endif

// Macros

#define unused(x) ((void)x)
#define paste(a, b) a##b
#define decay(macro) paste(macro,) // Used for yield() because MSVC doesn't consider __LINE__ to be a constant expression.
#define break() (Debug.on and (__debugbreak(), 1))
#define countof(array) (sizeof(array) / sizeof((array)[0]))
#define coroutine(pCoroutine) Coroutines.current = (pCoroutine); switch (Coroutines.current->state) case 0:
#define yield(...) do{ Coroutines.current->state = decay(__LINE__); return __VA_ARGS__; case decay(__LINE__):; }while(0)

// Constants

#define FPS 60
#define SCREEN_SIZE 256
#define SCREEN_CENTER (SCREEN_SIZE / 2)
#define SCREEN_RECT ((Rectangle){ 0, 0, SCREEN_SIZE, SCREEN_SIZE })

// Enumerations

typedef enum SerialOp {
	SAVE,
	LOAD,
} SerialOp;

// Datatypes

typedef struct Coroutine {
	int state;
} Coroutine;

typedef struct Sweep {
	float t;
	Vector2 normal;
} Sweep;

// Globals

extern struct Debug {
	bool on;
} Debug;
extern struct Time {
	int frame;
	double now;
} Time;
extern struct Input {
	char text[100];
	int mouseX; // [0, 255]
	int mouseY; // [0, 255]
	int mouseDx;
	int mouseDy;
	int mouseUnclampedX; // [0, 255] but may be outside of this range if mouse isn't in game window.
	int mouseUnclampedY; // [0, 255] but may be outside of this range if mouse isn't in game window.
	Vector2 mousePosition; // [0, 256)
	Vector2 mouseDelta;
	Vector2 mouseUnclampedPosition; // [0, 255) but may be outside of this range if mouse isn't in game window.
	bool mouseIsInGameWindow;
	bool control; // true if left or right control key is down.
	bool shift; // true if left or right shift key is down.
	bool alt; // true if left or right alt key is down.
} Input;
extern struct Fonts {
	Font raylib;
	Font game;
	Font ui;
} Fonts;
extern struct Coroutines {
	Coroutine *current;
} Coroutines;

// Debugging

#define Warn(condition) (TraceLog(LOG_WARNING, "'%s' in %s() file '%s' line %d.", #condition, __func__, __FILE__, __LINE__), break())
#define WarnXIf(condition, action){\
	if (condition)\
	{\
		Warn(condition);\
		action;\
	}\
}
#define WarnIf(condition){\
	WarnXIf(condition,);\
}
#define WarnReturnIf(condition){\
	WarnXIf(condition, return);\
}
#define WarnReturn0If(condition){\
	WarnXIf(condition, return 0);\
}

// Bytes

void ZeroBytes(void *bytes, int count);
bool BytesAreZero(void *bytes, int count);
bool BytesEqual(void *a, void *b, int count);
void SwapBytes(void *a, void *b, int count);

#define ZeroStruct(pStruct) ZeroBytes((pStruct), sizeof *(pStruct))
#define ZeroArray(array) ZeroBytes((array), sizeof(array))
#define StructIsZero(struct) BytesAreZero(&(struct), sizeof(struct))
#define StructsEqual(a, b) (sizeof(a) == sizeof(b) and BytesEqual(&(a), &(b), sizeof(a)))
#define SwapStructs(pA, pB) SwapBytes((pA), (pB), MinInt(sizeof *(pA), sizeof *(pB)))

// String

int CopyStringEx(char *to, int capacity, char *from);
bool StringsEqual(char *a, char *b);
char *Slice(char *string, int start, int end);
char *SlicePointer(char *start, char *endExclusive);
char *String(_Printf_format_string_ char *format, ...);
char *StringArgs(char *format, va_list args);
char *SkipWhitespace(char *string);

#define CopyString(to, from) CopyStringEx((to), sizeof(to), (from))
#define FormatString(to, format, ...) snprintf((to), sizeof(to), (format),##__VA_ARGS__)

// Math

float Sign(float x);
float Clamp01(float x);
float Wrap01(float x);
float WrapAngle(float angle);
float FlipAngle(float angle);
float Smoothstart01(float x);
float Smoothstart(float x, float edge0, float edge1);
float Smoothstop01(float x);
float Smoothstop(float x, float edge0, float edge1);
float Smoothstep01(float x);
float Smoothstep(float x, float edge0, float edge1);
float Smootherstep01(float x);
float Smootherstep(float x, float edge0, float edge1);
float Smoothbounce01(float x);
float Sawtooth(float x);
float PrevFloat(float x);
float NextFloat(float x);
float ShortestAngleDifference(float from, float to);
float Atan2Vector(Vector2 xy);
float SineLerp(float t);
int IntSign(int x);
int MinInt(int a, int b);
int MaxInt(int a, int b);
int ClampInt(int x, int min, int max);
int WrapInt(int x, int min, int max);
int RemapInt(int x, int fromMin, int fromMax, int toMin, int toMax);

// Vector

Vector2 RotateClockwise(Vector2 v);
Vector2 RotateCounterClockwise(Vector2 v);
Vector2 Vector2FromPolar(float length, float angle);
Vector2 UnitVector2WithAngle(float angle);
Vector2 Vector2Floor(Vector2 v);
Vector2 Vector2Ceil(Vector2 v);
Vector2 Vector2LerpAngle(Vector2 v1, Vector2 v2, float amount);

// Rectangle

float RectangleRight(Rectangle rect);
float RectangleBottom(Rectangle rect);
Vector2 RectangleMin(Rectangle rect);
Vector2 RectangleMax(Rectangle rect);
Vector2 RemapToRectangle(Rectangle rect, float u, float v);
Vector2 RemapToRectangleV(Rectangle rect, Vector2 uv);
Vector2 RectangleTopLeft(Rectangle rect);
Vector2 RectangleTopMiddle(Rectangle rect);
Vector2 RectangleTopRight(Rectangle rect);
Vector2 RectangleCenterLeft(Rectangle rect);
Vector2 RectangleCenter(Rectangle rect);
Vector2 RectangleCenterRight(Rectangle rect);
Vector2 RectangleBottomLeft(Rectangle rect);
Vector2 RectangleBottomMiddle(Rectangle rect);
Vector2 RectangleBottomRight(Rectangle rect);
Vector2 RectangleExtents(Rectangle rect);
Rectangle RectangleFromExtents(float centerX, float centerY, float extentX, float extentY);
Rectangle RectangleFromExtentsV(Vector2 center, Vector2 extent);
Rectangle RectangleFromCenter(float centerX, float centerY, float width, float height);
Rectangle RectangleFromCenterV(Vector2 center, Vector2 size);
Rectangle RectangleFromMinMax(float minX, float minY, float maxX, float maxY);
Rectangle RectangleFromMinMaxV(Vector2 min, Vector2 max);
Rectangle ExpandRectangle(Rectangle rect, float amount);
Rectangle ExpandRectangleEx(Rectangle rect, float horizontal, float vertical);
Rectangle ExpandRectanglePro(Rectangle rect, float left, float right, float top, float bottom);
Rectangle MoveRectangle(Rectangle rect, float dx, float dy);

// Collision

bool CheckCollisionCircleRotatedRect(Vector2 position, float radius, Rectangle rect, Vector2 origin, float angle);
bool ResolveCollisionPointRect(Vector2 *position, Rectangle rect);
bool ResolveCollisionCircleRect(Vector2 *position, float radius, Rectangle rect);
Sweep SweepPointRect(Vector2 position, Vector2 velocity, Rectangle rect);
Sweep SweepPointCircle(Vector2 position, Vector2 velocity, Vector2 center, float radius);
Sweep SweepCircleRect(Vector2 position, Vector2 velocity, float radius, Rectangle rect);
Sweep SweepCircleCircle(Vector2 position, Vector2 velocity, float radius, Vector2 center, float circleRadius);

// Color

Color ScaleBrightness(Color color, float amount);
Color Brighter(Color color);
Color Darker(Color color);
Vector4 ColorToHsva(Color color);
Color ColorFromHSVA(Vector4 hsva);
Color Hue(float hue);
Color Grayscale(float intensity);
Color GrayscaleAlpha(float intensity, float alpha);
float Luminance(Color color);
Color ContrastingColor(Color color);
Color ContrastingColorBlackOrWhite(Color color);
Color ColorBlend(Color from, Color to, float amount);
Color ColorAlphaMultiply(Color color, float alpha);

// Noise

unsigned BitNoise3(int seed, int x, int y, int z);
unsigned BitNoise2(int seed, int x, int y);
unsigned BitNoise1(int seed, int x);
bool BoolNoise2(int seed, int x, int y);
bool BoolNoise1(int seed, int x);
int IntNoise2(int seed, int min, int max, int x, int y);
int IntNoise1(int seed, int min, int max, int x);
float FloatNoise2(int seed, float min, float max, int x, int y);
float FloatNoise1(int seed, float min, float max, int x);
float PerlinNoise2(int seed, float min, float max, float x, float y);
float PerlinNoise1(int seed, float min, float max, float x);

// Random

int RandomInt(int min, int max);
float RandomFloat(float min, float max);
float RandomNormal(float mean, float sdev);
Vector2 RandomVectorInCircle(float radius);
bool RandomBool(float probability);
int RandomSelect(float weights[], int count);
void RandomShuffleEx(void *items, int count, int size);

#define RandomShuffle(array) RandomShuffleEx((array), countof(array), sizeof (array)[0])

// Bezier

Vector2 CubicBezierPoint(Vector2 start, Vector2 control1, Vector2 control2, Vector2 end, float t);

// Allocator

void *Allocate(int count, int size);
void *AllocateNonZero(int count, int size);

// Ini

int LoadIniInt(char *key, int defaultValue);
bool LoadIniBool(char *key, bool defaultValue);
float LoadIniFloat(char *key, float defaultValue);
char *LoadIniString(char *key, char *defaultValue);
void SaveIniInt(char *key, int value);
void SaveIniBool(char *key, bool value);
void SaveIniFloat(char *key, float value);
void SaveIniString(char *key, char *value);

// Serialization

bool CanSerializeLoadFrom(char *path);
FILE *BeginSerialize(SerialOp op, char *path);
void EndSerialize(SerialOp op, FILE **file);
bool SerializeBytes(SerialOp op, FILE *file, void *bytes, int count);
#define Serialize(op, file, pValue) SerializeBytes((op), (file), (pValue), sizeof *(pValue))
#define SerializeArray(op, file, array) SerializeBytes((op), (file), (array), sizeof(array))

// Rendering

void DrawTextureCentered(Texture texture, int x, int y, Color tint);
void DrawTextureCenteredV(Texture texture, Vector2 center, Color tint);
void DrawTextureFlipped(Texture texture, int x, int y, bool flipX, bool flipY, Color tint);
void DrawTextureFlippedV(Texture texture, Vector2 position, bool flipX, bool flipY, Color tint);
void DrawTextureRotatedV(Texture texture, Vector2 position, float angle, Color tint);
void DrawTextureRotatedFlippedV(Texture texture, Vector2 position, float angle, bool flipX, bool flipY, Color tint);
void DrawAntiAliasedRectangle(Rectangle rect, Color color);
void rlColor(Color color);
void rlVertex2fv(Vector2 xy);

// Text

Vector2 MeasureTextSize(Font font, const char *text, float fontSize);
void DrawTextCentered(Font font, char *text, float x, float y, float fontSize, Color color);
void DrawTextCenteredV(Font font, char *text, Vector2 pos, float fontSize, Color color);
void DrawTextCenteredOffset(Font font, char *text, float x, float y, float fontSize, Color color1, Color color2);

// Console

void AddCommand(char *syntaxLiteral, bool(*callback)(char *args[], int count));
void ExecuteCommand(char *command);
bool FailCommand(_Printf_format_string_ char *reason, ...);

// Ui

void UiPushIdInt(int id);
void UiPushIdString(char *id);
void UiPushIdPointer(void *id);
void UiPopId(void);
bool UiBeginWindow(char *title, Rectangle bounds);
void UiEndWindow(void);
bool UiBeginChild(char *title);
void UiEndChild(void);
void UiBeginGroup(void);
void UiEndGroup(void);
bool UiBeginTreeNode(char *label);
void UiEndTreeNode(void);
bool UiBeginTooltip(void);
void UiEndTooltip(void);
bool UiHeader(char *label);
void UiLayoutEx(int height, int widths[], int count);
void UiSkip(void);
int UiAvailableWidth(void);
int UiAvailableHeight(void);
int UiWindowWidth(void);
int UiWindowHeight(void);
bool UiIsItemHovered(void);
void UiTooltip(_Printf_format_string_ char *format, ...);
void UiLabel(_Printf_format_string_ char *format, ...);
void UiLabelArgs(char *format, va_list args);
void UiColorLabel(Color color, _Printf_format_string_ char *format, ...);
void UiColorLabelArgs(Color color, char *format, va_list args);
void UiTexture(Texture tex);
bool UiButton(char *label);
bool UiRadio(char *label, int *value, int index);
bool UiColorButton(char *label, Color color);
bool UiTextureButton(char *label, Texture tex);
bool UiTextureRadio(int *value, int index, Texture tex);
bool UiCheckbox(bool *value);
bool UiSlider(float *value, float min, float max);
bool UiSliderInt(int *value, int min, int max);
bool UiSliderVector2(Vector2 *value, float min, float max);
bool UiSliderVector2Ex(Vector2 *value, Vector2 min, Vector2 max);
bool UiColorPicker(Color *value);
bool UiTextBoxEx(char buffer[], int capacity);
bool UiFilePickerEx(char buffer[], int capacity);
bool UiDirectoryPickerEx(char buffer[], int capacity);
bool UiConsole(Rectangle bounds);

#define UiLayout(height, width0, ...) UiLayoutEx((height), ((int[]) { (width0),##__VA_ARGS__ }), countof(((int[]) { (width0),##__VA_ARGS__ })))
#define UiTextBox(buffer) UiTextBoxEx((buffer), sizeof(buffer))
#define UiFilePicker(buffer) UiFilePickerEx((buffer), sizeof(buffer))
#define UiDirectoryPicker(buffer) UiDirectoryPickerEx((buffer), sizeof(buffer))

// List

#define List(type, capacity) struct { type items[capacity]; int count; }
#define ListAllocate(pList) ((pList)->count < countof((pList)->items) ? &(pList)->items[(pList)->count++] : NULL)
#define ListAdd(pList, item) (*ListAllocate((pList)) = (item))
#define ListPop(pList) ((pList)->items[(pList)->count > 0 ? --(pList)->count : (Warn("Tried to pop an empty list."), 0)])
#define ListPeek(list) ((list).items[(list).count > 0 ? (list).count - 1 : (Warn("Tried to peek an empty list."), 0)])
#define ListInsert(pList, item, index){\
	if ((pList)->count >= countof((pList)->items))\
		Warn("Tried to insert into a full list.");\
	else if ((index) < 0 or (index) > (pList)->count)\
		Warn("Insertion index was out of bounds.");\
	else\
	{\
		memmove(&(pList)->items[(index) + 1], &(pList)->items[(index)], (size_t)((pList)->count - (index)) * sizeof (pList)->items[0]);\
		(pList)->items[index] = (item);\
		(pList)->count++;\
	}\
}
#define ListRemove(pList, index){\
	if ((pList)->count <= 0)\
		Warn("Tried to remove from an empty list.");\
	else if ((index) < 0 or (index) >= (pList)->count)\
		Warn("Removal index was out of bounds.");\
	else\
	{\
		(pList)->count--;\
		memmove(&(pList)->items[index], &(pList)->items[(index) + 1], (size_t)((pList)->count - (index)) * sizeof (pList)->items[0]);\
	}\
}
#define ListSwapRemove(pList, index){\
	if ((pList)->count <= 0)\
		Warn("Tried to remove from an empty list.");\
	else if ((index) < 0 or (index) >= (pList)->count)\
		Warn("Removal index was out of bounds.");\
	else\
	{\
		(pList)->count--;\
		SwapBytes(&(pList)->items[index], &(pList)->items[(pList)->count], sizeof (pList)->items[0]);\
	}\
}

// Ring

#define Ring(type, capacity) struct { type items[capacity]; int head; int tail; }
#define RingAllocate(pRing) ((pRing)->head++, (pRing)->head %= countof((pRing)->items), ((pRing)->head == (pRing)->tail and ((pRing)->tail++, (pRing)->tail %= countof((pRing)->items))), &(pRing)->items[((pRing)->head ? ((pRing)->head - 1) : (countof((pRing)->items) - 1))])
#define RingAdd(pRing, item) (*RingAllocate((pRing)) = (item))
#define RingForeach(index, ring) for (int index = (ring).tail; index != (ring).head; index = (index + 1) % countof((ring).items))
#define RingClear(pRing) ((pRing)->head = (pRing)->tail = 0)

// Runtime

extern void Initialize(void);
extern void Update(void);
extern void UpdateUi(void);
