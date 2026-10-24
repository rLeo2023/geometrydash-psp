#ifdef __PSP__
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"
#include "menus/components/ui_list.h"
#include "menus/components/ui_rectangle.h"
#include "menus/components/ui_label.h"
#include "menus/components/ui_image.h"
#include "menus/components/ui_icon.h"
#include "menus/components/ui_button.h"
#include "menus/settings.h"
#include "menus/icon_kit.h"
#include "graphics.h"
#include "icons.h"
#include "utils/i18n.h"
#include "save/config.h"
#include "save/saving.h"
#include "main.h"
#include "profile.h"
#include "../../psp/src/psp_port.h"

static C2D_SpriteSheet profile_sheet;

static UIScreen screen = {
    .isBottom = true,
};

static UILabel *name_label;
static bool yes_exit = false;

// the on-screen keyboard (the system one is a stub on the PSP)
static UIScreen keyboard_screen = {
    .isBottom = true,
};
static UILabel *keyboard_preview;
static char keyboard_text[32];
static bool editing = false;
static bool keyboard_done = false;
static bool keyboard_accept = false;
// R toggles the letter keys between capitals and small letters
static bool keyboard_caps = true;

C2D_SpriteSheet profile_assets(void) {
    if (!profile_sheet) {
        profile_sheet = C2D_SpriteSheetLoad("romfs:/gfx/profile.t3x");
    }
    return profile_sheet;
}

void profile_assets_free(void) {
    if (profile_sheet) {
        C2D_SpriteSheetFree(profile_sheet);
        profile_sheet = NULL;
    }
}

static void exit_profile(UIElement *e) {
    (void)e;
    yes_exit = true;
}

static void set_profile_name(UILabel *label, const char *name) {
    if (!label) return;
    ui_label_set_text(label, name);
    // GD's name is big: keep the layout's size and only shrink it when it
    // would run past the panel
    float name_scale = 0.5f;
    float width = get_text_length(fonts[0].charset, 1.f, false, name) * name_scale;
    if (width > 200.f) name_scale *= 200.f / width;
    ui_element_set_scale((UIElement *) label, name_scale);
}

static void keyboard_refresh(void) {
    if (keyboard_preview)
        ui_label_set_text(keyboard_preview, keyboard_text);
}

static void keyboard_put(char ch) {
    size_t len = strlen(keyboard_text);
    if (len + 1 >= sizeof(keyboard_text)) return;
    keyboard_text[len] = ch;
    keyboard_text[len + 1] = 0;
    keyboard_refresh();
}

static void keyboard_key(UIElement *e) {
    int ch = (int) (intptr_t) e->userdata;
    if (keyboard_caps && ch >= 'a' && ch <= 'z') ch -= 32;
    if (!keyboard_caps && ch >= 'A' && ch <= 'Z') ch += 32;
    if (ch) keyboard_put((char) ch);
}

// the keys' labels follow the R toggle
static void keyboard_relabel(void) {
    for (size_t i = 0; i < keyboard_screen.count; i++) {
        UIElement *el = keyboard_screen.elements[i];
        if (el->tag[0][0] != 'k' || el->tag[0][1] != '_' || !el->tag[0][2]) continue;

        char ch = el->tag[0][2];
        if (ch >= 'A' && ch <= 'Z') ch += keyboard_caps ? 0 : 32;
        else if (ch >= 'a' && ch <= 'z') ch -= keyboard_caps ? 32 : 0;

        char label[2] = { ch, 0 };
        if (el->type == UI_BUTTON || el->type == UI_WINDOW_BUTTON)
            ui_button_set_text((UIButton *) el, label);
    }
}

static void keyboard_space(UIElement *e) {
    (void)e;
    keyboard_put(' ');
}

static void keyboard_back(UIElement *e) {
    (void)e;
    size_t len = strlen(keyboard_text);
    if (!len) return;
    keyboard_text[len - 1] = 0;
    keyboard_refresh();
}

static void keyboard_ok(UIElement *e) {
    (void)e;
    keyboard_accept = true;
    keyboard_done = true;
}

static void keyboard_cancel(UIElement *e) {
    (void)e;
    keyboard_accept = false;
    keyboard_done = true;
}

// opens the keyboard with the name already in it; the loop below commits it
static void edit_name(UIElement *e) {
    (void)e;

    strncpy(keyboard_text, settingsState.player_name, sizeof(keyboard_text) - 1);
    keyboard_text[sizeof(keyboard_text) - 1] = 0;

    static UIAction keyboard_actions[] = {
        { "key", keyboard_key },
        { "space", keyboard_space },
        { "back", keyboard_back },
        { "ok", keyboard_ok },
        { "cancel", keyboard_cancel },
    };

    ui_load_screen(&keyboard_screen, keyboard_actions,
        sizeof(keyboard_actions) / sizeof(keyboard_actions[0]), "romfs:/menus/keyboard.txt");
    ui_screen_open(&keyboard_screen, ANIM_ZOOM);

    keyboard_preview = (UILabel *) ui_get_element_by_tag(&keyboard_screen, "name_preview");

    // the letter keys carry their character in their tag (k_Q, k_A, ...)
    for (size_t i = 0; i < keyboard_screen.count; i++) {
        UIElement *el = keyboard_screen.elements[i];
        if (el->tag[0][0] == 'k' && el->tag[0][1] == '_' && el->tag[0][2]) {
            ui_element_set_userdata(el, (void *) (intptr_t) (unsigned char) el->tag[0][2]);
            // not a heap pointer: never free it when the screen unloads
            el->userdata_destroy = NULL;
        }
    }

    keyboard_refresh();
    keyboard_done = false;
    keyboard_accept = false;
    keyboard_caps = true;
    keyboard_relabel();
    editing = true;
}

static UIAction actions[] = {
    { "exit", exit_profile },
};

typedef struct {
    const char *tag;
    int *value;
    float scale;
    float max_width;
} ProfileStat;

// everything here is already tracked by the save file, and the layout has a
// label for each one (the stats' row on top and the text panel below it)
static const ProfileStat stats[] = {
    { "v_stars", &total_stars, 0.45f, 36.f },
    { "v_gold", &total_coins, 0.45f, 36.f },
    { "v_user", &total_user_coins, 0.45f, 36.f },
    { "v_demons", &total_demons, 0.45f, 36.f },
    { "v_levels", &completed_main_levels, 0.36f, 70.f },
    { "v_attempts", &total_attempts, 0.36f, 70.f },
    { "v_jumps", &total_jumps, 0.36f, 70.f },
};

// the four stats share one row, so a long number has to stay in its slot
static void set_stat_value(UILabel *label, const ProfileStat *stat) {
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", *stat->value);
    ui_label_set_text(label, tmp);

    float scale = stat->scale;
    float width = get_text_length(fonts[2].charset, 1.f, false, tmp) * scale;
    if (width > stat->max_width) scale *= stat->max_width / width;
    ui_element_set_scale((UIElement *) label, scale);
}

void profile_init(void) {
    ui_load_screen(&screen, actions, sizeof(actions) / sizeof(actions[0]), "romfs:/menus/profile.txt");
    ui_screen_open(&screen, ANIM_ZOOM);
    yes_exit = false;

    // the robot icon draws from this sheet, as in the icon kit
    robots_sheet_need(true);

    name_label = (UILabel *) ui_get_element_by_tag(&screen, "profile_name");
    set_profile_name(name_label, settingsState.player_name);

    // GD's brown panels come from the profile sheet, not the UI one (they are
    // stretches of a comment cell, so they keep the original's rounded corners)
    C2D_SpriteSheet sheet = profile_assets();
    static const char *panel_tags[] = { "panel_icons", "panel_texts" };
    for (int i = 0; sheet && i < 2; i++) {
        UIImage *panel = (UIImage *) ui_get_element_by_tag(&screen, panel_tags[i]);
        if (!panel) continue;
        C2D_SpriteFromSheet(&panel->image.sprite, sheet, i + 1);
        C3D_TexSetFilter(panel->image.sprite.image.tex, GPU_LINEAR, GPU_LINEAR);
        panel->base.w = panel->image.sprite.image.subtex->width;
        panel->base.h = panel->image.sprite.image.subtex->height;
    }

    for (size_t i = 0; i < sizeof(stats) / sizeof(stats[0]); i++) {
        UILabel *value = (UILabel *) ui_get_element_by_tag(&screen, stats[i].tag);
        if (!value) continue;
        set_stat_value(value, &stats[i]);
    }

    static const int gamemodes[6] = {
        GAMEMODE_PLAYER, GAMEMODE_SHIP, GAMEMODE_PLAYER_BALL,
        GAMEMODE_BIRD, GAMEMODE_DART, ROBOT
    };
    static const char *tags[6] = {
        "picon0", "picon1", "picon2", "picon3", "picon4", "picon5"
    };

    for (int i = 0; i < 6; i++) {
        UIIcon *icon = (UIIcon *) ui_get_element_by_tag(&screen, tags[i]);
        if (!icon) continue;
        ui_icon_set_gamemode_index(icon, gamemodes[i], *current_icons[gamemodes[i]]);
        icon->isSelected = false;
        icon->player_color = true;
    }
}

int profile_loop(void) {
    if (yes_exit) {
        ui_unload_screen(&screen);
        robots_sheet_need(false);
        return true;
    }

    if (editing) {
        if (hidKeysDown() & KEY_R) {
            keyboard_caps = !keyboard_caps;
            keyboard_relabel();
        }

        UIInput touch;
        touchPosition touch_pos;
        hidTouchRead(&touch_pos);
        touch.touchPosition = touch_pos;
        touch.did_something = false;
        touch.interacted = false;
        ui_screen_update(&keyboard_screen, &touch);

        ui_screen_draw(&screen);
        ui_screen_draw(&keyboard_screen);

        if (keyboard_done) {
            ui_unload_screen(&keyboard_screen);
            editing = false;

            if (keyboard_accept && keyboard_text[0]) {
                strncpy(settingsState.player_name, keyboard_text,
                    sizeof(settingsState.player_name) - 1);
                settingsState.player_name[sizeof(settingsState.player_name) - 1] = 0;
                config_set_string(&cfg, "playerName", settingsState.player_name);
                cfg_save();

                set_profile_name(name_label, settingsState.player_name);
            }
        }

        return false;
    }

    // Triangle changes the name; the hint is in the layout
    if (hidKeysDown() & KEY_X)
        edit_name(NULL);

    UIInput touch;
    touchPosition touch_pos;
    hidTouchRead(&touch_pos);
    touch.touchPosition = touch_pos;
    touch.did_something = false;
    touch.interacted = false;
    ui_screen_update(&screen, &touch);
    ui_screen_draw(&screen);

    return false;
}
#endif
