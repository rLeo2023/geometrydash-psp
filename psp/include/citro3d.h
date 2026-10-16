/*
citro3d.h - the few citro3d names the game uses, backed by the PSP GE
(psp/src/c2d_psp.c). Only what the game touches is here: textures, the view
matrix type, blending, scissor and the frame calls.
*/
#pragma once
#include <3ds.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_TAU
#define M_TAU (2 * M_PI)
#endif
#define C3D_AngleFromDegrees(_angle) ((_angle) * M_TAU / 360.0f)

#define C3D_DEFAULT_CMDBUF_SIZE 0x40000
enum { C3D_FRAME_SYNCDRAW = BIT(0), C3D_FRAME_NONBLOCK = BIT(1) };

// citro3d's vectors keep their components backwards
typedef union {
	struct { float w, z, y, x; };
	struct { float r, k, j, i; };
	float c[4];
} C3D_FVec;

typedef union {
	C3D_FVec r[4];
	float m[4 * 4];
} C3D_Mtx;

typedef enum { GPU_NEAREST = 0, GPU_LINEAR = 1 } GPU_TEXTURE_FILTER_PARAM;
typedef enum { GPU_CLAMP_TO_EDGE = 0, GPU_CLAMP_TO_BORDER = 1, GPU_REPEAT = 2, GPU_MIRRORED_REPEAT = 3 } GPU_TEXTURE_WRAP_PARAM;

typedef enum {
	GPU_ZERO = 0, GPU_ONE, GPU_SRC_COLOR, GPU_ONE_MINUS_SRC_COLOR, GPU_DST_COLOR, GPU_ONE_MINUS_DST_COLOR,
	GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_DST_ALPHA, GPU_ONE_MINUS_DST_ALPHA,
	GPU_CONSTANT_COLOR, GPU_ONE_MINUS_CONSTANT_COLOR, GPU_CONSTANT_ALPHA, GPU_ONE_MINUS_CONSTANT_ALPHA,
	GPU_SRC_ALPHA_SATURATE,
} GPU_BLENDFACTOR;
typedef enum { GPU_BLEND_ADD = 0, GPU_BLEND_SUBTRACT, GPU_BLEND_REVERSE_SUBTRACT, GPU_BLEND_MIN, GPU_BLEND_MAX } GPU_BLENDEQUATION;

typedef enum { GPU_SCISSOR_DISABLE = 0, GPU_SCISSOR_INVERT = 1, GPU_SCISSOR_NORMAL = 3 } GPU_SCISSORMODE;

typedef enum { GPU_RB_RGBA8 = 0, GPU_RB_RGB8, GPU_RB_RGBA5551, GPU_RB_RGB565, GPU_RB_RGBA4 } GPU_COLORBUF;
typedef enum { GPU_RB_DEPTH16 = 0, GPU_RB_DEPTH24 = 2, GPU_RB_DEPTH24_STENCIL8 = 3 } GPU_DEPTHBUF;

// texture combiner names: accepted and ignored, the PSP has one fixed stage
typedef enum {
	GPU_PRIMARY_COLOR = 0x00, GPU_FRAGMENT_PRIMARY_COLOR = 0x01, GPU_FRAGMENT_SECONDARY_COLOR = 0x02,
	GPU_TEXTURE0 = 0x03, GPU_TEXTURE1 = 0x04, GPU_TEXTURE2 = 0x05, GPU_TEXTURE3 = 0x06,
	GPU_PREVIOUS_BUFFER = 0x0D, GPU_CONSTANT = 0x0E, GPU_PREVIOUS = 0x0F,
} GPU_TEVSRC;
typedef enum {
	GPU_REPLACE = 0x00, GPU_MODULATE = 0x01, GPU_ADD = 0x02, GPU_ADD_SIGNED = 0x03,
	GPU_INTERPOLATE = 0x04, GPU_SUBTRACT = 0x05, GPU_DOT3_RGB = 0x06, GPU_DOT3_RGBA = 0x07,
	GPU_MULTIPLY_ADD = 0x08, GPU_ADD_MULTIPLY = 0x09,
} GPU_COMBINEFUNC;
typedef enum { C3D_RGB = BIT(0), C3D_Alpha = BIT(1), C3D_Both = C3D_RGB | C3D_Alpha } C3D_TexEnvMode;

typedef struct { u16 srcRgb, srcAlpha; u32 opAll; u16 funcRgb, funcAlpha; u32 color; u16 scaleRgb, scaleAlpha; } C3D_TexEnv;
C3D_TexEnv *C3D_GetTexEnv(int id);
void C3D_TexEnvInit(C3D_TexEnv *env);
void C3D_TexEnvSrc(C3D_TexEnv *env, C3D_TexEnvMode mode, int s1, int s2, int s3);
void C3D_TexEnvFunc(C3D_TexEnv *env, C3D_TexEnvMode mode, int param);

// a texture page in RAM (psp/tools/mkatlas.py), drawn by the GE from there
typedef struct C3D_Tex {
	void *data;
	void *clut;        // T8 pages: 256 RGBA entries
	u16 width, height; // power of two
	u8 psm;            // GU_PSM_*
	u8 filter;         // GPU_NEAREST / GPU_LINEAR
	u8 swizzled;       // stored in the GE's 16x8 blocks
	u8 in_vram;        // data is in VRAM (kept for good, never freed)
} C3D_Tex;
void C3D_TexSetFilter(C3D_Tex *tex, GPU_TEXTURE_FILTER_PARAM magFilter, GPU_TEXTURE_FILTER_PARAM minFilter);

// a 3DS screen: the PSP draws one or both of them into its own frame
typedef struct C3D_RenderTarget_tag {
	int screen;        // GFX_TOP / GFX_BOTTOM, -1 offscreen
	int side;
	int width, height; // in the game's coordinates (400x240 / 320x240)
	u32 clearColor;
	bool clear;
} C3D_RenderTarget;

C3D_RenderTarget *C3D_RenderTargetCreate(int width, int height, GPU_COLORBUF colorFmt, GPU_DEPTHBUF depthFmt);
void C3D_RenderTargetDelete(C3D_RenderTarget *target);
void C3D_RenderTargetSetOutput(C3D_RenderTarget *target, gfxScreen_t screen, gfx3dSide_t side, u32 transferFlags);

bool C3D_Init(size_t cmdBufSize);
void C3D_Fini(void);
bool C3D_FrameBegin(u8 flags);
void C3D_FrameEnd(u8 flags);
void C3D_AlphaBlend(GPU_BLENDEQUATION colorEq, GPU_BLENDEQUATION alphaEq,
	GPU_BLENDFACTOR srcClr, GPU_BLENDFACTOR dstClr, GPU_BLENDFACTOR srcAlpha, GPU_BLENDFACTOR dstAlpha);
void C3D_SetScissor(GPU_SCISSORMODE mode, u32 left, u32 top, u32 right, u32 bottom);

float C3D_GetProcessingTime(void);
float C3D_GetDrawingTime(void);
float C3D_GetCmdBufUsage(void);

// only named by c2d_internal.h's structures
typedef struct { int dummy; } DVLB_s;
typedef struct { int dummy; } shaderProgram_s;
typedef struct { int dummy; } C3D_AttrInfo;
typedef struct { int dummy; } C3D_BufInfo;
typedef struct { int dummy; } C3D_ProcTex;
typedef struct { int dummy; } C3D_ProcTexLut;
typedef struct { int dummy; } CFNT_s;

#ifdef __cplusplus
}
#endif
