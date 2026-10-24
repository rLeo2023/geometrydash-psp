#pragma once
/*
triggers_v2 - the triggers GD 2.0 added (PSP build): groups, move, alpha,
toggle, spawn, pulse. See triggers_v2.c
*/
#ifdef __PSP__
#include <stdbool.h>
#include "color_channels.h"

// level_loading.c: an object's 2.0 keys, as the level has them
void trig2_parse_object(int obj, char **tokens, int count);
// after every object is in (groups -> their objects), and when it goes
void trig2_level_loaded(void);
void trig2_free(void);
// back to the level's start (death, restart)
void trig2_reset(void);

// color_channels.c: run a 2.0 trigger (false: not one of them)
bool trig2_run(int obj);
// only spawn triggers start it, never the player passing it
bool trig2_spawn_only(int obj);
// color_channels.c: whether running this object has any effect (any kind of
// trigger, 1.9 or 2.0). The scan of the level skips everything else
bool trig2_is_trigger(int obj);
// a color trigger's 2.0 opacity (35) and channel copy (50, HSV 49)
bool trig2_color_info(int obj, float *opacity, int *copied, float *h, float *s, float *v, bool *s_add, bool *v_add);

// main.c, each frame the level runs: moves, fades, spawns, pulses
void trig2_update(float dt);
// a teleport: the player's jump is not movement for the moves that follow him
void trig2_player_teleported(void);
// around the color channels' own update: channel pulses go on top of the
// channels' colors and come off again before the next update
void trig2_unpulse_channels(void);
void trig2_pulse_channels(void);

// graphics.c: an object's color and opacity with its groups' alpha and pulses
void trig2_object_color(int obj, int col_type, Color *color, float *opacity);
// graphics.c: whether its groups' alpha is 0, so it draws nothing
bool trig2_object_hidden(int obj);

// each object's size (key 32), NULL with no level
extern float *trig2_obj_scale;
#define OBJ_SCALE(o) (trig2_obj_scale ? trig2_obj_scale[o] : 1.f)

// practice.c: the groups' state in a checkpoint
void *trig2_save(void);
void trig2_restore(const void *snapshot);
void trig2_free_snapshot(void *snapshot);
#endif
