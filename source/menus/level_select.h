#pragma once
#include <stdbool.h>
#include "3ds/types.h"

void level_select_loop();

// the PSP's Creator menu Extras button: the selector shows the Meltdown levels
extern bool level_select_extras;

#ifdef __PSP__
// GD's own levels, so their save file (ext_<name>_<creator>) is told apart
// from the custom levels' by the stats (their coins are gold, their stars
// come from the definition and not from the level's saved data)
int extras_levels_count(void);
void extras_level_save_key(int i, char *out, size_t size);
int extras_levels_stars(int i);
#endif

#define LEVEL_CARD_Y_POS 80

#define MENU_COIN_FILLED_ID 33
#define MENU_COIN_UNFILLED_ID 34

extern const size_t NUM_MENU_COLORS;
extern const u32 default_lvl_colors[];
extern int curr_level_id;