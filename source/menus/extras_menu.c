#ifdef __PSP__
// The Extras screen: the row of square buttons the creator menu has, with the
// Meltdown levels behind one of them and the custom levels (the map packs)
// behind another. Built the same way as creator_menu.c
#include <3ds.h>
#include <citro2d.h>
#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"
#include "menus/components/ui_image.h"
#include "menus/components/ui_button.h"
#include "menus/components/ui_label.h"
#include "main.h"
#include "graphics.h"
#include "state.h"
#include "mp3_player.h"
#include "level_select.h"
#include "external_levels.h"
#include "extras_menu.h"

static int new_state = 0;
static bool exit_flag = false;

static UIImage *bg_gradient;
static UIImage *bg_gradient_top;

static void action_exit(UIElement *e) {
    exit_flag = true;
    set_fade_status(FADE_STATUS_OUT);
}

// Meltdown: the level selector's extras page (its own three levels)
static void action_open_meltdown(UIElement *e) {
    level_select_extras = true;
    curr_level_id = 0;
    new_state = STATE_LEVEL_SELECT;
    set_fade_status(FADE_STATUS_OUT);
}

// Map packs: a separate download, in their own folder (the same browser as the
// custom levels); nothing there yet? the screen says where to get them
static void action_open_mappacks(UIElement *e) {
    external_mappacks_mode = true;
    external_from_extras = true;
    new_state = STATE_EXTERNAL_LEVELS;
    set_fade_status(FADE_STATUS_OUT);
}

static UIAction actions[] = {
    {"exit", action_exit},
    {"meltdown", action_open_meltdown},
    {"mappacks", action_open_mappacks},
};

void extras_menu_loop() {
    exit_flag = false;
    new_state = STATE_EXTRAS_MENU;

    ui_load_screen(&default_screen, actions, sizeof(actions) / sizeof(actions[0]), "romfs:/menus/extras_menu.txt");
    bg_gradient = (UIImage *) ui_get_element_by_tag(&default_screen, "gradient");
    ui_load_screen(&default_screen_top, actions, sizeof(actions) / sizeof(actions[0]), "romfs:/menus/extras_menu_top.txt");
    bg_gradient_top = (UIImage *) ui_get_element_by_tag(&default_screen_top, "gradient_top");

    ui_image_set_tint(bg_gradient, C2D_Color32(50, 110, 255, 255));
    ui_image_set_tint(bg_gradient_top, C2D_Color32(50, 110, 255, 255));

    set_fade_status(FADE_STATUS_IN);

    if (!playing_menu_loop) {
        play_mp3("romfs:/songs/menuLoop.mp3", true, 0);
        playing_menu_loop = true;
    }

    while (aptMainLoop()) {
        hidScanInput();

        UIInput touch;
        touchPosition touchPos;
        hidTouchRead(&touchPos);
        touch.touchPosition = touchPos;
        touch.did_something = false;
        touch.interacted = false;
        ui_screen_update(&default_screen, &touch);

        // Frees a render target, so keep it out of the frame below
        update_stereo_target();

        do {
            update_touch_effect(DT);

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // Bottom screen
            C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
            C2D_SceneBegin(bot);
            draw_fade();

            ui_screen_draw(&default_screen);

            change_blending(true);
            draw_touch_effect();
            change_blending(false);

            // Top screen, drawn once per eye when 3D is on
            for (int eye = 0; begin_top_eye(eye); eye++) {
                draw_fade();

                begin_eye_layer(DEPTH_UI);
                ui_screen_draw(&default_screen_top);
                end_eye_layer();
            }
            C2D_ViewReset();
            C3D_FrameEnd(0);
        } while (handle_fading());

        if (exit_flag) {
            game_state = STATE_CREATOR_MENU;
            break;
        }

        if (new_state != STATE_EXTRAS_MENU) {
            game_state = new_state;
            break;
        }
    }
    C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));

    ui_unload_screen(&default_screen);
    ui_unload_screen(&default_screen_top);
}
#endif
