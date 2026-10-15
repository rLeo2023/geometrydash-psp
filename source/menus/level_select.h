#pragma once
#include "3ds/types.h"

void level_select_loop();

#define LEVEL_CARD_Y_POS 80

#define MENU_COIN_FILLED_ID 33
#define MENU_COIN_UNFILLED_ID 34

extern const size_t NUM_MENU_COLORS;
extern const u32 default_lvl_colors[];
extern int curr_level_id;