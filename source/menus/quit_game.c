#ifdef __PSP__
#include <3ds.h>
#include <citro2d.h>
#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"
#include "quit_game.h"

static int result;
static bool first_frame;

static UIScreen screen = {
    .isBottom = true
};

static void cancel_quit(UIElement* e) {
    result = QUIT_GAME_CANCEL;
}

static void yes_quit(UIElement* e) {
    result = QUIT_GAME_YES;
}

static UIAction actions[] = {
    { "cancel", cancel_quit },
    { "quit", yes_quit },
};

void quit_game_init() {
    ui_load_screen(&screen, actions, sizeof(actions) / sizeof(actions[0]), "romfs:/menus/quit_game.txt");
    ui_screen_open(&screen, ANIM_ZOOM);
    result = QUIT_GAME_OPEN;
    first_frame = true;
}

// QUIT_GAME_OPEN while it shows, then what was picked
int quit_game_loop() {
    if (result != QUIT_GAME_OPEN) {
        ui_unload_screen(&screen);
        return result;
    }

    UIInput touch;
    touchPosition touchPos;
    hidTouchRead(&touchPos);
    touch.touchPosition = touchPos;
    touch.did_something = false;
    touch.interacted = false;

    // the circle press that opened it would press Cancel too
    if (!first_frame) ui_screen_update(&screen, &touch);
    first_frame = false;

    ui_screen_draw(&screen);

    return QUIT_GAME_OPEN;
}
#endif
