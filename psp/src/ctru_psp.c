/*
ctru_psp.c - libctru on the PSP: time, threads, input, paths and the rest

Controls (the 3DS has a touch screen the PSP lacks):
  Cross      A, and a touch at the cursor while the touch screen is shown
  Circle     B         Square     Y          Triangle   X
  Start      Start     Select     Select     R          R
  D-pad      in the menus: from button to button; playing: the d-pad
  Analog     moves the cursor when the touch screen is shown, else the circle pad
*/
#include <pspkernel.h>
#include <pspsdk.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <psprtc.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include <3ds.h>
#include <citro2d.h>
#include "psp_port.h"
#include "main.h" // game_state

PSP_MODULE_INFO( "gdpsp", PSP_MODULE_USER, 1, 0 );
PSP_MAIN_THREAD_ATTR( THREAD_ATTR_USER | THREAD_ATTR_VFPU );
PSP_MAIN_THREAD_STACK_SIZE_KB( 512 );
PSP_HEAP_SIZE_KB( -2048 ); // room outside the heap for threads and the firmware modules (avcodec), which real PSPs take from the same memory

/*
==============================================================================

Log

==============================================================================
*/
void psp_log( const char *fmt, ... )
{
	static FILE *f;
	va_list args;

	if( !f )
	{
		// "w" alone doesn't always truncate under PPSSPP: a shorter log kept
		// the previous run's tail
		remove( "psp_log.txt" );
		f = fopen( "psp_log.txt", "w" );
	}

	va_start( args, fmt );
	if( f )
	{
		vfprintf( f, fmt, args );
		fflush( f );
	}
	va_end( args );

	va_start( args, fmt );
	vprintf( fmt, args );
	va_end( args );
}

/*
==============================================================================

Memory watch: an allocation that fails goes to the log with its size, and
psp_log_memory() says how much is left (the biggest block malloc gives)

==============================================================================
*/
void *__real_malloc( size_t size );
void *__real_calloc( size_t n, size_t size );
void *__real_realloc( void *p, size_t size );
void *__real_memalign( size_t align, size_t size );

static void PSP_AllocFailed( const char *what, size_t size, void *caller )
{
	static int reported;
	if( reported++ < 20 )
	{
		psp_log( "%s(%u) failed (from %p) at %u ms\n", what, (unsigned)size, caller,
			(unsigned)( sceKernelGetSystemTimeWide() / 1000 ));
		if( reported == 1 )
			psp_log_memory( "at the first failure" );
	}
}

// big blocks go to the log too, with who asked
#define PSP_BIG_ALLOC ( 256 * 1024 )

void *__wrap_malloc( size_t size )
{
	void *p = __real_malloc( size );
	if( !p && size ) PSP_AllocFailed( "malloc", size, __builtin_return_address( 0 ));
	else if( size >= PSP_BIG_ALLOC ) psp_log( "malloc(%u KB) from %p\n", (unsigned)( size >> 10 ), __builtin_return_address( 0 ));
	return p;
}

void *__wrap_calloc( size_t n, size_t size )
{
	void *p = __real_calloc( n, size );
	if( !p && n && size ) PSP_AllocFailed( "calloc", n * size, __builtin_return_address( 0 ));
	else if( n * size >= PSP_BIG_ALLOC ) psp_log( "calloc(%u KB) from %p\n", (unsigned)(( n * size ) >> 10 ), __builtin_return_address( 0 ));
	return p;
}

void *__wrap_realloc( void *old, size_t size )
{
	void *p = __real_realloc( old, size );
	if( !p && size ) PSP_AllocFailed( "realloc", size, __builtin_return_address( 0 ));
	else if( size >= PSP_BIG_ALLOC ) psp_log( "realloc(%u KB) from %p\n", (unsigned)( size >> 10 ), __builtin_return_address( 0 ));
	return p;
}

void *__wrap_memalign( size_t align, size_t size )
{
	void *p = __real_memalign( align, size );
	if( !p && size ) PSP_AllocFailed( "memalign", size, __builtin_return_address( 0 ));
	return p;
}

void psp_log_memory( const char *when )
{
	struct mallinfo mi = mallinfo();
	size_t lo = 0, hi = 64 << 20;

	// the biggest single block, by bisection
	while( hi - lo > 16 * 1024 )
	{
		size_t mid = ( lo + hi ) / 2;
		void *p = __real_malloc( mid );
		if( p ) { free( p ); lo = mid; }
		else hi = mid;
	}
	psp_log( "memory %s: in use %d KB, biggest free block %d KB\n", when, mi.uordblks / 1024, (int)( lo / 1024 ));
}

/*
==============================================================================

Time

==============================================================================
*/
u64 svcGetSystemTick( void )
{
	u64 us = sceKernelGetSystemTimeWide();
	return us * 268 + us * 111856 / 1000000; // SYSCLOCK_ARM11 / 1e6 per microsecond
}

void svcSleepThread( s64 ns )
{
	if( ns <= 0 )
	{
		sceKernelDelayThread( 0 );
		return;
	}
	sceKernelDelayThread( (SceUInt)( ns / 1000 ));
}

u64 osGetTime( void )
{
	// milliseconds; the game only takes differences
	u64 tick;
	sceRtcGetCurrentTick( &tick );
	return tick / 1000;
}

void svcBreak( u32 reason )
{
	psp_log( "svcBreak(%u): the game stopped on a fatal error\n", (unsigned)reason );
	sceKernelDelayThread( 3 * 1000 * 1000 );
	sceKernelExitGame();
}

Result svcGetSystemInfo( s64 *out, u32 type, s32 param )
{
	(void)type; (void)param;
	*out = 0; // not Citra
	return 0;
}

Result svcGetThreadPriority( s32 *out, Handle handle )
{
	(void)handle;
	*out = 0x30;
	return 0;
}

void osSetSpeedupEnable( bool enable )
{
	// the PSP's top speed, the closest thing to the New 3DS clock
	if( enable )
		scePowerSetClockFrequency( 333, 333, 166 );
}

float osGet3DSliderState( void ) { return 0; }

size_t envGetHeapSize( void )
{
	return 40 * 1024 * 1024;
}

/*
==============================================================================

Memory

==============================================================================
*/
void *linearAlloc( size_t size ) { return memalign( 64, size ); }
void linearFree( void *mem ) { free( mem ); }

u32 linearSpaceFree( void )
{
	struct mallinfo mi = mallinfo();
	return (u32)mi.fordblks;
}

/*
==============================================================================

App, system

==============================================================================
*/
static volatile bool exit_requested;

static int PSP_ExitCallback( int arg1, int arg2, void *common )
{
	(void)arg1; (void)arg2; (void)common;
	exit_requested = true;
	return 0;
}

static int PSP_CallbackThread( SceSize args, void *argp )
{
	int cb;

	(void)args; (void)argp;
	cb = sceKernelCreateCallback( "exit", PSP_ExitCallback, NULL );
	sceKernelRegisterExitCallback( cb );
	sceKernelSleepThreadCB();
	return 0;
}

bool aptMainLoop( void )
{
	if( exit_requested )
		sceKernelExitGame();
	return true;
}

static void PSP_InputInit( void );
static void PSP_MoveOldSaves( void );

/*
A real PSP's FPU traps on a float division by zero, an invalid operation
(0/0, sqrt of a negative...) or an overflow, and the game dies on the spot;
PPSSPP doesn't, and the 3DS's VFP doesn't either, so the game's code does
them without a care (a volume slider, the pause menu...). Each thread starts
with them on, so every thread turns them off first
*/
static void __attribute__(( constructor )) PSP_FpuQuiet( void )
{
	pspSdkDisableFPUExceptions();
}

Result romfsInit( void )
{
	int th;

	pspSdkDisableFPUExceptions();
	th = sceKernelCreateThread( "callbacks", PSP_CallbackThread, 0x11, 0xFA0, 0, NULL );
	if( th >= 0 )
		sceKernelStartThread( th, 0, NULL );

	PSP_InputInit();
	psp_log( "gdpsp: started\n" );
	PSP_MoveOldSaves(); // before the game reads its saves
	return 0;
}

Result romfsExit( void ) { return 0; }

Result cfguInit( void ) { return 0; }
void cfguExit( void ) {}

Result CFGU_GetSystemModel( u8 *model )
{
	// a New 3DS: the full speed paths, no 2DS limits
	*model = CFG_MODEL_N3DS;
	return 0;
}

Result socInit( u32 *context_addr, u32 context_size ) { (void)context_addr; (void)context_size; return -1; }
Result socExit( void ) { return 0; }

void gfxInitDefault( void ) {}
void gfxExit( void ) {}
void gfxSet3D( bool enable ) { (void)enable; }
void gfxSetWide( bool enable ) { (void)enable; }
void gfxSwapBuffers( void ) {}
void gfxFlushBuffers( void ) {}
void gspWaitForVBlank( void ) { sceDisplayWaitVblankStart(); }

PrintConsole *consoleInit( gfxScreen_t screen, PrintConsole *console )
{
	(void)screen;
	return console;
}

Result HIDUSER_GetSoundVolume( u8 *volume )
{
	*volume = 63;
	return 0;
}

/*
==============================================================================

Threads

==============================================================================
*/
struct PspThread_s {
	SceUID id;
	ThreadFunc func;
	void *arg;
};

static int PSP_ThreadEntry( SceSize args, void *argp )
{
	struct PspThread_s *t = *(struct PspThread_s **)argp;
	(void)args;
	pspSdkDisableFPUExceptions(); // (see PSP_FpuQuiet)
	t->func( t->arg );
	return 0;
}

Thread threadCreate( ThreadFunc entrypoint, void *arg, size_t stack_size, int prio, int core_id, bool detached )
{
	struct PspThread_s *t = calloc( 1, sizeof( *t ));
	int psp_prio;

	(void)core_id; (void)detached;
	if( !t )
		return NULL;

	// the 3DS main thread runs at 0x30 and the PSP's at 0x20; lower is more urgent on both
	psp_prio = 0x20 + ( prio - 0x30 );
	if( psp_prio < 0x10 ) psp_prio = 0x10;
	if( psp_prio > 0x6f ) psp_prio = 0x6f;

	t->func = entrypoint;
	t->arg = arg;
	t->id = sceKernelCreateThread( "ctru", PSP_ThreadEntry, psp_prio,
		stack_size < 0x4000 ? 0x4000 : stack_size, PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, NULL );
	if( t->id < 0 )
	{
		free( t );
		return NULL;
	}
	sceKernelStartThread( t->id, sizeof( t ), &t );
	return t;
}

Result threadJoin( Thread thread, u64 timeout_ns )
{
	(void)timeout_ns;
	if( !thread )
		return -1;
	sceKernelWaitThreadEnd( thread->id, NULL );
	sceKernelDeleteThread( thread->id );
	free( thread );
	return 0;
}

void threadFree( Thread thread ) { (void)thread; }

// a zeroed LightLock is a valid unlocked one on the 3DS, so it's made on first use
static void PSP_LockMake( LightLock *lock )
{
	if( lock->sema <= 0 )
		lock->sema = sceKernelCreateSema( "lightlock", 0, 1, 1, NULL );
}

void LightLock_Init( LightLock *lock )
{
	lock->sema = 0;
	PSP_LockMake( lock );
}

void LightLock_Lock( LightLock *lock )
{
	PSP_LockMake( lock );
	sceKernelWaitSema( lock->sema, 1, NULL );
}

void LightLock_Unlock( LightLock *lock )
{
	PSP_LockMake( lock );
	sceKernelSignalSema( lock->sema, 1 );
}

void LightEvent_Init( LightEvent *event, ResetType reset_type )
{
	if( event->inited && event->sema > 0 )
	{
		// back to "not signalled"
		while( sceKernelPollSema( event->sema, 1 ) >= 0 )
			;
	}
	else
	{
		event->sema = sceKernelCreateSema( "lightevent", 0, 0, 1, NULL );
		event->inited = 1;
	}
	event->type = reset_type;
}

void LightEvent_Signal( LightEvent *event )
{
	if( !event->inited )
		LightEvent_Init( event, RESET_ONESHOT );
	sceKernelSignalSema( event->sema, 1 ); // fails when already signalled, as it should
}

void LightEvent_Wait( LightEvent *event )
{
	if( !event->inited )
		LightEvent_Init( event, RESET_ONESHOT );
	sceKernelWaitSema( event->sema, 1, NULL );
	if( event->type == RESET_STICKY )
		sceKernelSignalSema( event->sema, 1 );
}

void LightEvent_Clear( LightEvent *event )
{
	if( event->inited )
		while( sceKernelPollSema( event->sema, 1 ) >= 0 )
			;
}

/*
==============================================================================

Input

==============================================================================
*/
#define TOUCH_W 320
#define TOUCH_H 240

static u32 keys_held, keys_old;
static touchPosition touch_pos;
static circlePosition circle_pos;
static float cursor_x = TOUCH_W / 2, cursor_y = TOUCH_H / 2;
static bool cursor_used;

// the HID shared memory, as utils/precise_input.c reads it
static u32 hid_mem[0x400];
vu32 *hidSharedMem = hid_mem;

#define PAD_TICK_WORD    0
#define PAD_IDX_WORD     4
#define PAD_RING_WORD    ( 0x28 / 4 )
#define TOUCH_TICK_WORD  42
#define TOUCH_IDX_WORD   46
#define TOUCH_RING_WORD  (( 0xA8 + 0x20 ) / 4 )

static void PSP_SetTick( int word, u64 tick )
{
	hid_mem[word] = (u32)tick;
	hid_mem[word + 1] = (u32)( tick >> 32 );
}

static u64 PSP_GetTick( int word )
{
	return hid_mem[word] | ( (u64)hid_mem[word + 1] << 32 );
}

// one sample per scan, the ring laps every 8 like the 3DS's
static void PSP_PushSample( int tick_word, int idx_word, int ring_word, int entry_words, const u32 *entry )
{
	u32 idx = ( hid_mem[idx_word] + 1 ) & 7;
	int i;

	if( idx == 0 )
	{
		PSP_SetTick( tick_word + 2, PSP_GetTick( tick_word ));
		PSP_SetTick( tick_word, svcGetSystemTick());
	}
	for( i = 0; i < entry_words; i++ )
		hid_mem[ring_word + idx * entry_words + i] = entry[i];
	hid_mem[idx_word] = idx;
}

static void PSP_InputInit( void )
{
	u64 now = svcGetSystemTick();
	u64 frame = (u64)( CPU_TICKS_PER_MSEC * 16.7 );

	sceCtrlSetSamplingCycle( 0 );
	sceCtrlSetSamplingMode( PSP_CTRL_MODE_ANALOG );

	// a lap of history, so the sample interval is sane from the start
	PSP_SetTick( PAD_TICK_WORD, now );
	PSP_SetTick( PAD_TICK_WORD + 2, now - 8 * frame );
	PSP_SetTick( TOUCH_TICK_WORD, now );
	PSP_SetTick( TOUCH_TICK_WORD + 2, now - 8 * frame );
}

// the cursor is out, Cross touches with it: in the menus, and while playing
// only in the pause menu and the level complete window (Cross jumps else;
// the practice buttons over the level go by their keys)
static bool PSP_TouchShown( void )
{
	if( game_state == STATE_GAME && !game_paused && !in_level_complete )
		return false;
	return psp_view == PSP_VIEW_BOTTOM || psp_view == PSP_VIEW_BOTH || psp_view == PSP_VIEW_MERGED;
}

/*
==============================================================================

D-pad navigation in the menus

ui_screen.c lists what can be touched while the menus update; the d-pad
moves the cursor to the nearest one that way and Cross touches it. A
direction a button already answers to (level select's arrows) stays a key,
and on a slider left/right move it.

==============================================================================
*/
static PspTarget nav[PSP_MAX_TARGETS]; // the last frame's
static int nav_count;
static int snap_pending; // frames until the cursor goes to the nearest button
static bool cursor_by_dpad;

// a target where the cursor moves (top screen units when merged, touch screen ones else)
static bool PSP_TargetToCursor( const PspTarget *t, float *cx, float *cy, float *hw, float *hh )
{
	float k = 1, dx = 0, dy = 0;

	// a list's row scrolled out of sight
	if( t->in_list && ( t->x < t->cx0 || t->x > t->cx1 || t->y < t->cy0 || t->y > t->cy1 ))
		return false;

	if( psp_view == PSP_VIEW_MERGED )
	{
		if( t->bottom )
			psp_get_merge( &dx, &dy, &k );
	}
	else if( !t->bottom )
	{
		return false;
	}

	*cx = dx + t->x * k;
	*cy = dy + t->y * k;
	*hw = t->hw * k;
	*hh = t->hh * k;
	return true;
}

static const PspTarget *PSP_TargetAt( float x, float y )
{
	const PspTarget *best = NULL;
	float best_area = 1e9f;
	int i;

	for( i = 0; i < nav_count; i++ )
	{
		float cx, cy, hw, hh;
		if( !PSP_TargetToCursor( &nav[i], &cx, &cy, &hw, &hh ))
			continue;
		if( x < cx - hw || x >= cx + hw || y < cy - hh || y >= cy + hh )
			continue;
		if( hw * hh < best_area )
		{
			best_area = hw * hh;
			best = &nav[i];
		}
	}
	return best;
}

// to the nearest target that way; any direction (0, 0) takes the nearest at all
static bool PSP_Navigate( int dx, int dy )
{
	float best_score = 1e9f, bx = 0, by = 0;
	bool found = false;
	int i;

	for( i = 0; i < nav_count; i++ )
	{
		float cx, cy, hw, hh, vx, vy, along, across, score;

		if( !PSP_TargetToCursor( &nav[i], &cx, &cy, &hw, &hh ))
			continue;

		vx = cx - cursor_x;
		vy = cy - cursor_y;
		if( dx || dy )
		{
			along = vx * dx + vy * dy;
			across = fabsf( vx * dy ) + fabsf( vy * dx );
			if( along < 6 )
				continue;
			score = along + across * 2;
		}
		else
		{
			score = vx * vx + vy * vy;
		}

		if( score < best_score )
		{
			best_score = score;
			bx = cx;
			by = cy;
			found = true;
		}
	}

	if( found )
	{
		cursor_x = bx;
		cursor_y = by;
	}
	return found;
}

void hidScanInput( void )
{
	SceCtrlData pad;
	u32 keys = 0;
	float ax, ay;
	static u32 psp_old;
	u32 entry[4] = { 0 };
	u32 tentry[2] = { 0 };
	static int last_state = -1;
	static bool last_merged, last_paused, last_complete;
	float cursor_w;

	sceCtrlPeekBufferPositive( &pad, 1 );

	// what the menus offered last frame; this frame's list starts empty
	memcpy( nav, psp_targets, sizeof( PspTarget ) * psp_target_count );
	nav_count = psp_target_count;
	psp_target_count = 0;

	// the view follows the game: a menu with a PSP layout is one screen, the
	// others show both; playing, the level fills the PSP's screen with the
	// touch screen's buttons over it (pause, practice), like the phone game
	if( game_state != last_state || psp_menu_merged != last_merged || game_paused != last_paused
		|| in_level_complete != last_complete )
	{
		if( game_state == STATE_GAME )
			psp_view = psp_menu_merged ? PSP_VIEW_MERGED : PSP_VIEW_TOP;
		else
			psp_view = psp_menu_merged ? PSP_VIEW_MERGED : PSP_VIEW_BOTH;
		last_state = game_state;
		last_merged = psp_menu_merged;
		last_paused = game_paused;
		last_complete = in_level_complete;

		// the cursor starts in the middle, then goes to the nearest button
		cursor_x = ( psp_view == PSP_VIEW_MERGED ? GSP_SCREEN_HEIGHT_TOP : TOUCH_W ) / 2;
		cursor_y = TOUCH_H / 2;
		snap_pending = 3;
	}

	// Cross touches in the menus (A would also fire the buttons bound to A,
	// whichever is selected), jumps when playing
	if(( pad.Buttons & PSP_CTRL_CROSS ) && !PSP_TouchShown()) keys |= KEY_A;
	if( pad.Buttons & PSP_CTRL_CIRCLE )   keys |= KEY_B;
	if( pad.Buttons & PSP_CTRL_SQUARE )   keys |= KEY_Y;
	if( pad.Buttons & PSP_CTRL_TRIANGLE ) keys |= KEY_X;
	if( pad.Buttons & PSP_CTRL_START )    keys |= KEY_START;
	if( pad.Buttons & PSP_CTRL_SELECT )   keys |= KEY_SELECT;
	if( pad.Buttons & PSP_CTRL_RTRIGGER ) keys |= KEY_R;
	if( pad.Buttons & PSP_CTRL_UP )       keys |= KEY_DUP;
	if( pad.Buttons & PSP_CTRL_DOWN )     keys |= KEY_DDOWN;
	if( pad.Buttons & PSP_CTRL_LEFT )     keys |= KEY_DLEFT;
	if( pad.Buttons & PSP_CTRL_RIGHT )    keys |= KEY_DRIGHT;


	ax = ( pad.Lx - 128 ) / 128.0f;
	ay = ( pad.Ly - 128 ) / 128.0f;
	if( ax > -0.25f && ax < 0.25f ) ax = 0;
	if( ay > -0.25f && ay < 0.25f ) ay = 0;

	circle_pos.dx = circle_pos.dy = 0;
	if( PSP_TouchShown())
	{
		// the cursor, faster the further the stick goes; merged, it moves
		// over the top screen and touches the touch screen laid on it
		cursor_w = psp_view == PSP_VIEW_MERGED ? GSP_SCREEN_HEIGHT_TOP : TOUCH_W;
		cursor_x += ax * fabsf( ax ) * 6.0f;
		cursor_y += ay * fabsf( ay ) * 6.0f;
		if( cursor_x < 0 ) cursor_x = 0;
		if( cursor_y < 0 ) cursor_y = 0;
		if( cursor_x > cursor_w - 1 ) cursor_x = cursor_w - 1;
		if( cursor_y > TOUCH_H - 1 ) cursor_y = TOUCH_H - 1;
		if( ax || ay )
			cursor_used = true;

		if( ax || ay )
			cursor_by_dpad = false;

		// a couple of frames in, the list is the new menu's
		if( snap_pending && --snap_pending <= 0 )
		{
			if( nav_count )
			{
				PSP_Navigate( 0, 0 );
				cursor_by_dpad = true;
			}
			else
			{
				snap_pending = 1;
			}
		}

		// going by d-pad and the button went away (a popup opened, a menu
		// closed): on to the nearest one
		if( cursor_by_dpad && !snap_pending && nav_count && !PSP_TargetAt( cursor_x, cursor_y ))
			PSP_Navigate( 0, 0 );

		// the d-pad: from button to button
		{
			static const struct { u32 psp, key; int dx, dy; } dirs[4] = {
				{ PSP_CTRL_UP, KEY_DUP, 0, -1 }, { PSP_CTRL_DOWN, KEY_DDOWN, 0, 1 },
				{ PSP_CTRL_LEFT, KEY_DLEFT, -1, 0 }, { PSP_CTRL_RIGHT, KEY_DRIGHT, 1, 0 },
			};
			static int repeat;
			const PspTarget *at = PSP_TargetAt( cursor_x, cursor_y );
			int i, j;

			if( pad.Buttons & ( PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT ))
				repeat++;
			else
				repeat = 0;

			for( i = 0; i < 4; i++ )
			{
				bool bound = false;
				bool edge = ( pad.Buttons & dirs[i].psp ) && !( psp_old & dirs[i].psp );

				if( !( pad.Buttons & dirs[i].psp ))
					continue;

				for( j = 0; j < nav_count; j++ )
					if( nav[j].keys & dirs[i].key )
						bound = true;
				if( bound )
					continue; // the menu's own key

				// in a list, up and down also scroll it (to the rows below)
				if( at && at->in_list && dirs[i].dy )
				{
					if( edge )
					{
						PSP_Navigate( dirs[i].dx, dirs[i].dy );
						cursor_by_dpad = true;
					}
					continue;
				}

				keys &= ~dirs[i].key;

				if( at && at->slider && dirs[i].dx )
				{
					// held: a step every few frames
					if( edge || ( repeat > 20 && !( repeat % 3 )))
					{
						psp_slider_focus = at->elem;
						psp_slider_nudge = dirs[i].dx;
					}
				}
				else if( edge )
				{
					// off any button: first to the nearest one
					if( !at )
						PSP_Navigate( 0, 0 );
					else
						PSP_Navigate( dirs[i].dx, dirs[i].dy );
					cursor_by_dpad = true;
				}
			}
		}

		if( pad.Buttons & PSP_CTRL_CROSS )
		{
			float tx = cursor_x, ty = cursor_y;

			if( psp_view == PSP_VIEW_MERGED )
			{
				float dx, dy, k;
				psp_get_merge( &dx, &dy, &k );
				tx = ( cursor_x - dx ) / k;
				ty = ( cursor_y - dy ) / k;
			}

			keys |= KEY_TOUCH;
			// past the touch screen's edges too (the PSP layouts put buttons
			// there): the game reads these as signed (ui_screen.c)
			touch_pos.px = (u16)(s16)(int)tx;
			touch_pos.py = (u16)(s16)(int)ty;
			if( !touch_pos.px && !touch_pos.py )
				touch_pos.px = 1; // 0,0 means "no touch" to the game
		}
	}
	else
	{
		circle_pos.dx = (s16)( ax * 156 );
		circle_pos.dy = (s16)( -ay * 156 );
		if( ax > 0.5f )  keys |= KEY_CPAD_RIGHT;
		if( ax < -0.5f ) keys |= KEY_CPAD_LEFT;
		if( ay > 0.5f )  keys |= KEY_CPAD_DOWN;
		if( ay < -0.5f ) keys |= KEY_CPAD_UP;
	}

	if( !( keys & KEY_TOUCH ))
		touch_pos.px = touch_pos.py = 0;

	keys_old = keys_held;
	keys_held = keys;

	entry[0] = keys;
	PSP_PushSample( PAD_TICK_WORD, PAD_IDX_WORD, PAD_RING_WORD, 4, entry );
	tentry[0] = touch_pos.px | ( (u32)touch_pos.py << 16 );
	tentry[1] = ( keys & KEY_TOUCH ) ? 1 : 0;
	PSP_PushSample( TOUCH_TICK_WORD, TOUCH_IDX_WORD, TOUCH_RING_WORD, 2, tentry );

	psp_old = pad.Buttons;
}

u32 hidKeysDown( void ) { return keys_held & ~keys_old; }
u32 hidKeysHeld( void ) { return keys_held; }
u32 hidKeysUp( void ) { return ~keys_held & keys_old; }
void hidTouchRead( touchPosition *pos ) { *pos = touch_pos; }
void hidCircleRead( circlePosition *pos ) { *pos = circle_pos; }

void PSP_DrawOverlay( void )
{
	float x, y;

	int screen = psp_view == PSP_VIEW_MERGED ? GFX_TOP : GFX_BOTTOM;
	const PspTarget *at;

	if( !PSP_TouchShown() || !PSP_ScreenToPsp( screen, cursor_x, cursor_y, &x, &y ))
		return;

	// the selected button, framed
	if(( at = PSP_TargetAt( cursor_x, cursor_y )))
	{
		float cx, cy, hw, hh, x0, y0, x1, y1;
		if( PSP_TargetToCursor( at, &cx, &cy, &hw, &hh )
			&& PSP_ScreenToPsp( screen, cx - hw, cy - hh, &x0, &y0 )
			&& PSP_ScreenToPsp( screen, cx + hw, cy + hh, &x1, &y1 ))
		{
			u32 c = 0xc000ffff; // yellow
			PSP_DrawRect2D( x0 - 2, y0 - 2, x1 + 2, y0, c );
			PSP_DrawRect2D( x0 - 2, y1, x1 + 2, y1 + 2, c );
			PSP_DrawRect2D( x0 - 2, y0, x0, y1, c );
			PSP_DrawRect2D( x1, y0, x1 + 2, y1, c );
		}
	}

	// a small cross, white on black
	PSP_DrawRect2D( x - 6, y - 1, x + 7, y + 2, 0xff000000 );
	PSP_DrawRect2D( x - 1, y - 6, x + 2, y + 7, 0xff000000 );
	PSP_DrawRect2D( x - 5, y, x + 6, y + 1, ( keys_held & KEY_TOUCH ) ? 0xff00ffff : 0xffffffff );
	PSP_DrawRect2D( x, y - 5, x + 1, y + 6, ( keys_held & KEY_TOUCH ) ? 0xff00ffff : 0xffffffff );
	(void)cursor_used;
}

/*
==============================================================================

Software keyboard: none yet, every request is cancelled

==============================================================================
*/
void swkbdInit( SwkbdState *swkbd, SwkbdType type, int numButtons, int maxTextLength )
{
	memset( swkbd, 0, sizeof( *swkbd ));
	swkbd->type = type;
	swkbd->num_buttons = numButtons;
	swkbd->max_len = maxTextLength;
}
void swkbdSetFeatures( SwkbdState *swkbd, u32 features ) { swkbd->features = features; }
void swkbdSetHintText( SwkbdState *swkbd, const char *text ) { snprintf( swkbd->hint, sizeof( swkbd->hint ), "%s", text ? text : "" ); }
void swkbdSetInitialText( SwkbdState *swkbd, const char *text ) { snprintf( swkbd->initial, sizeof( swkbd->initial ), "%s", text ? text : "" ); }
void swkbdSetValidation( SwkbdState *swkbd, SwkbdValidInput validInput, u32 filterFlags, u32 maxDigits ) { (void)swkbd; (void)validInput; (void)filterFlags; (void)maxDigits; }
void swkbdSetButton( SwkbdState *swkbd, SwkbdButton button, const char *text, bool submit ) { (void)swkbd; (void)button; (void)text; (void)submit; }

SwkbdButton swkbdInputText( SwkbdState *swkbd, char *buf, size_t bufsize )
{
	if( buf && bufsize )
		snprintf( buf, bufsize, "%s", swkbd->initial );
	return SWKBD_BUTTON_LEFT;
}

/*
==============================================================================

Paths

The game names its files "romfs:/..." (inside the 3DS executable) and
"sdmc:/..." (the SD card). Here both are folders next to the EBOOT: romfs/,
and mstick/ for the SD card, where the game's own folder 3ds/gd3ds/ is
psp/gdpsp/ (saves, external_levels, saved_songs). The calls are wrapped at
link time (-Wl,--wrap=...), which also reaches the libraries (mpg123 opens
the songs itself).

==============================================================================
*/
#define PSP_CARD     "mstick/"
#define PSP_GAME_DIR PSP_CARD "psp/gdpsp"

static const char *PSP_Path( const char *in, char *buf, size_t size )
{
	const char *tail;

	if( !in )
		return in;
	if( !strncmp( in, "romfs:/", 7 ))
	{
		snprintf( buf, size, "romfs/%s", in + 7 );
		return buf;
	}

	// the SD card is the 3DS's default device: "/3ds/gd3ds/..." is on it
	if( !strncmp( in, "sdmc:/", 6 ))
		tail = in + 6;
	else if( in[0] == '/' )
		tail = in + 1;
	else
		return in;

	if( !strncmp( tail, "3ds/gd3ds", 9 ) && ( tail[9] == '/' || !tail[9] ))
		snprintf( buf, size, PSP_GAME_DIR "%s", tail + 9 );
	else if( !strncmp( tail, "3ds/", 4 ))
		snprintf( buf, size, PSP_CARD "psp/%s", tail + 4 );
	else
		snprintf( buf, size, PSP_CARD "%s", tail );
	return buf;
}

/*
The saves were in sdmc/3ds/gd3ds/ before: moved over once, so the progress
stays (only when the new folder isn't there yet)
*/
int __real_mkdir( const char *path, mode_t mode );
int __real_rename( const char *from, const char *to );
int __real_access( const char *path, int mode );

static void PSP_MoveOldSaves( void )
{
	if( __real_access( "sdmc/3ds/gd3ds", 0 ) != 0 || __real_access( PSP_GAME_DIR, 0 ) == 0 )
		return;
	__real_mkdir( "mstick", 0777 );
	__real_mkdir( PSP_CARD "psp", 0777 );
	if( __real_rename( "sdmc/3ds/gd3ds", PSP_GAME_DIR ) == 0 )
		psp_log( "saves moved from sdmc/3ds/gd3ds to " PSP_GAME_DIR "\n" );
	else
		psp_log( "couldn't move sdmc/3ds/gd3ds to " PSP_GAME_DIR "\n" );
}

// a NULL string crashed a menu (read at 0 in strcmp): say who passed it, and
// compare it as an empty string instead
int __real_strcmp( const char *a, const char *b );
int __wrap_strcmp( const char *a, const char *b )
{
	if( !a || !b )
	{
		static void *seen[16];
		void *from = __builtin_return_address( 0 );
		int i;

		for( i = 0; i < 16 && seen[i] && seen[i] != from; i++ );
		if( i < 16 && !seen[i] )
		{
			seen[i] = from;
			// __wrap_strcmp's own address gives the module's load address
			psp_log( "strcmp: NULL string from %p (this function at %p) other: %s\n",
				from, (void *)__wrap_strcmp, a ? a : b ? b : "NULL too" );
		}
		return __real_strcmp( a ? a : "", b ? b : "" );
	}
	return __real_strcmp( a, b );
}

FILE *__real_fopen( const char *path, const char *mode );
FILE *__wrap_fopen( const char *path, const char *mode )
{
	char buf[512];
	PSP_PresentPending(); // a "Loading..." frame shows before the load
	return __real_fopen( PSP_Path( path, buf, sizeof( buf )), mode );
}

int __real_open( const char *path, int flags, int mode );
int __wrap_open( const char *path, int flags, int mode )
{
	char buf[512];
	return __real_open( PSP_Path( path, buf, sizeof( buf )), flags, mode );
}

int __real_stat( const char *path, struct stat *st );
int __wrap_stat( const char *path, struct stat *st )
{
	char buf[512];
	return __real_stat( PSP_Path( path, buf, sizeof( buf )), st );
}

DIR *__real_opendir( const char *path );
DIR *__wrap_opendir( const char *path )
{
	char buf[512];
	return __real_opendir( PSP_Path( path, buf, sizeof( buf )));
}

int __real_access( const char *path, int mode );
int __wrap_access( const char *path, int mode )
{
	char buf[512];
	return __real_access( PSP_Path( path, buf, sizeof( buf )), mode );
}

int __real_remove( const char *path );
int __wrap_remove( const char *path )
{
	char buf[512];
	return __real_remove( PSP_Path( path, buf, sizeof( buf )));
}

int __real_unlink( const char *path );
int __wrap_unlink( const char *path )
{
	char buf[512];
	return __real_unlink( PSP_Path( path, buf, sizeof( buf )));
}

int __real_rename( const char *from, const char *to );
int __wrap_rename( const char *from, const char *to )
{
	char a[512], b[512];
	return __real_rename( PSP_Path( from, a, sizeof( a )), PSP_Path( to, b, sizeof( b )));
}

// the 3DS SD card has its folders; here the parents may be missing too
int __real_mkdir( const char *path, mode_t mode );
int __wrap_mkdir( const char *path, mode_t mode )
{
	char tmp[512], buf[512];
	char *p;

	snprintf( buf, sizeof( buf ), "%s", PSP_Path( path, tmp, sizeof( tmp )));
	for( p = buf + 1; *p; p++ )
	{
		if( *p == '/' && p[-1] != ':' )
		{
			*p = 0;
			__real_mkdir( buf, mode );
			*p = '/';
		}
	}
	return __real_mkdir( buf, mode );
}
