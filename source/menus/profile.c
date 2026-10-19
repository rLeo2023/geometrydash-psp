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

static C2D_SpriteSheet profile_sheet;

static UIScreen screen = {
    .isBottom = true,
};

static UIList *list;
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
    if (!profile_sheet)
        profile_sheet = C2D_SpriteSheetLoad("romfs:/gfx/profile.t3x");
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
    // long names shrink to fit the panel (never grow past the layout's size)
    float width = get_text_length(fonts[0].charset, 1.f, false, name);
    float name_scale = width > 0.f ? 290.f / width : 0.4f;
    if (name_scale > 0.4f) name_scale = 0.4f;
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
    const char *name;
    int *value;
} ProfileStat;

// everything here is already tracked by the save file
static const ProfileStat stats[] = {
    { "Stars", &total_stars },
    { "Secret Coins", &total_coins },
    { "Demons", &total_demons }, // the GD term stays in English
    { "Levels", &completed_main_levels },
    { "Attempts", &total_attempts },
    { "Jumps", &total_jumps },
};

static void add_stat_row(int i, const char *name, int value, float list_width) {
    UIElement *card = (UIElement *) ui_create_rectangle(&screen.ctx);

    if (!card) return;
    ui_rectangle_set_color((UIRectangle *) card,
        (i & 1 ? C2D_Color32(194, 114, 62, 255) : C2D_Color32(161, 88, 48, 255)));
    ui_element_set_size(card, 0, 13);

    UILabel *label = ui_create_label(&screen.ctx);
    if (label) {
        ui_label_set_text(label, tr(name));
        ui_element_set_position((UIElement *) label, -list_width + 6, 0);
        ui_element_set_scale((UIElement *) label, 0.4f);
        label->font = 2;
        ui_element_add_child(card, (UIElement *) label);
    }

    UILabel *value_label = ui_create_label(&screen.ctx);
    if (value_label) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%d", value);
        ui_label_set_text(value_label, tmp);
        ui_element_set_position((UIElement *) value_label, list_width - 6, 0);
        ui_element_set_scale((UIElement *) value_label, 0.4f);
        value_label->font = 2;
        value_label->alignment = 1;
        ui_element_add_child(card, (UIElement *) value_label);
    }

    ui_list_add(list, card);
}

void profile_init(void) {
    ui_load_screen(&screen, actions, sizeof(actions) / sizeof(actions[0]), "romfs:/menus/profile.txt");
    ui_screen_open(&screen, ANIM_ZOOM);
    yes_exit = false;

    // the robot icon draws from this sheet, as in the icon kit
    robots_sheet_need(true);

    name_label = (UILabel *) ui_get_element_by_tag(&screen, "profile_name");
    set_profile_name(name_label, settingsState.player_name);

    // the big user picture comes from the profile sheet, not the UI one
    C2D_SpriteSheet sheet = profile_assets();
    UIImage *picture = (UIImage *) ui_get_element_by_tag(&screen, "profile_pic");
    if (picture && sheet) {
        C2D_SpriteFromSheet(&picture->image.sprite, sheet, 0);
        C3D_TexSetFilter(picture->image.sprite.image.tex, GPU_LINEAR, GPU_LINEAR);
        picture->base.w = picture->image.sprite.image.subtex->width;
        picture->base.h = picture->image.sprite.image.subtex->height;
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

    list = (UIList *) ui_get_element_by_tag(&screen, "list");
    if (list) {
        float list_width = list->base.w * 0.5f;
        for (int i = 0; i < (int) (sizeof(stats) / sizeof(stats[0])); i++)
            add_stat_row(i, stats[i].name, *stats[i].value, list_width);
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
