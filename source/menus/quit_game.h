#pragma once

// PSP: the "Quit Game" popup of the main menu (circle opens it)
enum {
    QUIT_GAME_OPEN,
    QUIT_GAME_CANCEL,
    QUIT_GAME_YES
};

void quit_game_init();
int quit_game_loop();
