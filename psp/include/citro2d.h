/*
citro2d.h - citro2d's API on the PSP (psp/src/c2d_psp.c)

Same types and semantics as citro2d, so the game's drawing code is left as it
is. Sprite sheets are .psheet files (psp/tools/mkatlas.py) loaded under the
.t3x names the game asks for.
*/
#pragma once
#include <citro3d.h>
#include <tex3ds.h>

#ifdef __cplusplus
extern "C" {
#endif

#define C2D_DEFAULT_MAX_OBJECTS 4096

typedef struct {
	struct { float x, y, w, h; } pos;
	struct { float x, y; } center;
	float depth;
	float angle;
} C2D_DrawParams;

typedef enum { C2D_TopLeft, C2D_TopRight, C2D_BotLeft, C2D_BotRight } C2D_Corner;

typedef struct {
	C3D_Tex *tex;
	const Tex3DS_SubTexture *subtex;
} C2D_Image;

typedef struct {
	u32 color;
	float blend;
} C2D_Tint;

typedef struct {
	C2D_Tint corners[4];
} C2D_ImageTint;

typedef enum { C2D_TintSolid, C2D_TintMult, C2D_TintLuma } C2D_TintMode;

typedef struct C2D_SpriteSheet_s *C2D_SpriteSheet;

typedef struct {
	C2D_Image image;
	C2D_DrawParams params;
} C2D_Sprite;

/* ---- colors (same layout as the GE's: 0xAABBGGRR) ------------------- */

static inline u32 C2D_Color32(u8 r, u8 g, u8 b, u8 a)
{
	return r | (g << (u32)8) | (b << (u32)16) | (a << (u32)24);
}

static inline u32 C2D_Color32f(float r, float g, float b, float a)
{
	return C2D_Color32((u8)(r * 255), (u8)(g * 255), (u8)(b * 255), (u8)(a * 255));
}

/* ---- setup and frames ------------------------------------------------ */

bool C2D_Init(size_t maxObjects);
void C2D_Fini(void);
void C2D_Prepare(void);
void C2D_Flush(void);
void C2D_SceneSize(u32 width, u32 height, bool tilt);
void C2D_SceneTarget(C3D_RenderTarget *target);
static inline void C2D_SceneBegin(C3D_RenderTarget *target) { C2D_SceneTarget(target); }
void C2D_TargetClear(C3D_RenderTarget *target, u32 color);
void C2D_Fade(u32 color);
void C2D_SetTintMode(C2D_TintMode mode);
C3D_RenderTarget *C2D_CreateScreenTarget(gfxScreen_t screen, gfx3dSide_t side);

/* ---- view ------------------------------------------------------------ */

void C2D_ViewReset(void);
void C2D_ViewSave(C3D_Mtx *matrix);
void C2D_ViewRestore(const C3D_Mtx *matrix);
void C2D_ViewTranslate(float x, float y);
void C2D_ViewRotate(float rotation);
static inline void C2D_ViewRotateDegrees(float rotation) { C2D_ViewRotate(C3D_AngleFromDegrees(rotation)); }
void C2D_ViewShear(float x, float y);
void C2D_ViewScale(float x, float y);

/* ---- sprite sheets --------------------------------------------------- */

C2D_SpriteSheet C2D_SpriteSheetLoad(const char *filename);
C2D_SpriteSheet C2D_SpriteSheetLoadFromMem(const void *data, size_t size);
void C2D_SpriteSheetFree(C2D_SpriteSheet sheet);
size_t C2D_SpriteSheetCount(C2D_SpriteSheet sheet);
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet sheet, size_t index);

/* ---- tints ----------------------------------------------------------- */

static inline void C2D_SetTintColor(C2D_Tint *tint, u32 color, float blend) { tint->color = color; tint->blend = blend; }

static inline void C2D_SetImageTint(C2D_ImageTint *tint, C2D_Corner corner, u32 color, float blend)
{
	tint->corners[corner].color = color;
	tint->corners[corner].blend = blend;
}

static inline void C2D_PlainImageTint(C2D_ImageTint *tint, u32 color, float blend)
{
	for (int i = 0; i < 4; i++) C2D_SetImageTint(tint, (C2D_Corner)i, color, blend);
}

static inline void C2D_AlphaImageTint(C2D_ImageTint *tint, float alpha)
{
	C2D_PlainImageTint(tint, C2D_Color32f(0.0f, 0.0f, 0.0f, alpha), 0.0f);
}

static inline void C2D_TopImageTint(C2D_ImageTint *tint, u32 color, float blend)
{
	C2D_SetImageTint(tint, C2D_TopLeft, color, blend);
	C2D_SetImageTint(tint, C2D_TopRight, color, blend);
}

static inline void C2D_BottomImageTint(C2D_ImageTint *tint, u32 color, float blend)
{
	C2D_SetImageTint(tint, C2D_BotLeft, color, blend);
	C2D_SetImageTint(tint, C2D_BotRight, color, blend);
}

static inline void C2D_LeftImageTint(C2D_ImageTint *tint, u32 color, float blend)
{
	C2D_SetImageTint(tint, C2D_TopLeft, color, blend);
	C2D_SetImageTint(tint, C2D_BotLeft, color, blend);
}

static inline void C2D_RightImageTint(C2D_ImageTint *tint, u32 color, float blend)
{
	C2D_SetImageTint(tint, C2D_TopRight, color, blend);
	C2D_SetImageTint(tint, C2D_BotRight, color, blend);
}

/* ---- drawing --------------------------------------------------------- */

bool C2D_DrawImage(C2D_Image img, const C2D_DrawParams *params, const C2D_ImageTint *tint);

static inline bool C2D_DrawImageAt(C2D_Image img, float x, float y, float depth,
	const C2D_ImageTint *tint, float scaleX, float scaleY)
{
	C2D_DrawParams params = {
		{ x, y, scaleX * img.subtex->width, scaleY * img.subtex->height },
		{ 0.0f, 0.0f },
		depth, 0.0f
	};
	return C2D_DrawImage(img, &params, tint);
}

static inline bool C2D_DrawImageAtRotated(C2D_Image img, float x, float y, float depth, float angle,
	const C2D_ImageTint *tint, float scaleX, float scaleY)
{
	C2D_DrawParams params = {
		{ x, y, scaleX * img.subtex->width, scaleY * img.subtex->height },
		{ (scaleX * img.subtex->width) / 2.0f, (scaleY * img.subtex->height) / 2.0f },
		depth, angle
	};
	return C2D_DrawImage(img, &params, tint);
}

bool C2D_DrawTriangle(float x0, float y0, u32 clr0, float x1, float y1, u32 clr1,
	float x2, float y2, u32 clr2, float depth);
bool C2D_DrawLine(float x0, float y0, u32 clr0, float x1, float y1, u32 clr1, float thickness, float depth);
bool C2D_DrawRectangle(float x, float y, float z, float w, float h, u32 clr0, u32 clr1, u32 clr2, u32 clr3);
static inline bool C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 clr)
{
	return C2D_DrawRectangle(x, y, z, w, h, clr, clr, clr, clr);
}
bool C2D_DrawEllipse(float x, float y, float z, float w, float h, u32 clr0, u32 clr1, u32 clr2, u32 clr3);
static inline bool C2D_DrawEllipseSolid(float x, float y, float z, float w, float h, u32 clr)
{
	return C2D_DrawEllipse(x, y, z, w, h, clr, clr, clr, clr);
}
static inline bool C2D_DrawCircle(float x, float y, float z, float radius, u32 clr0, u32 clr1, u32 clr2, u32 clr3)
{
	return C2D_DrawEllipse(x - radius, y - radius, z, radius * 2, radius * 2, clr0, clr1, clr2, clr3);
}
static inline bool C2D_DrawCircleSolid(float x, float y, float z, float radius, u32 clr)
{
	return C2D_DrawCircle(x, y, z, radius, clr, clr, clr, clr);
}

/* ---- sprites (citro2d's own inline helpers) -------------------------- */

static inline void C2D_SpriteFromImage(C2D_Sprite *sprite, C2D_Image image)
{
	sprite->image           = image;
	sprite->params.pos.x    = 0.0f;
	sprite->params.pos.y    = 0.0f;
	sprite->params.pos.w    = image.subtex->width;
	sprite->params.pos.h    = image.subtex->height;
	sprite->params.center.x = 0.0f;
	sprite->params.center.y = 0.0f;
	sprite->params.angle    = 0.0f;
	sprite->params.depth    = 0.0f;
}

static inline void C2D_SpriteFromSheet(C2D_Sprite *sprite, C2D_SpriteSheet sheet, size_t index)
{
	C2D_SpriteFromImage(sprite, C2D_SpriteSheetGetImage(sheet, index));
}

static inline void C2D_SpriteScale(C2D_Sprite *sprite, float x, float y)
{
	sprite->params.pos.w *= x;
	sprite->params.pos.h *= y;
	sprite->params.center.x *= x;
	sprite->params.center.y *= y;
}

static inline void C2D_SpriteRotate(C2D_Sprite *sprite, float radians) { sprite->params.angle += radians; }
static inline void C2D_SpriteRotateDegrees(C2D_Sprite *sprite, float degrees) { C2D_SpriteRotate(sprite, C3D_AngleFromDegrees(degrees)); }

static inline void C2D_SpriteMove(C2D_Sprite *sprite, float x, float y)
{
	sprite->params.pos.x += x;
	sprite->params.pos.y += y;
}

static inline void C2D_SpriteSetScale(C2D_Sprite *sprite, float x, float y)
{
	float oldCenterX = sprite->params.center.x / sprite->params.pos.w;
	float oldCenterY = sprite->params.center.y / sprite->params.pos.h;
	sprite->params.pos.w = x * sprite->image.subtex->width;
	sprite->params.pos.h = y * sprite->image.subtex->height;
	sprite->params.center.x = fabsf(oldCenterX * sprite->params.pos.w);
	sprite->params.center.y = fabsf(oldCenterY * sprite->params.pos.h);
}

static inline void C2D_SpriteSetRotation(C2D_Sprite *sprite, float radians) { sprite->params.angle = radians; }
static inline void C2D_SpriteSetRotationDegrees(C2D_Sprite *sprite, float degrees) { C2D_SpriteSetRotation(sprite, C3D_AngleFromDegrees(degrees)); }

static inline void C2D_SpriteSetCenter(C2D_Sprite *sprite, float x, float y)
{
	sprite->params.center.x = x * fabsf(sprite->params.pos.w);
	sprite->params.center.y = y * fabsf(sprite->params.pos.h);
}

static inline void C2D_SpriteSetCenterRaw(C2D_Sprite *sprite, float x, float y)
{
	sprite->params.center.x = x;
	sprite->params.center.y = y;
}

static inline void C2D_SpriteSetPos(C2D_Sprite *sprite, float x, float y)
{
	sprite->params.pos.x = x;
	sprite->params.pos.y = y;
}

static inline void C2D_SpriteSetDepth(C2D_Sprite *sprite, float depth) { sprite->params.depth = depth; }

static inline bool C2D_DrawSprite(const C2D_Sprite *sprite)
{
	return C2D_DrawImage(sprite->image, &sprite->params, NULL);
}

static inline bool C2D_DrawSpriteTinted(const C2D_Sprite *sprite, const C2D_ImageTint *tint)
{
	return C2D_DrawImage(sprite->image, &sprite->params, tint);
}

#ifdef __cplusplus
}
#endif
