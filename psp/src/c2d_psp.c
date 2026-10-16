/*
c2d_psp.c - citro2d on the PSP GE

The game draws two 3DS screens per frame: the top one (400x240) and the touch
screen (320x240). The PSP shows them one at a time, scaled to its 480x272, or
both side by side (psp_view, chosen in ctru_psp.c).

Everything is transformed on the CPU as it is appended (the view matrix,
then the screen's place on the PSP), so the GE gets plain 2D vertices
(GU_TRANSFORM_2D) and the batch only breaks when the texture, the mode or
the blending changes, like citro2d's.

The game uses citro2d's internals too (utils/gfx.c, through
utils/c2d_internal.h), so the batching follows that header's context.
*/
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include <citro2d.h>
#include "../../source/utils/c2d_internal.h"
#include "psp_port.h"
#include "main.h"           // game_state, game_paused, in_level_complete
#include "menus/settings.h" // settingsState.fps60

#define PSP_SCR_W   480
#define PSP_SCR_H   272
#define PSP_BUF_W   512
#define FB_SIZE     ( PSP_BUF_W * PSP_SCR_H * 4 )

// one frame's commands and vertices; a frame that needs more is cut in two.
// A heavy official level peaks around 100 KB ("peak: display list")
#define LIST_SIZE   ( 512 * 1024 )

#define VTX_MAX     6144
#define IDX_MAX     ( VTX_MAX * 3 / 2 )

typedef struct {
	float u, v;
	u32 color;
	float x, y, z;
} PspVertex;
#define PSP_VTYPE ( GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D )

C2Di_Context __C2Di_Context;

static unsigned int __attribute__(( aligned( 16 ))) gu_list[LIST_SIZE / 4];
static size_t list_used;      // bytes handed out by sceGuGetMemory this list
static bool in_frame;

static C2Di_Vertex vtx_buf[VTX_MAX];
static u16 idx_buf[IDX_MAX];

// the view (citro2d's model view), 2D affine: x' = a x + b y + tx
typedef struct { float a, b, c, d, tx, ty; } Affine;
static Affine view;

// where the current target lands on the PSP screen
// (full: the target may draw anywhere on the PSP screen, not just its own rectangle)
typedef struct { bool visible; float sx, sy, ox, oy; int w, h; bool full; } Placement;

// merged view: where the touch screen sits on the top one (top screen units)
static float merge_dx = 40, merge_dy = 0, merge_k = 1;
static C3D_RenderTarget *cur_target;
static Placement place;

static u32 fade_color;
static int tint_mode = C2D_TintMult;

// the game's scissor, in the target's own coordinates
static bool game_scissor;
static int sc_x0, sc_y0, sc_x1, sc_y1;

static C3D_TexEnv tex_envs[6];

int psp_view = PSP_VIEW_TOP;

/*
==============================================================================

Placement of the 3DS screens

==============================================================================
*/
static Placement PSP_Place( const C3D_RenderTarget *t )
{
	Placement p = { false, 1, 1, 0, 0, 0, 0, false };

	if( !t || t->screen < 0 )
		return p;

	p.w = t->width;
	p.h = t->height;

	switch( psp_view )
	{
	case PSP_VIEW_MERGED:
		// one screen, like the phone game: the top screen stretched to the
		// PSP's, the touch screen's contents laid over it (drawn last, see
		// the deferred list below)
		p.sx = p.sy = (float)PSP_SCR_W / GSP_SCREEN_HEIGHT_TOP;
		p.oy = ( PSP_SCR_H - GSP_SCREEN_WIDTH * p.sy ) * 0.5f;
		if( t->screen == GFX_BOTTOM )
		{
			p.ox = merge_dx * p.sx;
			p.oy += merge_dy * p.sy;
			p.sx *= merge_k;
			p.sy *= merge_k;
			p.full = true;
		}
		break;
	case PSP_VIEW_TOP:
		if( t->screen != GFX_TOP )
			return p;
		// the whole PSP width, the same scale both ways (stretching looked
		// off): 400x240 -> 480x288, 8 lines cut at the top and the bottom
		p.sx = p.sy = (float)PSP_SCR_W / t->width;
		p.oy = ( PSP_SCR_H - t->height * p.sy ) * 0.5f;
		break;
	case PSP_VIEW_BOTTOM:
		if( t->screen != GFX_BOTTOM )
			return p;
		// the touch screen is 4:3: stretching it would be too much
		p.sx = p.sy = (float)PSP_SCR_H / t->height;
		p.ox = ( PSP_SCR_W - t->width * p.sx ) * 0.5f;
		break;
	default: // both, side by side: 400 + 320 wide
		p.sx = p.sy = (float)PSP_SCR_W / ( GSP_SCREEN_HEIGHT_TOP + GSP_SCREEN_HEIGHT_BOTTOM );
		p.ox = t->screen == GFX_TOP ? 0 : GSP_SCREEN_HEIGHT_TOP * p.sx;
		p.oy = ( PSP_SCR_H - GSP_SCREEN_WIDTH * p.sy ) * 0.5f;
		break;
	}

	p.visible = true;
	return p;
}

// a target's rectangle on the PSP screen, clipped to the screen
static void PSP_TargetRect( const Placement *p, int *x0, int *y0, int *x1, int *y1 )
{
	if( p->full )
	{
		*x0 = *y0 = 0;
		*x1 = PSP_SCR_W;
		*y1 = PSP_SCR_H;
		return;
	}

	*x0 = (int)( p->ox );
	*y0 = (int)( p->oy );
	*x1 = (int)( p->ox + p->w * p->sx + 0.5f );
	*y1 = (int)( p->oy + p->h * p->sy + 0.5f );
	if( *x0 < 0 ) *x0 = 0;
	if( *y0 < 0 ) *y0 = 0;
	if( *x1 > PSP_SCR_W ) *x1 = PSP_SCR_W;
	if( *y1 > PSP_SCR_H ) *y1 = PSP_SCR_H;
}

/*
==============================================================================

GE commands, now or later

In the merged view the touch screen is drawn after everything else, whatever
order the game draws its two screens in: its commands are kept here while
it is the target, and sent at the end of the frame. Only the few state
changes this file makes are needed: texture, blending, scissor, vertices.

==============================================================================
*/
enum { OP_TEX, OP_BLEND, OP_SCISSOR, OP_DRAW };

typedef struct {
	u8 type;
	u8 prim;
	u16 pad;
	union {
		C3D_Tex *tex;
		struct { int op, s, d; unsigned sf, df; } blend;
		struct { short x0, y0, x1, y1; } sc;
		struct { int first, count; } draw;
	};
} GeOp;

// the touch screen's half of a menu: the icon kit, the busiest, is about 100
// ops and 500 vertices ("peak: deferred" in the log)
#define DEFER_OPS   1024
#define DEFER_VERTS 4096

static GeOp defer_ops[DEFER_OPS];
static int defer_nops;
static PspVertex defer_verts[DEFER_VERTS];
static int defer_nverts;
static bool deferring;

// the blending the game asked for last, to put back after a deferred scene
static struct { int op, s, d; unsigned sf, df; } cur_blend = { GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 };

static void *PSP_GetMemory( size_t size );
static void PSP_SetTexture( C3D_Tex *tex );

static GeOp *GE_Op( int type )
{
	if( defer_nops >= DEFER_OPS )
		return NULL;
	defer_ops[defer_nops].type = type;
	return &defer_ops[defer_nops++];
}

static void GE_Tex( C3D_Tex *tex )
{
	GeOp *op;

	if( !deferring )
	{
		PSP_SetTexture( tex );
		return;
	}
	if(( op = GE_Op( OP_TEX )))
		op->tex = tex;
}

static void GE_Blend( int o, int s, int d, unsigned sf, unsigned df )
{
	GeOp *op;

	if( !deferring )
	{
		sceGuBlendFunc( o, s, d, sf, df );
		return;
	}
	if(( op = GE_Op( OP_BLEND )))
	{
		op->blend.op = o; op->blend.s = s; op->blend.d = d;
		op->blend.sf = sf; op->blend.df = df;
	}
}

// this pspsdk's sceGuScissor takes a width and a height, not the far corner
static void PSP_Scissor( int x0, int y0, int x1, int y1 )
{
	GeOp *op;

	if( !deferring )
	{
		sceGuScissor( x0, y0, x1 - x0, y1 - y0 );
		return;
	}
	if(( op = GE_Op( OP_SCISSOR )))
	{
		op->sc.x0 = x0; op->sc.y0 = y0;
		op->sc.x1 = x1; op->sc.y1 = y1;
	}
}

// room for n vertices, sent by GE_DrawEnd
static PspVertex *GE_DrawBegin( int n )
{
	if( !deferring )
		return PSP_GetMemory( n * sizeof( PspVertex ));
	if( defer_nverts + n > DEFER_VERTS )
	{
		static bool told;
		if( !told ) psp_log( "deferred vertices full: some of the menu not drawn\n" );
		told = true;
		return NULL;
	}
	return &defer_verts[defer_nverts];
}

static void GE_DrawEnd( int prim, PspVertex *v, int n )
{
	GeOp *op;

	if( !v )
		return;
	if( !deferring )
	{
		sceGuDrawArray( prim, PSP_VTYPE, n, 0, v );
		return;
	}
	if(( op = GE_Op( OP_DRAW )))
	{
		op->prim = prim;
		op->draw.first = defer_nverts;
		op->draw.count = n;
		defer_nverts += n;
	}
}

// the deferred scene starts with the blending in force at that point
static void GE_BeginDeferred( void )
{
	deferring = true;
	GE_Blend( cur_blend.op, cur_blend.s, cur_blend.d, cur_blend.sf, cur_blend.df );
}

static void GE_EndDeferred( void )
{
	deferring = false;
	sceGuBlendFunc( cur_blend.op, cur_blend.s, cur_blend.d, cur_blend.sf, cur_blend.df );
}

static void GE_Replay( void )
{
	int i;

	for( i = 0; i < defer_nops; i++ )
	{
		GeOp *op = &defer_ops[i];
		PspVertex *v;

		switch( op->type )
		{
		case OP_TEX:
			PSP_SetTexture( op->tex );
			break;
		case OP_BLEND:
			sceGuBlendFunc( op->blend.op, op->blend.s, op->blend.d, op->blend.sf, op->blend.df );
			break;
		case OP_SCISSOR:
			sceGuScissor( op->sc.x0, op->sc.y0, op->sc.x1 - op->sc.x0, op->sc.y1 - op->sc.y0 );
			break;
		case OP_DRAW:
			v = PSP_GetMemory( op->draw.count * sizeof( PspVertex ));
			memcpy( v, &defer_verts[op->draw.first], op->draw.count * sizeof( PspVertex ));
			sceGuDrawArray( op->prim, PSP_VTYPE, op->draw.count, 0, v );
			break;
		}
	}
	defer_nops = defer_nverts = 0;
	sceGuBlendFunc( cur_blend.op, cur_blend.s, cur_blend.d, cur_blend.sf, cur_blend.df );
}

bool psp_merged_bottom_now( void )
{
	return deferring;
}

void psp_set_merge( float dx, float dy, float k )
{
	merge_dx = dx;
	merge_dy = dy;
	merge_k = k > 0 ? k : 1;
}

void psp_get_merge( float *dx, float *dy, float *k )
{
	*dx = merge_dx;
	*dy = merge_dy;
	*k = merge_k;
}

static void PSP_ApplyScissor( void )
{
	int x0, y0, x1, y1;

	if( !place.visible )
		return;

	PSP_TargetRect( &place, &x0, &y0, &x1, &y1 );

	if( game_scissor )
	{
		int gx0 = (int)( place.ox + sc_x0 * place.sx );
		int gy0 = (int)( place.oy + sc_y0 * place.sy );
		int gx1 = (int)( place.ox + sc_x1 * place.sx + 0.5f );
		int gy1 = (int)( place.oy + sc_y1 * place.sy + 0.5f );
		if( gx0 > x0 ) x0 = gx0;
		if( gy0 > y0 ) y0 = gy0;
		if( gx1 < x1 ) x1 = gx1;
		if( gy1 < y1 ) y1 = gy1;
		if( x1 < x0 ) x1 = x0;
		if( y1 < y0 ) y1 = y0;
	}

	PSP_Scissor( x0, y0, x1, y1 );
}

/*
==============================================================================

Display list

==============================================================================
*/
static void PSP_ListStart( void )
{
	sceGuStart( GU_DIRECT, gu_list );
	list_used = 0;
}

static void *PSP_GetMemory( size_t size )
{
	// what the list really holds, commands too (a frame of many small
	// sprites is mostly commands): counting only the vertices let a heavy
	// level run past the end of the list, over whatever came after it.
	// The margin is for the state commands sent before the next vertices
	if( sceGuCheckList() + size + 16 * 1024 > LIST_SIZE )
	{
		static int flushes;
		if(( flushes++ & 255 ) == 0 )
			psp_log( "display list full: sent early (%d times)\n", flushes );
		sceGuFinish();
		sceGuSync( 0, 0 );
		PSP_ListStart();
	}
	list_used += size + 16;
	return sceGuGetMemory( size );
}

/*
==============================================================================

citro2d internals (utils/c2d_internal.h)

==============================================================================
*/
// what the GE has now, so switching back and forth costs nothing when it
// didn't change (the game sets the texture for nearly every sprite).
// ge_tex_valid is cleared when a new frame starts
static C3D_Tex *ge_tex;
static void *ge_clut;
static u8 ge_filter;
static bool ge_tex_valid;

static void PSP_SetTexture( C3D_Tex *tex )
{
	if( ge_tex_valid && tex == ge_tex && ( !tex || tex->filter == ge_filter ))
		return;
	ge_tex = tex;
	ge_tex_valid = true;

	if( !tex )
	{
		sceGuDisable( GU_TEXTURE_2D );
		return;
	}
	ge_filter = tex->filter;

	sceGuEnable( GU_TEXTURE_2D );
	if( tex->psm == GU_PSM_T8 && tex->clut != ge_clut )
	{
		// 1 KB read by the GE each time: only when the page changes
		sceGuClutMode( GU_PSM_8888, 0, 0xff, 0 );
		sceGuClutLoad( 256 / 8, tex->clut );
		ge_clut = tex->clut;
	}
	sceGuTexMode( tex->psm, 0, 0, tex->swizzled );
	sceGuTexImage( 0, tex->width, tex->height, tex->width, tex->data );
	sceGuTexFunc( GU_TFX_MODULATE, GU_TCC_RGBA );
	sceGuTexFilter( tex->filter == GPU_LINEAR ? GU_LINEAR : GU_NEAREST,
		tex->filter == GPU_LINEAR ? GU_LINEAR : GU_NEAREST );
	sceGuTexWrap( GU_CLAMP, GU_CLAMP );
}

static bool PSP_ModeTextured( u32 flags )
{
	return ( flags & C2DiF_Mode_Mask ) >= C2DiF_Mode_ImageSolid;
}

/*
A quad from C2Di_AppendQuad (indices a, a+1, a+2, a+2, a+1, a+3: top left,
top right, bottom left, bottom right) that is an upright rectangle, in
position and texture, with one color: the GE draws it from two corners
(GU_SPRITES), a third of the vertices of its two triangles.
*/
static bool PSP_IsSprite( const C2Di_Context *ctx, size_t i )
{
	const u16 *x = &ctx->idxBuf[i];
	const C2Di_Vertex *tl, *tr, *bl, *br;

	if( x[1] != x[0] + 1 || x[2] != x[0] + 2 || x[3] != x[2] || x[4] != x[1] || x[5] != x[0] + 3 )
		return false;
	tl = &ctx->vtxBuf[x[0]]; tr = &ctx->vtxBuf[x[0] + 1];
	bl = &ctx->vtxBuf[x[0] + 2]; br = &ctx->vtxBuf[x[0] + 3];

	return tl->pos[1] == tr->pos[1] && bl->pos[1] == br->pos[1]
		&& tl->pos[0] == bl->pos[0] && tr->pos[0] == br->pos[0]
		&& tl->pos[0] < br->pos[0] && tl->pos[1] < br->pos[1]
		&& tl->texcoord[1] == tr->texcoord[1] && bl->texcoord[1] == br->texcoord[1]
		&& tl->texcoord[0] == bl->texcoord[0] && tr->texcoord[0] == br->texcoord[0]
		&& tl->color == tr->color && tl->color == bl->color && tl->color == br->color;
}

static void PSP_PutVertex( PspVertex *out, const C2Di_Vertex *v )
{
	out->u = v->texcoord[0];
	out->v = v->texcoord[1];
	out->color = v->color;
	out->x = v->pos[0];
	out->y = v->pos[1];
	out->z = 0;
}

void C2Di_FlushVtxBuf( void )
{
	C2Di_Context *ctx = C2Di_GetContext();
	size_t n = ctx->idxBufPos;

	// in runs, in the order drawn: upright quads as sprites, the rest
	// (rotated, gradients, triangles) as triangles
	size_t i = 0;
	while( n && in_frame && place.visible && i < n )
	{
		PspVertex *out;
		size_t j = i, k;

		if( i + 6 <= n && PSP_IsSprite( ctx, i ))
		{
			while( j + 6 <= n && PSP_IsSprite( ctx, j ))
				j += 6;
			// two corners per quad: top left, bottom right
			out = GE_DrawBegin(( j - i ) / 3 );
			for( k = i; out && k < j; k += 6 )
			{
				PSP_PutVertex( &out[( k - i ) / 3], &ctx->vtxBuf[ctx->idxBuf[k]] );
				PSP_PutVertex( &out[( k - i ) / 3 + 1], &ctx->vtxBuf[ctx->idxBuf[k + 5]] );
			}
			GE_DrawEnd( GU_SPRITES, out, ( j - i ) / 3 );
		}
		else
		{
			do
				j += 3;
			while( j < n && !( j + 6 <= n && PSP_IsSprite( ctx, j )));
			if( j > n )
				j = n;
			out = GE_DrawBegin( j - i );
			for( k = i; out && k < j; k++ )
				PSP_PutVertex( &out[k - i], &ctx->vtxBuf[ctx->idxBuf[k]] );
			GE_DrawEnd( GU_TRIANGLES, out, j - i );
		}
		i = j;
	}

	ctx->idxBufPos = ctx->idxBufLastPos = 0;
	ctx->vtxBufPos = 0;
}

void C2Di_Update( void )
{
	C2Di_Context *ctx = C2Di_GetContext();

	if( !( ctx->flags & ( C2DiF_DirtyTex | C2DiF_DirtyMode )))
		return;

	C2Di_FlushVtxBuf();

	if( in_frame )
		GE_Tex( PSP_ModeTextured( ctx->flags ) ? ctx->curTex : NULL );

	ctx->flags &= ~( C2DiF_DirtyTex | C2DiF_DirtyMode | C2DiF_DirtyProj | C2DiF_DirtyMdlv | C2DiF_DirtyFade );
}

void C2Di_CalcQuad( C2Di_Quad *quad, const C2D_DrawParams *params )
{
	const float w = fabsf( params->pos.w );
	const float h = fabsf( params->pos.h );

	quad->topLeft[0]  = -params->center.x;
	quad->topLeft[1]  = -params->center.y;
	quad->topRight[0] = -params->center.x + w;
	quad->topRight[1] = -params->center.y;
	quad->botLeft[0]  = -params->center.x;
	quad->botLeft[1]  = -params->center.y + h;
	quad->botRight[0] = -params->center.x + w;
	quad->botRight[1] = -params->center.y + h;

	if( params->angle != 0.0f )
	{
		float c, s;
		psp_sincosf( params->angle, &s, &c );
#define ROTATE( _v ) do { \
			float a = _v[0] * c - _v[1] * s; \
			float b = _v[1] * c + _v[0] * s; \
			_v[0] = a; \
			_v[1] = b; \
		} while( 0 )
		ROTATE( quad->topLeft );
		ROTATE( quad->topRight );
		ROTATE( quad->botLeft );
		ROTATE( quad->botRight );
#undef ROTATE
	}

	quad->topLeft[0]  += params->pos.x;
	quad->topLeft[1]  += params->pos.y;
	quad->topRight[0] += params->pos.x;
	quad->topRight[1] += params->pos.y;
	quad->botLeft[0]  += params->pos.x;
	quad->botLeft[1]  += params->pos.y;
	quad->botRight[0] += params->pos.x;
	quad->botRight[1] += params->pos.y;
}

void C2Di_AppendTri( void )
{
	C2Di_Context *ctx = C2Di_GetContext();
	u16 v = ctx->vtxBufPos;

	ctx->idxBuf[ctx->idxBufPos++] = v + 0;
	ctx->idxBuf[ctx->idxBufPos++] = v + 1;
	ctx->idxBuf[ctx->idxBufPos++] = v + 2;
}

void C2Di_AppendQuad( void )
{
	C2Di_Context *ctx = C2Di_GetContext();
	u16 v = ctx->vtxBufPos;

	ctx->idxBuf[ctx->idxBufPos++] = v + 0;
	ctx->idxBuf[ctx->idxBufPos++] = v + 1;
	ctx->idxBuf[ctx->idxBufPos++] = v + 2;
	ctx->idxBuf[ctx->idxBufPos++] = v + 2;
	ctx->idxBuf[ctx->idxBufPos++] = v + 1;
	ctx->idxBuf[ctx->idxBufPos++] = v + 3;
}

/*
The vertex goes in finished: through the view, onto its place on the PSP
screen, the texture coordinates in texels (the 2D mode wants them so) and the
color with the tint's blend already applied. citro2d's "mult" tint is
texture * mix(white, color, blend), which is the GE's modulate with that
mixed color.
*/
void C2Di_AppendVtx( float x, float y, float z, float u, float v, float ptx, float pty, u32 color )
{
	C2Di_Context *ctx = C2Di_GetContext();
	C2Di_Vertex *vtx = &ctx->vtxBuf[ctx->vtxBufPos++];
	float vx = view.a * x + view.b * y + view.tx;
	float vy = view.c * x + view.d * y + view.ty;

	(void)z;
	(void)ptx;

	vtx->pos[0] = place.ox + vx * place.sx;
	vtx->pos[1] = place.oy + vy * place.sy;
	vtx->pos[2] = 0;

	if( PSP_ModeTextured( ctx->flags ) && ctx->curTex )
	{
		vtx->texcoord[0] = u * ctx->curTex->width;
		vtx->texcoord[1] = ( 1.0f - v ) * ctx->curTex->height;

		if( pty < 1.0f )
		{
			// blend 0 leaves the texture as it is, 1 multiplies by the color
			u32 r = color & 0xff, g = ( color >> 8 ) & 0xff, b = ( color >> 16 ) & 0xff;
			float k = pty < 0.0f ? 0.0f : pty;
			r = (u32)( 255.0f - ( 255.0f - r ) * k );
			g = (u32)( 255.0f - ( 255.0f - g ) * k );
			b = (u32)( 255.0f - ( 255.0f - b ) * k );
			color = ( color & 0xff000000 ) | r | ( g << 8 ) | ( b << 16 );
		}
	}
	else
	{
		vtx->texcoord[0] = vtx->texcoord[1] = 0;
	}

	vtx->ptcoord[0] = ptx;
	vtx->ptcoord[1] = pty;
	vtx->color = color;
}

// room for a primitive, flushing what is there if it doesn't fit
static bool PSP_Reserve( unsigned idx, unsigned vtx )
{
	C2Di_Context *ctx = C2Di_GetContext();

	if( !( ctx->flags & C2DiF_Active ))
		return false;
	if( !C2Di_CheckBufSpace( ctx, idx, vtx ))
		C2Di_FlushVtxBuf();
	return true;
}

/*
==============================================================================

citro2d

==============================================================================
*/
bool C2D_Init( size_t maxObjects )
{
	C2Di_Context *ctx = C2Di_GetContext();

	(void)maxObjects;
	memset( ctx, 0, sizeof( *ctx ));
	ctx->vtxBuf = vtx_buf;
	ctx->idxBuf = idx_buf;
	ctx->vtxBufSize = VTX_MAX;
	ctx->idxBufSize = IDX_MAX;
	ctx->flags = C2DiF_Active | C2DiF_DirtyAny;
	C2D_SetTintMode( C2D_TintSolid );
	C2D_ViewReset();
	return true;
}

void C2D_Fini( void )
{
}

void C2D_Prepare( void )
{
	C2Di_Context *ctx = C2Di_GetContext();
	ctx->flags |= C2DiF_Active | C2DiF_DirtyAny;
	ctx->curTex = NULL;
}

void C2D_Flush( void )
{
	C2Di_FlushVtxBuf();
}

void C2D_SceneSize( u32 width, u32 height, bool tilt )
{
	(void)width; (void)height; (void)tilt;
}

static void PSP_FadeRect( int x0, int y0, int x1, int y1 )
{
	PspVertex *v;

	PSP_Scissor( x0, y0, x1, y1 );
	GE_Tex( NULL );
	v = GE_DrawBegin( 2 );
	if( v )
	{
		memset( v, 0, 2 * sizeof( PspVertex ));
		v[0].x = x0; v[0].y = y0; v[0].color = fade_color;
		v[1].x = x1; v[1].y = y1; v[1].color = fade_color;
	}
	GE_DrawEnd( GU_SPRITES, v, 2 );
	C2Di_GetContext()->flags |= C2DiF_DirtyTex;
}

static void PSP_DrawFade( void )
{
	int x0, y0, x1, y1;

	// merged: once over everything, at the end of the frame
	if( !in_frame || !place.visible || !( fade_color >> 24 ) || psp_view == PSP_VIEW_MERGED )
		return;

	C2Di_FlushVtxBuf();
	PSP_TargetRect( &place, &x0, &y0, &x1, &y1 );
	PSP_FadeRect( x0, y0, x1, y1 );
	PSP_ApplyScissor();
}

void C2D_SceneTarget( C3D_RenderTarget *target )
{
	C2Di_FlushVtxBuf();
	PSP_DrawFade(); // the previous scene's

	cur_target = target;
	place = PSP_Place( target );
	game_scissor = false;

	if( in_frame )
	{
		bool defer = psp_view == PSP_VIEW_MERGED && target && target->screen == GFX_BOTTOM;
		if( defer && !deferring )
			GE_BeginDeferred();
		else if( !defer && deferring )
			GE_EndDeferred();
		PSP_ApplyScissor();
	}

	C2Di_GetContext()->flags |= C2DiF_DirtyAny;
}

void C2D_TargetClear( C3D_RenderTarget *target, u32 color )
{
	Placement p = PSP_Place( target );
	int x0, y0, x1, y1;

	// merged: the touch screen's clear would wipe the top screen under it
	if( !in_frame || !p.visible || p.full )
		return;

	C2Di_FlushVtxBuf();
	PSP_TargetRect( &p, &x0, &y0, &x1, &y1 );
	sceGuScissor( x0, y0, x1 - x0, y1 - y0 );
	sceGuClearColor( color );
	sceGuClear( GU_COLOR_BUFFER_BIT );
	PSP_ApplyScissor();
}

void C2D_Fade( u32 color )
{
	fade_color = color;
}

void C2D_SetTintMode( C2D_TintMode mode )
{
	C2Di_Context *ctx = C2Di_GetContext();
	u32 new_mode;

	tint_mode = mode;
	switch( mode )
	{
	default:
	case C2D_TintSolid: new_mode = C2DiF_Mode_ImageSolid; break;
	case C2D_TintMult:  new_mode = C2DiF_Mode_ImageMult;  break;
	case C2D_TintLuma:  new_mode = C2DiF_Mode_ImageLuma;  break;
	}

	ctx->flags = ( ctx->flags & ~C2DiF_TintMode_Mask ) | ( new_mode << ( C2DiF_TintMode_Shift - C2DiF_Mode_Shift ));
}

C3D_RenderTarget *C2D_CreateScreenTarget( gfxScreen_t screen, gfx3dSide_t side )
{
	C3D_RenderTarget *t = C3D_RenderTargetCreate( 240, screen == GFX_TOP ? 400 : 320, GPU_RB_RGBA8, GPU_RB_DEPTH16 );
	C3D_RenderTargetSetOutput( t, screen, side, 0 );
	return t;
}

/* ---- view ---------------------------------------------------------- */

void C2D_ViewReset( void )
{
	C2Di_FlushVtxBuf();
	view.a = view.d = 1;
	view.b = view.c = view.tx = view.ty = 0;
}

void C2D_ViewSave( C3D_Mtx *m )
{
	memset( m, 0, sizeof( *m ));
	m->r[0].x = view.a; m->r[0].y = view.b; m->r[0].w = view.tx;
	m->r[1].x = view.c; m->r[1].y = view.d; m->r[1].w = view.ty;
	m->r[2].z = 1;
	m->r[3].w = 1;
}

void C2D_ViewRestore( const C3D_Mtx *m )
{
	view.a = m->r[0].x; view.b = m->r[0].y; view.tx = m->r[0].w;
	view.c = m->r[1].x; view.d = m->r[1].y; view.ty = m->r[1].w;
}

// all of them apply on the right, as citro2d's: the new one acts first
void C2D_ViewTranslate( float x, float y )
{
	view.tx += view.a * x + view.b * y;
	view.ty += view.c * x + view.d * y;
}

void C2D_ViewScale( float x, float y )
{
	view.a *= x; view.c *= x;
	view.b *= y; view.d *= y;
}

void C2D_ViewRotate( float r )
{
	float c = cosf( r ), s = sinf( r );
	Affine o = view;

	view.a = o.a * c + o.b * s;
	view.b = -o.a * s + o.b * c;
	view.c = o.c * c + o.d * s;
	view.d = -o.c * s + o.d * c;
}

void C2D_ViewShear( float x, float y )
{
	Affine o = view;

	view.a = o.a + o.b * y;
	view.b = o.a * x + o.b;
	view.c = o.c + o.d * y;
	view.d = o.c * x + o.d;
}

/* ---- drawing ------------------------------------------------------- */

bool C2D_DrawImage( C2D_Image img, const C2D_DrawParams *params, const C2D_ImageTint *tints )
{
	static const C2D_Tint default_tint[4] = {
		{ 0xFFu << 24, 0.0f }, { 0xFFu << 24, 0.0f }, { 0xFFu << 24, 0.0f }, { 0xFFu << 24, 0.0f }
	};
	C2Di_Context *ctx = C2Di_GetContext();
	const C2D_Tint *tint = tints ? tints->corners : default_tint;
	float tl[2], tr[2], bl[2], br[2];
	C2Di_Quad quad;

	if( !img.tex || !img.subtex )
		return false;
	if( !PSP_Reserve( 6, 4 ))
		return false;

	C2Di_SetMode(( ctx->flags & C2DiF_TintMode_Mask ) >> ( C2DiF_TintMode_Shift - C2DiF_Mode_Shift ));
	C2Di_SetTex( img.tex );
	C2Di_Update();

	C2Di_CalcQuad( &quad, params );

	Tex3DS_SubTextureTopLeft( img.subtex, &tl[0], &tl[1] );
	Tex3DS_SubTextureTopRight( img.subtex, &tr[0], &tr[1] );
	Tex3DS_SubTextureBottomLeft( img.subtex, &bl[0], &bl[1] );
	Tex3DS_SubTextureBottomRight( img.subtex, &br[0], &br[1] );

	if( params->pos.w < 0 )
	{
		C2Di_SwapUV( tl, tr );
		C2Di_SwapUV( bl, br );
	}
	if( params->pos.h < 0 )
	{
		C2Di_SwapUV( tl, bl );
		C2Di_SwapUV( tr, br );
	}

	C2Di_AppendQuad();
	C2Di_AppendVtx( quad.topLeft[0],  quad.topLeft[1],  params->depth, tl[0], tl[1], 0, tint[0].blend, tint[0].color );
	C2Di_AppendVtx( quad.topRight[0], quad.topRight[1], params->depth, tr[0], tr[1], 0, tint[1].blend, tint[1].color );
	C2Di_AppendVtx( quad.botLeft[0],  quad.botLeft[1],  params->depth, bl[0], bl[1], 0, tint[2].blend, tint[2].color );
	C2Di_AppendVtx( quad.botRight[0], quad.botRight[1], params->depth, br[0], br[1], 0, tint[3].blend, tint[3].color );
	return true;
}

static void PSP_SolidMode( void )
{
	C2Di_SetMode( C2DiF_Mode_Solid );
	C2Di_Update();
}

bool C2D_DrawTriangle( float x0, float y0, u32 clr0, float x1, float y1, u32 clr1,
	float x2, float y2, u32 clr2, float depth )
{
	if( !PSP_Reserve( 3, 3 ))
		return false;

	PSP_SolidMode();
	C2Di_AppendTri();
	C2Di_AppendVtx( x0, y0, depth, -1, -1, 0, 0, clr0 );
	C2Di_AppendVtx( x1, y1, depth, -1, -1, 0, 0, clr1 );
	C2Di_AppendVtx( x2, y2, depth, -1, -1, 0, 0, clr2 );
	return true;
}

bool C2D_DrawLine( float x0, float y0, u32 clr0, float x1, float y1, u32 clr1, float thickness, float depth )
{
	float dx = x1 - x0, dy = y1 - y0;
	float len = sqrtf( dx * dx + dy * dy );
	float th, nx, ny;

	if( len <= 0.0f || !PSP_Reserve( 6, 4 ))
		return false;

	th = thickness / 2.0f / len;
	nx = -dy * th;
	ny = dx * th;

	PSP_SolidMode();
	C2Di_AppendQuad();
	C2Di_AppendVtx( x0 + nx, y0 + ny, depth, -1, -1, 0, 0, clr0 );
	C2Di_AppendVtx( x0 - nx, y0 - ny, depth, -1, -1, 0, 0, clr0 );
	C2Di_AppendVtx( x1 + nx, y1 + ny, depth, -1, -1, 0, 0, clr1 );
	C2Di_AppendVtx( x1 - nx, y1 - ny, depth, -1, -1, 0, 0, clr1 );
	return true;
}

bool C2D_DrawRectangle( float x, float y, float z, float w, float h, u32 clr0, u32 clr1, u32 clr2, u32 clr3 )
{
	if( !PSP_Reserve( 6, 4 ))
		return false;

	PSP_SolidMode();
	C2Di_AppendQuad();
	C2Di_AppendVtx( x,     y,     z, -1, -1, 0, 0, clr0 );
	C2Di_AppendVtx( x + w, y,     z, -1, -1, 0, 0, clr1 );
	C2Di_AppendVtx( x,     y + h, z, -1, -1, 0, 0, clr2 );
	C2Di_AppendVtx( x + w, y + h, z, -1, -1, 0, 0, clr3 );
	return true;
}

bool C2D_DrawEllipse( float x, float y, float z, float w, float h, u32 clr0, u32 clr1, u32 clr2, u32 clr3 )
{
	int i, n = 32;
	float cx = x + w / 2, cy = y + h / 2;

	(void)clr1; (void)clr2; (void)clr3;
	for( i = 0; i < n; i++ )
	{
		float a0 = i * 2 * M_PI / n, a1 = ( i + 1 ) * 2 * M_PI / n;
		C2D_DrawTriangle( cx, cy, clr0,
			cx + cosf( a0 ) * w / 2, cy + sinf( a0 ) * h / 2, clr0,
			cx + cosf( a1 ) * w / 2, cy + sinf( a1 ) * h / 2, clr0, z );
	}
	return true;
}

/*
==============================================================================

citro3d

==============================================================================
*/
bool C3D_Init( size_t cmdBufSize )
{
	(void)cmdBufSize;

	sceGuInit();
	sceGuStart( GU_DIRECT, gu_list );
	sceGuDrawBuffer( GU_PSM_8888, (void *)0, PSP_BUF_W );
	sceGuDispBuffer( PSP_SCR_W, PSP_SCR_H, (void *)FB_SIZE, PSP_BUF_W );
	sceGuDepthBuffer( (void *)( FB_SIZE * 2 ), PSP_BUF_W );
	sceGuOffset( 2048 - PSP_SCR_W / 2, 2048 - PSP_SCR_H / 2 );
	sceGuViewport( 2048, 2048, PSP_SCR_W, PSP_SCR_H );
	PSP_Scissor( 0, 0, PSP_SCR_W, PSP_SCR_H );
	sceGuEnable( GU_SCISSOR_TEST );
	sceGuDisable( GU_DEPTH_TEST );
	sceGuDisable( GU_CULL_FACE );
	sceGuShadeModel( GU_SMOOTH );
	sceGuEnable( GU_BLEND );
	sceGuBlendFunc( GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );
	sceGuEnable( GU_TEXTURE_2D );
	sceGuTexFunc( GU_TFX_MODULATE, GU_TCC_RGBA );
	sceGuClearColor( 0 );
	sceGuClear( GU_COLOR_BUFFER_BIT );
	sceGuFinish();
	sceGuSync( 0, 0 );

	sceDisplayWaitVblankStart();
	sceGuDisplay( GU_TRUE );
	return true;
}

void C3D_Fini( void )
{
	sceGuTerm();
}

/*
The frame is shown when the next one begins, not when it ends: between the two
the game runs its logic while the GE is still drawing, instead of one waiting
for the other. The list is free again once the GE is done with it.
*/
static bool frame_pending;
static SceUID render_thread;
static u32 last_vcount;

/*
Vblanks per frame: 2 (30 fps) while a level is being played, unless the
"60 FPS Switch" option is on. The level times itself (its delta), the menus
and the pause and level complete windows don't (a fixed 1/60 per frame), so
those stay at 60 or they would animate at half speed.
*/
static int PSP_FrameVblanks( void )
{
	if( settingsState.fps60 )
		return 1;
	if( game_state == STATE_GAME && !game_paused && !in_level_complete )
		return 2;
	return 1;
}

void PSP_PresentPending( void )
{
	u32 target;

	// only the thread that draws (the music thread opens files too)
	if( !frame_pending || sceKernelGetThreadId() != render_thread )
		return;
	frame_pending = false;
	sceGuSync( 0, 0 );

	// at least one vblank, more until the frame's time is up
	target = last_vcount + PSP_FrameVblanks();
	do
		sceDisplayWaitVblankStart();
	while(( s32 )( sceDisplayGetVcount() - target ) < 0 );
	sceGuSwapBuffers();
	last_vcount = sceDisplayGetVcount();
}

u32 PSP_PresentVcount( void )
{
	return last_vcount;
}

bool C3D_FrameBegin( u8 flags )
{
	(void)flags;

	PSP_PresentPending();
	PSP_ListStart();
	in_frame = true;
	ge_tex_valid = false;
	ge_clut = NULL;

	// the PSP screen around the 3DS ones
	PSP_Scissor( 0, 0, PSP_SCR_W, PSP_SCR_H );
	sceGuClearColor( 0xff000000 );
	sceGuClear( GU_COLOR_BUFFER_BIT );

	C2D_Prepare();
	defer_nops = defer_nverts = 0;
	sceGuBlendFunc( cur_blend.op, cur_blend.s, cur_blend.d, cur_blend.sf, cur_blend.df );
	place = PSP_Place( cur_target );
	if( psp_view == PSP_VIEW_MERGED && cur_target && cur_target->screen == GFX_BOTTOM )
		GE_BeginDeferred();
	PSP_ApplyScissor();
	return true;
}

void C3D_FrameEnd( u8 flags )
{
	(void)flags;

	C2Di_FlushVtxBuf();
	PSP_DrawFade();
	if( deferring )
		GE_EndDeferred();

	// the most the buffers have held, in the log when it grows (to size them)
	{
		static int peak_ops, peak_verts;
		if( defer_nops > peak_ops + peak_ops / 8 || defer_nverts > peak_verts + peak_verts / 8 )
		{
			if( defer_nops > peak_ops ) peak_ops = defer_nops;
			if( defer_nverts > peak_verts ) peak_verts = defer_nverts;
			psp_log( "peak: deferred %d ops, %d verts\n", peak_ops, peak_verts );
		}
	}

	if( psp_view == PSP_VIEW_MERGED )
	{
		// the touch screen over the top one, then the fade over both
		GE_Replay();
		if( fade_color >> 24 )
			PSP_FadeRect( 0, 0, PSP_SCR_W, PSP_SCR_H );
	}
	defer_nops = defer_nverts = 0;

	PSP_DrawOverlay(); // ctru_psp.c: the touch cursor

	{
		static int peak_list;
		int used = sceGuCheckList();
		if( used > peak_list + peak_list / 8 )
		{
			peak_list = used;
			psp_log( "peak: display list %d KB\n", used / 1024 );
		}
	}

	sceGuFinish();
	in_frame = false;
	frame_pending = true;
	render_thread = sceKernelGetThreadId();
}

// the vertices go out with the 2D overlay helpers below too
// The rectangle of the current 3DS screen that covers all of the PSP screen, in
// that screen's own coordinates. A "full screen" element (the menus' darkening)
// has to reach the whole PSP screen whichever screen it happens to be drawn
// into: the two have different sizes, and in the merged view the darkening is
// the one thing that has to cover both of them at once.
bool PSP_FullScreenRect( float *x, float *y, float *w, float *h )
{
	Placement p;
	float x0, y0, x1, y1;

	if( !cur_target )
		return false;

	p = PSP_Place( cur_target );
	if( !p.visible )
		return false;

	// the placement is v' = o + v * s, so invert it for the screen's corners
	x0 = -p.ox / p.sx;
	x1 = ( PSP_SCR_W - p.ox ) / p.sx;
	y0 = -p.oy / p.sy;
	y1 = ( PSP_SCR_H - p.oy ) / p.sy;

	*x = x0;
	*y = y0;
	*w = x1 - x0;
	*h = y1 - y0;
	return true;
}

void PSP_DrawRect2D( float x0, float y0, float x1, float y1, u32 color )
{
	PspVertex *v;

	if( !in_frame )
		return;

	C2Di_FlushVtxBuf();
	PSP_Scissor( 0, 0, PSP_SCR_W, PSP_SCR_H );
	PSP_SetTexture( NULL );
	v = PSP_GetMemory( 2 * sizeof( PspVertex ));
	memset( v, 0, 2 * sizeof( PspVertex ));
	v[0].x = x0; v[0].y = y0; v[0].color = color;
	v[1].x = x1; v[1].y = y1; v[1].color = color;
	sceGuDrawArray( GU_SPRITES, PSP_VTYPE, 2, 0, v );
	C2Di_GetContext()->flags |= C2DiF_DirtyTex;
	PSP_ApplyScissor();
}

// a point of a 3DS screen on the PSP one; false when that screen isn't shown
bool PSP_ScreenToPsp( int screen, float x, float y, float *px, float *py )
{
	C3D_RenderTarget t = { screen, 0, screen == GFX_TOP ? 400 : 320, 240, 0, false };
	Placement p = PSP_Place( &t );

	if( !p.visible )
		return false;
	*px = p.ox + x * p.sx;
	*py = p.oy + y * p.sy;
	return true;
}

static int PSP_BlendFactor( GPU_BLENDFACTOR f, bool src, unsigned *fix )
{
	switch( f )
	{
	case GPU_ZERO:                return *fix = 0, GU_FIX;
	case GPU_ONE:                 return *fix = 0xffffff, GU_FIX;
	case GPU_SRC_COLOR:           return src ? GU_DST_COLOR : GU_SRC_COLOR;
	case GPU_ONE_MINUS_SRC_COLOR: return src ? GU_ONE_MINUS_DST_COLOR : GU_ONE_MINUS_SRC_COLOR;
	case GPU_DST_COLOR:           return src ? GU_DST_COLOR : GU_SRC_COLOR;
	case GPU_ONE_MINUS_DST_COLOR: return src ? GU_ONE_MINUS_DST_COLOR : GU_ONE_MINUS_SRC_COLOR;
	case GPU_SRC_ALPHA:           return GU_SRC_ALPHA;
	case GPU_ONE_MINUS_SRC_ALPHA: return GU_ONE_MINUS_SRC_ALPHA;
	case GPU_DST_ALPHA:           return GU_DST_ALPHA;
	case GPU_ONE_MINUS_DST_ALPHA: return GU_ONE_MINUS_DST_ALPHA;
	default:                      return GU_SRC_ALPHA;
	}
}

void C3D_AlphaBlend( GPU_BLENDEQUATION colorEq, GPU_BLENDEQUATION alphaEq,
	GPU_BLENDFACTOR srcClr, GPU_BLENDFACTOR dstClr, GPU_BLENDFACTOR srcAlpha, GPU_BLENDFACTOR dstAlpha )
{
	static const int ops[] = { GU_ADD, GU_SUBTRACT, GU_REVERSE_SUBTRACT, GU_MIN, GU_MAX };
	unsigned sfix = 0, dfix = 0;
	int s, d;

	(void)alphaEq; (void)srcAlpha; (void)dstAlpha;

	C2Di_FlushVtxBuf();

	s = PSP_BlendFactor( srcClr, true, &sfix );
	d = PSP_BlendFactor( dstClr, false, &dfix );
	cur_blend.op = ops[colorEq < 5 ? colorEq : 0];
	cur_blend.s = s; cur_blend.d = d;
	cur_blend.sf = sfix; cur_blend.df = dfix;

	// out of a frame it waits for the next one (FrameBegin puts it back)
	if( in_frame )
		GE_Blend( cur_blend.op, s, d, sfix, dfix );
}

void C3D_SetScissor( GPU_SCISSORMODE mode, u32 left, u32 top, u32 right, u32 bottom )
{
	C2Di_FlushVtxBuf();

	if( mode == GPU_SCISSOR_DISABLE || !cur_target )
	{
		game_scissor = false;
	}
	else
	{
		// the 3DS framebuffer stands on its side: undo utils/gfx.c set_scissor
		int W = cur_target->width, H = cur_target->height;
		sc_x0 = W - (int)bottom;
		sc_x1 = W - (int)top;
		sc_y0 = H - (int)right;
		sc_y1 = H - (int)left;
		game_scissor = true;
	}

	if( in_frame )
		PSP_ApplyScissor();
}

void C3D_TexSetFilter( C3D_Tex *tex, GPU_TEXTURE_FILTER_PARAM magFilter, GPU_TEXTURE_FILTER_PARAM minFilter )
{
	(void)minFilter;
	if( tex && tex->filter != magFilter )
	{
		if( tex == C2Di_GetContext()->curTex )
			C2Di_GetContext()->flags |= C2DiF_DirtyTex;
		tex->filter = magFilter;
	}
}

C3D_RenderTarget *C3D_RenderTargetCreate( int width, int height, GPU_COLORBUF colorFmt, GPU_DEPTHBUF depthFmt )
{
	C3D_RenderTarget *t = calloc( 1, sizeof( *t ));

	(void)colorFmt; (void)depthFmt;
	if( !t )
		return NULL;
	// the 3DS framebuffer is on its side: "width" is the 240 and "height" the long one
	t->screen = -1;
	t->width = height;
	t->height = width;
	if( t->width > 400 ) t->width = 400; // wide mode or antialiasing: same screen
	if( t->height > 240 ) t->height = 240;
	return t;
}

void C3D_RenderTargetDelete( C3D_RenderTarget *target )
{
	if( target == cur_target )
		cur_target = NULL;
	free( target );
}

void C3D_RenderTargetSetOutput( C3D_RenderTarget *target, gfxScreen_t screen, gfx3dSide_t side, u32 transferFlags )
{
	(void)transferFlags;
	if( !target )
		return;
	target->screen = side == GFX_RIGHT ? -1 : screen; // one eye only
	target->side = side;
	target->width = screen == GFX_TOP ? 400 : 320;
	target->height = 240;
}

float C3D_GetProcessingTime( void ) { return 0; }
float C3D_GetDrawingTime( void ) { return 0; }
float C3D_GetCmdBufUsage( void ) { return (float)list_used / LIST_SIZE; }

C3D_TexEnv *C3D_GetTexEnv( int id ) { return &tex_envs[id < 6 ? id : 0]; }
void C3D_TexEnvInit( C3D_TexEnv *env ) { memset( env, 0, sizeof( *env )); }
void C3D_TexEnvSrc( C3D_TexEnv *env, C3D_TexEnvMode mode, int s1, int s2, int s3 ) { (void)env; (void)mode; (void)s1; (void)s2; (void)s3; }
void C3D_TexEnvFunc( C3D_TexEnv *env, C3D_TexEnvMode mode, int param ) { (void)env; (void)mode; (void)param; }

/*
==============================================================================

Sprite sheets (.psheet, psp/tools/mkatlas.py)

==============================================================================
*/
struct C2D_SpriteSheet_s {
	u32 num_images, num_pages;
	C3D_Tex *pages;
	Tex3DS_SubTexture *subtex;
	u16 *page_of;
};

typedef struct { u16 w, h; u8 psm, pad[3]; } PsheetPage;
typedef struct { u16 page, x, y, w, h, pad; } PsheetImage;

static int PSP_PsmBytes( int psm, int texels )
{
	switch( psm )
	{
	case 0: return texels * 4;
	case 1: return texels * 2;
	default: return texels;
	}
}

/*
VRAM is 2 MB: the two frame buffers take 1.06 MB and the depth buffer after
them is never used (no depth test, never cleared), so the rest, 960 KB, holds
the level objects' sheet for good: 896 KB less of RAM, and the GE reads its
own memory faster. Handed out once, never given back
*/
#define VRAM_SIZE  ( 2 * 1024 * 1024 )
static u32 vram_next = FB_SIZE * 2;

static void *PSP_VramAlloc( int size )
{
	u32 at = ( vram_next + 15 ) & ~15;
	if( at + size > VRAM_SIZE )
		return NULL;
	vram_next = at + size;
	return (void *)((u32)sceGeEdramGetAddr() + at );
}

// the sheets that live in VRAM: loaded at start, never freed
static bool PSP_SheetInVram( const char *path )
{
	return strstr( path, "/sprites.psheet" ) != NULL;
}

C2D_SpriteSheet C2D_SpriteSheetLoad( const char *filename )
{
	char path[256];
	char *dot;
	FILE *f;
	C2D_SpriteSheet sheet;
	char magic[4];
	u32 i;

	// the game asks for the 3DS name: romfs:/gfx/x.t3x -> romfs:/gfx/x.psheet
	snprintf( path, sizeof( path ), "%s", filename );
	if(( dot = strrchr( path, '.' )) && !strcmp( dot, ".t3x" ))
		strcpy( dot, ".psheet" );

	if( !( f = fopen( path, "rb" )))
	{
		psp_log( "C2D_SpriteSheetLoad: can't open %s\n", path );
		return NULL;
	}

	sheet = calloc( 1, sizeof( *sheet ));
	if( fread( magic, 1, 4, f ) != 4 || memcmp( magic, "PSH1", 4 )
		|| fread( &sheet->num_images, 4, 1, f ) != 1 || fread( &sheet->num_pages, 4, 1, f ) != 1 )
		goto fail;

	sheet->pages = calloc( sheet->num_pages, sizeof( C3D_Tex ));
	for( i = 0; i < sheet->num_pages; i++ )
	{
		PsheetPage pg;
		C3D_Tex *t = &sheet->pages[i];
		int size;

		if( fread( &pg, sizeof( pg ), 1, f ) != 1 )
			goto fail;

		t->width = pg.w;
		t->height = pg.h;
		t->filter = GPU_LINEAR;
		t->swizzled = ( pg.psm & 0x80 ) != 0;
		pg.psm &= 0x7f;
		t->psm = pg.psm == 0 ? GU_PSM_8888 : pg.psm == 1 ? GU_PSM_4444 : GU_PSM_T8;

		if( pg.psm == 2 )
		{
			t->clut = memalign( 16, 256 * 4 );
			if( !t->clut || fread( t->clut, 4, 256, f ) != 256 )
				goto fail;
		}

		size = PSP_PsmBytes( pg.psm, pg.w * pg.h );
		if( PSP_SheetInVram( path ) && ( t->data = PSP_VramAlloc( size )))
		{
			// written through the uncached mirror: nothing left in the cache
			t->in_vram = 1;
			if( fread((void *)((u32)t->data | 0x40000000 ), 1, size, f ) != (size_t)size )
				goto fail;
			continue;
		}
		t->data = memalign( 16, size );
		if( !t->data )
		{
			psp_log( "C2D_SpriteSheetLoad: %s: out of memory (%d KB)\n", path, size / 1024 );
			goto fail;
		}
		if( fread( t->data, 1, size, f ) != (size_t)size )
			goto fail;
	}

	sheet->subtex = calloc( sheet->num_images, sizeof( Tex3DS_SubTexture ));
	sheet->page_of = calloc( sheet->num_images, sizeof( u16 ));
	for( i = 0; i < sheet->num_images; i++ )
	{
		PsheetImage im;
		const C3D_Tex *t;
		Tex3DS_SubTexture *s = &sheet->subtex[i];

		if( fread( &im, sizeof( im ), 1, f ) != 1 || im.page >= sheet->num_pages )
			goto fail;

		t = &sheet->pages[im.page];
		sheet->page_of[i] = im.page;
		s->width = im.w;
		s->height = im.h;
		s->left = (float)im.x / t->width;
		s->right = (float)( im.x + im.w ) / t->width;
		s->top = 1.0f - (float)im.y / t->height;
		s->bottom = 1.0f - (float)( im.y + im.h ) / t->height;
	}

	fclose( f );
	sceKernelDcacheWritebackAll();
	return sheet;

fail:
	psp_log( "C2D_SpriteSheetLoad: %s is broken\n", path );
	fclose( f );
	C2D_SpriteSheetFree( sheet );
	return NULL;
}

C2D_SpriteSheet C2D_SpriteSheetLoadFromMem( const void *data, size_t size )
{
	(void)data; (void)size;
	return NULL;
}

void C2D_SpriteSheetFree( C2D_SpriteSheet sheet )
{
	u32 i;

	if( !sheet )
		return;
	if( frame_pending )
		sceGuSync( 0, 0 ); // the GE may still be reading these pages
	if( sheet->pages )
	{
		for( i = 0; i < sheet->num_pages; i++ )
		{
			if( !sheet->pages[i].in_vram )
				free( sheet->pages[i].data );
			free( sheet->pages[i].clut );
		}
		free( sheet->pages );
	}
	free( sheet->subtex );
	free( sheet->page_of );
	free( sheet );
}

size_t C2D_SpriteSheetCount( C2D_SpriteSheet sheet )
{
	return sheet ? sheet->num_images : 0;
}

C2D_Image C2D_SpriteSheetGetImage( C2D_SpriteSheet sheet, size_t index )
{
	// a missing image still needs a subtexture: the game reads its size
	static const Tex3DS_SubTexture empty = { 1, 1, 0, 1, 0, 1 };
	C2D_Image img = { NULL, &empty };

	if( !sheet || index >= sheet->num_images )
	{
		psp_log( "C2D_SpriteSheetGetImage: no image %d\n", (int)index );
		return img;
	}
	img.tex = &sheet->pages[sheet->page_of[index]];
	img.subtex = &sheet->subtex[index];
	return img;
}
