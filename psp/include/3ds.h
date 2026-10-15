/*
3ds.h - the part of libctru the game uses, on the PSP

The game was written against libctru; instead of touching every file, this
header gives it the same names, backed by the PSP SDK (psp/src/ctru_psp.c,
psp/src/ndsp_psp.c).
*/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
// libctru's headers bring these along, and the game counts on it
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef volatile u8  vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s32 vs32;
typedef s32 Result;
typedef u32 Handle;

#define BIT(n) (1U << (n))
#ifndef U64_MAX
#define U64_MAX UINT64_MAX
#endif
#define R_FAILED(res) ((res) < 0)
#define R_SUCCEEDED(res) ((res) >= 0)
#define CUR_THREAD_HANDLE 0xFFFF8000

/* ---- time ------------------------------------------------------------ */

// the game counts in 3DS ARM11 ticks; svcGetSystemTick scales the PSP clock
#define SYSCLOCK_ARM11 268111856
#define CPU_TICKS_PER_MSEC (SYSCLOCK_ARM11 / 1000.0)
#define CPU_TICKS_PER_USEC (SYSCLOCK_ARM11 / 1000000.0)

u64 svcGetSystemTick(void);
void svcSleepThread(s64 ns);
u64 osGetTime(void);

#define USERBREAK_PANIC 0
void svcBreak(u32 reason);
Result svcGetSystemInfo(s64 *out, u32 type, s32 param);
Result svcGetThreadPriority(s32 *out, Handle handle);

void osSetSpeedupEnable(bool enable);
float osGet3DSliderState(void);
size_t envGetHeapSize(void);

/* ---- memory ------------------------------------------------------------ */

void *linearAlloc(size_t size);
void linearFree(void *mem);
u32 linearSpaceFree(void);

/* ---- app / system ---------------------------------------------------- */

bool aptMainLoop(void);
Result romfsInit(void);
Result romfsExit(void);

Result cfguInit(void);
void cfguExit(void);
#define CFG_MODEL_3DS    0
#define CFG_MODEL_3DSXL  1
#define CFG_MODEL_N3DS   2
#define CFG_MODEL_2DS    3
#define CFG_MODEL_N3DSXL 4
#define CFG_MODEL_N2DSXL 5
Result CFGU_GetSystemModel(u8 *model);

Result socInit(u32 *context_addr, u32 context_size);
Result socExit(void);

/* ---- threads ------------------------------------------------------------ */

typedef struct PspThread_s *Thread;
typedef void (*ThreadFunc)(void *);
Thread threadCreate(ThreadFunc entrypoint, void *arg, size_t stack_size, int prio, int core_id, bool detached);
Result threadJoin(Thread thread, u64 timeout_ns);
void threadFree(Thread thread);

typedef struct { int sema; } LightLock;
void LightLock_Init(LightLock *lock);
void LightLock_Lock(LightLock *lock);
void LightLock_Unlock(LightLock *lock);

typedef enum { RESET_ONESHOT = 0, RESET_STICKY = 1, RESET_PULSE = 2 } ResetType;
typedef struct { int sema; int inited; ResetType type; } LightEvent;
void LightEvent_Init(LightEvent *event, ResetType reset_type);
void LightEvent_Signal(LightEvent *event);
void LightEvent_Wait(LightEvent *event);
void LightEvent_Clear(LightEvent *event);

/* ---- screens ------------------------------------------------------------ */

typedef enum { GFX_TOP = 0, GFX_BOTTOM = 1 } gfxScreen_t;
typedef enum { GFX_LEFT = 0, GFX_RIGHT = 1 } gfx3dSide_t;

#define GSP_SCREEN_TOP           0
#define GSP_SCREEN_BOTTOM        1
#define GSP_SCREEN_WIDTH         240
#define GSP_SCREEN_HEIGHT_TOP    400
#define GSP_SCREEN_HEIGHT_TOP_2X 800
#define GSP_SCREEN_HEIGHT_BOTTOM 320

void gfxInitDefault(void);
void gfxExit(void);
void gfxSet3D(bool enable);
void gfxSetWide(bool enable);
void gfxSwapBuffers(void);
void gfxFlushBuffers(void);
void gspWaitForVBlank(void);

typedef struct { int dummy; } PrintConsole;
PrintConsole *consoleInit(gfxScreen_t screen, PrintConsole *console);

#define GX_TRANSFER_FMT_RGBA8 0
#define GX_TRANSFER_FMT_RGB8  1
#define GX_TRANSFER_SCALE_NO  0
#define GX_TRANSFER_SCALE_X   1
#define GX_TRANSFER_SCALE_XY  2
#define GX_TRANSFER_FLIP_VERT(x)   ((x) << 0)
#define GX_TRANSFER_OUT_TILED(x)   ((x) << 1)
#define GX_TRANSFER_RAW_COPY(x)    ((x) << 3)
#define GX_TRANSFER_IN_FORMAT(x)   ((x) << 8)
#define GX_TRANSFER_OUT_FORMAT(x)  ((x) << 12)
#define GX_TRANSFER_SCALING(x)     ((x) << 24)

/* ---- input ------------------------------------------------------------ */

enum {
	KEY_A       = BIT(0),
	KEY_B       = BIT(1),
	KEY_SELECT  = BIT(2),
	KEY_START   = BIT(3),
	KEY_DRIGHT  = BIT(4),
	KEY_DLEFT   = BIT(5),
	KEY_DUP     = BIT(6),
	KEY_DDOWN   = BIT(7),
	KEY_R       = BIT(8),
	KEY_L       = BIT(9),
	KEY_X       = BIT(10),
	KEY_Y       = BIT(11),
	KEY_ZL      = BIT(14),
	KEY_ZR      = BIT(15),
	KEY_TOUCH   = BIT(20),
	KEY_CSTICK_RIGHT = BIT(24),
	KEY_CSTICK_LEFT  = BIT(25),
	KEY_CSTICK_UP    = BIT(26),
	KEY_CSTICK_DOWN  = BIT(27),
	KEY_CPAD_RIGHT = BIT(28),
	KEY_CPAD_LEFT  = BIT(29),
	KEY_CPAD_UP    = BIT(30),
	KEY_CPAD_DOWN  = BIT(31),

	KEY_UP    = KEY_DUP    | KEY_CPAD_UP,
	KEY_DOWN  = KEY_DDOWN  | KEY_CPAD_DOWN,
	KEY_LEFT  = KEY_DLEFT  | KEY_CPAD_LEFT,
	KEY_RIGHT = KEY_DRIGHT | KEY_CPAD_RIGHT,
};

typedef struct { u16 px, py; } touchPosition;
typedef struct { s16 dx, dy; } circlePosition;

void hidScanInput(void);
u32 hidKeysDown(void);
u32 hidKeysHeld(void);
u32 hidKeysUp(void);
void hidTouchRead(touchPosition *pos);
void hidCircleRead(circlePosition *pos);
Result HIDUSER_GetSoundVolume(u8 *volume);

// the HID shared memory layout, filled by hidScanInput (utils/precise_input.c
// reads the button and touch rings straight from it)
extern vu32 *hidSharedMem;

/* ---- software keyboard (not on the PSP yet: returns nothing) ----------- */

typedef enum { SWKBD_TYPE_NORMAL = 0, SWKBD_TYPE_QWERTY, SWKBD_TYPE_NUMPAD, SWKBD_TYPE_WESTERN } SwkbdType;
typedef enum { SWKBD_BUTTON_NONE = -1, SWKBD_BUTTON_LEFT = 0, SWKBD_BUTTON_MIDDLE, SWKBD_BUTTON_RIGHT, SWKBD_BUTTON_CONFIRM = SWKBD_BUTTON_RIGHT } SwkbdButton;
enum {
	SWKBD_PARENTAL = BIT(0), SWKBD_DARKEN_TOP_SCREEN = BIT(1), SWKBD_PREDICTIVE_INPUT = BIT(2),
	SWKBD_MULTILINE = BIT(3), SWKBD_FIXED_WIDTH = BIT(4), SWKBD_ALLOW_HOME = BIT(5),
	SWKBD_ALLOW_RESET = BIT(6), SWKBD_ALLOW_POWER = BIT(7), SWKBD_DEFAULT_QWERTY = BIT(9),
};
typedef enum { SWKBD_ANYTHING = 0, SWKBD_NOTEMPTY, SWKBD_NOTEMPTY_NOTBLANK, SWKBD_NOTBLANK_NOTEMPTY = SWKBD_NOTEMPTY_NOTBLANK, SWKBD_NOTBLANK, SWKBD_FIXEDLEN } SwkbdValidInput;
typedef struct { int type; int num_buttons; int max_len; char hint[64]; char initial[256]; u32 features; } SwkbdState;
void swkbdInit(SwkbdState *swkbd, SwkbdType type, int numButtons, int maxTextLength);
void swkbdSetFeatures(SwkbdState *swkbd, u32 features);
void swkbdSetHintText(SwkbdState *swkbd, const char *text);
void swkbdSetInitialText(SwkbdState *swkbd, const char *text);
void swkbdSetValidation(SwkbdState *swkbd, SwkbdValidInput validInput, u32 filterFlags, u32 maxDigits);
void swkbdSetButton(SwkbdState *swkbd, SwkbdButton button, const char *text, bool submit);
SwkbdButton swkbdInputText(SwkbdState *swkbd, char *buf, size_t bufsize);

/* ---- sound ------------------------------------------------------------ */
#include "3ds/ndsp.h"

#ifdef __cplusplus
}
#endif
