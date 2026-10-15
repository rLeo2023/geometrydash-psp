/*
tex3ds.h - Tex3DS_SubTexture, as citro2d uses it

Texture coordinates go 0..1; "top" is the bigger v, as the 3DS keeps its
textures upside down. The PSP renderer flips v back (psp/src/c2d_psp.c), so
the game's own sub-rectangle math (utils/gfx.c select_box) still works.
*/
#pragma once
#include <3ds.h>

typedef struct {
	u16 width;
	u16 height;
	float left;
	float top;
	float right;
	float bottom;
} Tex3DS_SubTexture;

static inline bool Tex3DS_SubTextureRotated(const Tex3DS_SubTexture *subtex)
{
	return subtex->top < subtex->bottom;
}

static inline void Tex3DS_SubTextureTopLeft(const Tex3DS_SubTexture *subtex, float *u, float *v)
{
	if (!Tex3DS_SubTextureRotated(subtex)) { *u = subtex->left; *v = subtex->top; }
	else { *u = subtex->top; *v = subtex->left; }
}

static inline void Tex3DS_SubTextureTopRight(const Tex3DS_SubTexture *subtex, float *u, float *v)
{
	if (!Tex3DS_SubTextureRotated(subtex)) { *u = subtex->right; *v = subtex->top; }
	else { *u = subtex->top; *v = subtex->right; }
}

static inline void Tex3DS_SubTextureBottomLeft(const Tex3DS_SubTexture *subtex, float *u, float *v)
{
	if (!Tex3DS_SubTextureRotated(subtex)) { *u = subtex->left; *v = subtex->bottom; }
	else { *u = subtex->bottom; *v = subtex->left; }
}

static inline void Tex3DS_SubTextureBottomRight(const Tex3DS_SubTexture *subtex, float *u, float *v)
{
	if (!Tex3DS_SubTextureRotated(subtex)) { *u = subtex->right; *v = subtex->bottom; }
	else { *u = subtex->bottom; *v = subtex->right; }
}
