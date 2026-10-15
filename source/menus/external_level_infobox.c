#include <3ds.h>
#include <citro2d.h>
#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"
#include "menus/components/ui_label.h"
#include "save/saving.h"

static bool yes_exit = false;

static UIScreen screen = {
    .isBottom = true
};

static UILabel *name;

void exit_external_level_infobox(UIElement* e) {
    yes_exit = true;
}

static UIAction actions[] = {
    { "exit", exit_external_level_infobox },
};

void external_level_infobox_init() {
    ui_load_screen(&screen, actions, sizeof(actions) / sizeof(actions[0]), "romfs:/menus/level_info_pop_up.txt");
    ui_screen_open(&screen, ANIM_ZOOM);

    name = (UILabel *) ui_get_element_by_tag(&screen, "levelname");

    ui_label_set_text(name, level_info.level_name);

    LevelData *data = &level_data;

    char attempts[256];
    snprintf(attempts, sizeof(attempts), "<#40e348>Total Attempts</>: %d", data->attempts);
    
    char jumps[256];
    snprintf(jumps, sizeof(jumps), "<#60abef>Total Jumps</>: %d", data->jumps);
    
    char normal[256];
    snprintf(normal, sizeof(normal), "<#ff00ff>Normal</>: %d%%", data->normal_progress);
    
    char practice[256];
    snprintf(practice, sizeof(practice), "<#ffa54b>Practice</>: %d%%", data->practice_progress);

    ui_label_set_text((UILabel *) ui_get_element_by_tag(&screen, "totalattempts"), attempts);
    ui_label_set_text((UILabel *) ui_get_element_by_tag(&screen, "totaljumps"), jumps);
    ui_label_set_text((UILabel *) ui_get_element_by_tag(&screen, "normalprogressvalue"), normal);
    ui_label_set_text((UILabel *) ui_get_element_by_tag(&screen, "practiceprogressvalue"), practice);

    yes_exit = false;
}

int external_level_infobox_loop() {
    if (yes_exit) {
        ui_unload_screen(&screen);
        return true;
    }

    UIInput touch;
    touchPosition touchPos;
    hidTouchRead(&touchPos);
    touch.touchPosition = touchPos;
    touch.did_something = false;
    touch.interacted = false;
    ui_screen_update(&screen, &touch);

    return false;
}

void external_level_infobox_draw() {
    ui_screen_draw(&screen);
}
