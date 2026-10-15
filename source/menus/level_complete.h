#pragma once

#define COMPLETE_COIN_FILLED_ID 26
#define COMPLETE_COIN_UNFILLED_ID 23

void level_complete_init();
int level_complete_loop(float delta);
void level_complete_destroy();
void draw_level_complete_top();
void draw_level_complete();