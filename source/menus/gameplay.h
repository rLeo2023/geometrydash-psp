#pragma once

#define MAX_DECIMAL_PERCENT 17

#define COIN_FILLED_ID 15
#define COIN_UNFILLED_ID 16
#define PAUSE_COIN_FILLED_ID 418
#define PAUSE_COIN_UNFILLED_ID 419

void reset_coins();
void gameplay_screen_init();
int gameplay_screen_top_loop();
int gameplay_screen_bot_loop();
void unpause_game();
void pause_game();