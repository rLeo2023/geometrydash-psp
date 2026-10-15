#include <stdlib.h>
#include <3ds.h>
#include <citro2d.h>
#include "menus/core/common_setters.h"
#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"
#include "math_helpers.h"
#include "menus/components/ui_list.h"
#include "menus/components/ui_window.h"
#include "menus/components/ui_window_button.h"
#include "menus/components/ui_image.h"
#include "menus/components/ui_label.h"
#include "menus/components/ui_progress_bar.h"
#include "fonts/bigFont.h"
#include "main.h"
#include "easing.h"
#include "color_channels.h"
#include "mp3_player.h"
#include "graphics.h"
#include "level_select.h"

#include "level/main_levels.h"
#include "save/saving.h"

#include "menus/gameplay.h"

#include "state.h"

static bool start_level = false;
static bool exit_flag = false;

int curr_level_id = 0;

s8 scroll_dir = 0;

float anim_duration = 1.f;
float anim_time = 0;

static bool dragging;
static int dragStartX;
static int lastTouchX;
static int dragDistance;
static int dragDir;
//Prevents cards from overlapping 
//hopefully
static bool cardCorrection;

static UIImage *bg_gradient = NULL;
static UIImage *bg_gradient_top = NULL;
static UIWindowButton *level_card_window = NULL;
static UILabel *level_card_title_top = NULL;

static UILabel *level_card_title = NULL;
static UILabel *level_card_stars = NULL;
static UIImage *level_card_face = NULL;

static UIWindowButton *level_card_2_window = NULL;

static UILabel *level_card_2_title = NULL;
static UILabel *level_card_2_stars = NULL;

static UIProgressBar *level_card_normal_progress = NULL;
static UILabel *level_card_normal_progress_val = NULL;
static UIProgressBar *level_card_2_normal_progress = NULL;
static UILabel *level_card_2_normal_progress_val = NULL;

static UIProgressBar *level_card_practice_progress = NULL;
static UILabel *level_card_practice_progress_val = NULL;
static UIProgressBar *level_card_2_practice_progress = NULL;
static UILabel *level_card_2_practice_progress_val = NULL;

static UIImage *level_card_coin_1 = NULL;
static UIImage *level_card_coin_2 = NULL;
static UIImage *level_card_coin_3 = NULL;
static UIImage *level_card_2_coin_1 = NULL;
static UIImage *level_card_2_coin_2 = NULL;
static UIImage *level_card_2_coin_3 = NULL;

#define ANIM_DURATION 0.8f
#define COLOR_FADE_DURATION 0.1f

#define C2D_Color32Const(r, g, b, a) (r | (g << (u32)8) | (b << (u32)16) | (a << (u32)24))

const u32 default_lvl_colors[] = {
    C2D_Color32Const(0, 0, 232, 255),
    C2D_Color32Const(227, 0, 229, 255), 
    C2D_Color32Const(233, 0, 115, 255),
    C2D_Color32Const(233, 0, 0, 255),
    C2D_Color32Const(231, 112, 0, 255),
    C2D_Color32Const(233, 232, 0, 255),
    C2D_Color32Const(0, 231, 0, 255),
    C2D_Color32Const(0, 227, 228, 255),
    C2D_Color32Const(0, 112, 229, 255),
};

const size_t NUM_MENU_COLORS = ARRAY_LEN(default_lvl_colors);

#ifdef __PSP__
/*
One more page after the last level (and so before the first), like the
phone game: "Coming Soon!" alone, nothing to open. Each card shows a label of
its own for it (tag "soon" in level_select_psp.txt) instead of the level
*/
#define LEVEL_PAGES (MAIN_LEVELS_NUM + 1)
#define IS_SOON(l) ((l) == MAIN_LEVELS_NUM)
static int card_page[2];
static bool card2_shown;
#else
#define LEVEL_PAGES MAIN_LEVELS_NUM
#define IS_SOON(l) false
#endif

static int wrap_page(int level) {
    if (level < 0) return LEVEL_PAGES - 1;
    if (level >= LEVEL_PAGES) return 0;
    return level;
}

#define DOTS_SCALE 0.75f

#ifdef __PSP__
// drawn with the touch screen's half (40 + x): up top the ground covered them,
// and 232 is past the 8 lines the PSP cuts off the bottom
#define DOTS_X (SCREEN_BOT_WIDTH / 2)
#define DOTS_Y 226
#else
#define DOTS_X (SCREEN_WIDTH / 2)
#define DOTS_Y 232
#endif
#define DOTS_WIDTH 8
#define DOTS_MARGIN 8

static void draw_dots(int level_id) {
    C2D_Sprite spr = { 0 };
    C2D_SpriteFromSheet(&spr, ui_sheet, 421);
    C3D_TexSetFilter(spr.image.tex, GPU_LINEAR, GPU_LINEAR);
    C2D_SpriteSetCenter(&spr, 0.5f, 0.5f);
    C2D_SpriteSetScale(&spr, DOTS_SCALE, DOTS_SCALE);
    
    C2D_ImageTint tint = {0};

    C2D_PlainImageTint(&tint, C2D_Color32(125, 125, 125, 255), 1.f);

    int left_side = DOTS_X - ((DOTS_WIDTH * LEVEL_PAGES) + (DOTS_MARGIN * (LEVEL_PAGES - 1))) * DOTS_SCALE / 2;

    for (int i = 0; i < LEVEL_PAGES; i++) {
        C2D_SpriteSetPos(&spr, left_side + i * (DOTS_WIDTH + DOTS_MARGIN) * DOTS_SCALE, DOTS_Y);
        
        // Do not tint if current level
        if (i == level_id) {
            C2D_DrawSprite(&spr);
        } else {
            C2D_DrawSpriteTinted(&spr, &tint);
        }
    }
}

void update_level_name(int level, int card);
void update_level_stars(int level, int card);

void disable_card_2(UIElement *e) {
    e->enabled = false;
#ifdef __PSP__
    card2_shown = false;
#endif
}

void enable_card_2(UIElement *e) {
    e->enabled = true;
#ifdef __PSP__
    card2_shown = true;
#endif
}

#ifdef __PSP__
static bool has_tag(UIElement *e, const char *tag) {
    for (int i = 0; i < TAGS_PER_ELEMENT; i++)
        if (!strcmp(e->tag[i], tag)) return true;
    return false;
}

// each card shows either its level or "Coming Soon!", run every frame (the
// game turns the second card on and off as it slides)
static bool soon_card_on, soon_card_soon;
static void soon_apply(UIElement *e) {
    bool label = has_tag(e, "soon");
    e->enabled = soon_card_on && (label ? soon_card_soon : !soon_card_soon);
}

static void disable_soon_btn(UIElement *e) { e->enabled = false; }
static void enable_soon_btn(UIElement *e) { e->enabled = true; }

static void soon_refresh(void) {
    soon_card_on = true;
    soon_card_soon = IS_SOON(card_page[0]);
    ui_run_func_on_tag(&default_screen, "level_card", soon_apply);

    soon_card_on = card2_shown;
    soon_card_soon = IS_SOON(card_page[1]);
    ui_run_func_on_tag(&default_screen, "level_card_2", soon_apply);

    // nothing to open there: no button over the card for the cursor to frame
    ui_run_func_on_tag(&default_screen, "open_level_btn",
        IS_SOON(curr_level_id) ? disable_soon_btn : enable_soon_btn);
}
#endif

void level_card_move_right(UIElement *e) {
    e->x += 320;
}

void level_card_move_left(UIElement *e) {
    e->x -= 320;
}

void update_level_progress(int level, int card) {
    level = wrap_page(level);
    if (IS_SOON(level)) return;

    LevelData *data = &main_level_data[level]; 

    char normal[256];
    snprintf(normal, sizeof(normal), "<#ff00ff>Normal</>: %d%%", data->normal_progress);
    
    char practice[256];
    snprintf(practice, sizeof(practice), "<#ffa54b>Practice</>: %d%%", data->practice_progress);

    UIProgressBar *normal_prog = (card) ? level_card_2_normal_progress : level_card_normal_progress;
    UIProgressBar *practice_prog = (card) ? level_card_2_practice_progress : level_card_practice_progress;
    UILabel *normal_progval = (card) ? level_card_2_normal_progress_val : level_card_normal_progress_val;
    UILabel *practice_progval = (card) ? level_card_2_practice_progress_val : level_card_practice_progress_val;

    UIImage *coin_1 = (card) ? level_card_2_coin_1 : level_card_coin_1;
    UIImage *coin_2 = (card) ? level_card_2_coin_2 : level_card_coin_2;
    UIImage *coin_3 = (card) ? level_card_2_coin_3 : level_card_coin_3;

    normal_prog->value = data->normal_progress;
    practice_prog->value = data->practice_progress;

    snprintf(normal, sizeof(normal), "%d%%", data->normal_progress);
    snprintf(practice, sizeof(practice), "%d%%", data->practice_progress);

    ui_label_set_text(normal_progval, normal);
    ui_label_set_text(practice_progval, practice);
    
    ui_image_set_image(coin_1, (data->coin1 ? MENU_COIN_FILLED_ID : MENU_COIN_UNFILLED_ID), 0);
    ui_image_set_image(coin_2, (data->coin2 ? MENU_COIN_FILLED_ID : MENU_COIN_UNFILLED_ID), 0);
    ui_image_set_image(coin_3, (data->coin3 ? MENU_COIN_FILLED_ID : MENU_COIN_UNFILLED_ID), 0);
}

void update_level_name(int level, int card) {
    level = wrap_page(level);
#ifdef __PSP__
    card_page[card ? 1 : 0] = level;
#endif
    if (IS_SOON(level)) return;

    UILabel *e = (card) ? level_card_2_title : level_card_title;
    level_card_title_top = (UILabel *) ui_get_element_by_tag(&default_screen_top, "levelname");

    ui_element_set_scale((UIElement *) e, 1.f);

    ui_label_set_text(e, main_levels[level].level_name);

#ifdef __PSP__
    // one screen: the difficulty face goes in the card, by the name, like the
    // phone game (the 3DS has it on the other screen)
    UIImage *face = (UIImage *) ui_get_element_by_tag(&default_screen, card ? "card_face_2" : "card_face");
    if (face) ui_image_set_image(face, 239 + main_levels[level].difficulty, 0);
#endif
}

void update_level_stars(int level, int card) {
    level = wrap_page(level);
    if (IS_SOON(level)) return;

    UILabel *e = (card) ? level_card_2_stars : level_card_stars;
    char stars[10] = { 0 };
    snprintf(stars, 9, "%d", main_levels[level].stars);
    ui_label_set_text(e, stars);
}

void update_level_face(int level) {
    level = wrap_page(level);
    if (IS_SOON(level)) return;

    ui_image_set_image(level_card_face, 239 + main_levels[level].difficulty, 0);
}
void update_level_top(int level){
    if (IS_SOON(level)) return;
    LevelData *data = &main_level_data[level];

    char attempts[256];
    snprintf(attempts, sizeof(attempts), "<#40e348>Total Attempts</>: %d", data->attempts);
    
    char jumps[256];
    snprintf(jumps, sizeof(jumps), "<#60abef>Total Jumps</>: %d", data->jumps);

    char normal[256];
    snprintf(normal, sizeof(normal), "<#ff00ff>Normal</>: %d%%", data->normal_progress);
    
    char practice[256];
    snprintf(practice, sizeof(practice), "<#ffa54b>Practice</>: %d%%", data->practice_progress);

    ui_label_set_text(level_card_title_top, main_levels[level].level_name);
    ui_label_set_text((UILabel *) ui_get_element_by_tag(&default_screen_top, "totalattempts"), attempts);
    ui_label_set_text((UILabel *) ui_get_element_by_tag(&default_screen_top, "totaljumps"), jumps);
    ui_label_set_text((UILabel *) ui_get_element_by_tag(&default_screen_top, "normalprogress"), normal);
    ui_label_set_text((UILabel *) ui_get_element_by_tag(&default_screen_top, "practiceprogress"), practice);
}

void action_open_level(UIElement* e) {
    if (IS_SOON(curr_level_id)) return; // nothing there yet
    play_sfx(&play_sound, 1);
    set_fade_status(FADE_STATUS_OUT);
    start_level = true; 
}

void recenter(){
    update_level_name(curr_level_id, 0);
    update_level_stars(curr_level_id, 0);
    update_level_progress(curr_level_id, 0);
}

void handle_card_movement() {
    if (!dragging) {
        if (anim_time > anim_duration) {
            update_level_name(curr_level_id, 0);
            update_level_stars(curr_level_id, 0);
            update_level_progress(curr_level_id, 0);

            ui_run_func_on_tag(&default_screen, "level_card_2", disable_card_2);
            ui_set_pos_on_tag(&default_screen, 160, LEVEL_CARD_Y_POS, "level_card");
            ui_set_pos_on_tag(&default_screen, 160, LEVEL_CARD_Y_POS, "level_card_2");
            scroll_dir = 0;
            dragDistance = 0;

            return;
        }

        float start = (scroll_dir == 0) ? dragDistance : dragDistance * scroll_dir;
        float end = (scroll_dir == 0) ? 0 : 320;

        float fade_value = easeValue(ELASTIC_OUT, start, end, anim_time, anim_duration, 0.6f);

        float value = (scroll_dir == 0) ? 160 + fade_value : 160 + fade_value * scroll_dir;
        anim_time += 0.016666f;

        ui_set_pos_on_tag(&default_screen, value, LEVEL_CARD_Y_POS, "level_card");
        ui_set_pos_on_tag(&default_screen, value - 320 * scroll_dir, LEVEL_CARD_Y_POS, "level_card_2");
    }
}

void action_move_right(UIElement* e) { 
    curr_level_id++;
    scroll_dir = -1;
    anim_time = 0;
    anim_duration = 1.f;
    
    if (curr_level_id >= LEVEL_PAGES) curr_level_id = 0;

    ui_set_pos_on_tag(&default_screen, 160, LEVEL_CARD_Y_POS, "level_card");
    ui_run_func_on_tag(&default_screen, "level_card_2", enable_card_2);
    ui_run_func_on_tag(&default_screen, "level_card_2", level_card_move_right);
    
    upload_color_to_buffer(0, default_lvl_colors[curr_level_id % NUM_MENU_COLORS], COLOR_FADE_DURATION);

    update_level_name(curr_level_id - 1, 0);
    update_level_stars(curr_level_id - 1, 0);
    update_level_progress(curr_level_id - 1, 0);
    
    update_level_face(curr_level_id);
    update_level_top(curr_level_id);

    update_level_name(curr_level_id, 1);
    update_level_stars(curr_level_id, 1);
    update_level_progress(curr_level_id, 1);
};

void action_move_left(UIElement* e) { 
    curr_level_id--;
    scroll_dir = 1;
    anim_time = 0;
    anim_duration = 1.f;

    if (curr_level_id < 0) curr_level_id = LEVEL_PAGES-1;

    ui_set_pos_on_tag(&default_screen, 160, LEVEL_CARD_Y_POS, "level_card");
    ui_run_func_on_tag(&default_screen, "level_card_2", enable_card_2);
    ui_run_func_on_tag(&default_screen, "level_card_2", level_card_move_left);

    upload_color_to_buffer(0, default_lvl_colors[curr_level_id % NUM_MENU_COLORS], COLOR_FADE_DURATION);

    update_level_name(curr_level_id + 1, 0);
    update_level_stars(curr_level_id + 1, 0);
    update_level_progress(curr_level_id + 1, 0);
    
    update_level_face(curr_level_id);
    update_level_top(curr_level_id);

    update_level_name(curr_level_id, 1);
    update_level_stars(curr_level_id, 1);
    update_level_progress(curr_level_id, 1);
}

void lerp_level_colors(u32 color1, u32 color2){
    float frac = abs(dragDistance) / 320.f;
    frac = clampf(frac, 0.f, 1.f);
    u32 lerpCol = color_lerp_u32(color1, color2, frac);
    upload_color_to_buffer(0, lerpCol, 0);
}

void peek_right(){
    ui_run_func_on_tag(&default_screen, "level_card_2", enable_card_2);

    int card2id = wrap_page(curr_level_id + 1);

    update_level_name(card2id, 1);
    update_level_stars(card2id, 1);
    update_level_progress(card2id, 1);

    u32 col1 = default_lvl_colors[curr_level_id % NUM_MENU_COLORS];
    u32 col2 = default_lvl_colors[card2id % NUM_MENU_COLORS];
    lerp_level_colors(col1, col2);
}

void peek_left(){
    ui_run_func_on_tag(&default_screen, "level_card_2", enable_card_2);

    int card2id = wrap_page(curr_level_id - 1);

    update_level_name(card2id, 1);
    update_level_stars(card2id, 1);
    update_level_progress(card2id, 1);

    u32 col1 = default_lvl_colors[curr_level_id % NUM_MENU_COLORS];
    u32 col2 = default_lvl_colors[card2id % NUM_MENU_COLORS];
    lerp_level_colors(col1, col2);
}

void action_exit(UIElement* e) {
    exit_flag = true;
    set_fade_status(FADE_STATUS_OUT);
}

void tint_ground(UIElement *e) {
    ColorChannel channel = channels[0];
    ui_image_set_tint((UIImage *) e, C2D_Color32(channel.color.r, channel.color.g, channel.color.b, 255));
}

static UIAction actions[] = {
    {"open_level", action_open_level},
    {"exit", action_exit},
    {"move_right", action_move_right},
    {"move_left", action_move_left}
};

static UIAction actions_top[] = {

};

int mode = 0;

void level_select_loop() {
    start_level = false;
    exit_flag = false;
    state.custom_level = false;
    dragDistance = 0;
    ui_load_screen(&default_screen, actions, sizeof(actions) / sizeof(actions[0]), "romfs:/menus/level_select.txt");
    ui_load_screen(&default_screen_top, actions_top, sizeof(actions_top) / sizeof(actions_top[0]), "romfs:/menus/level_select_top.txt");

    // Set window color
    level_card_window = (UIWindowButton *) ui_get_element_by_tag(&default_screen, "card_window");
    ui_window_button_set_tint(level_card_window, C2D_Color32(0, 0, 0, 127));

    level_card_2_window = (UIWindowButton *) ui_get_element_by_tag(&default_screen, "card_window_2");
    ui_window_button_set_tint(level_card_2_window, C2D_Color32(0, 0, 0, 127));
    
    ui_window_set_tint((UIWindow *) ui_get_element_by_tag(&default_screen_top, "face_card"), C2D_Color32(0, 0, 0, 127));

    // Get level card components
    level_card_title = (UILabel *) ui_get_element_by_tag(&default_screen, "level_title");
    level_card_stars = (UILabel *) ui_get_element_by_tag(&default_screen, "level_stars");
    level_card_face  = (UIImage *) ui_get_element_by_tag(&default_screen_top, "level_face");

    level_card_2_title = (UILabel *) ui_get_element_by_tag(&default_screen, "level_title_2");
    level_card_2_stars = (UILabel *) ui_get_element_by_tag(&default_screen, "level_stars_2");

    level_card_normal_progress = (UIProgressBar *) ui_get_element_by_tag(&default_screen, "normalprogress");
    level_card_normal_progress_val = (UILabel *) ui_get_element_by_tag(&default_screen, "normalprogressvalue");
    level_card_2_normal_progress = (UIProgressBar *) ui_get_element_by_tag(&default_screen, "normalprogress_2");
    level_card_2_normal_progress_val = (UILabel *) ui_get_element_by_tag(&default_screen, "normalprogressvalue_2");
    
    level_card_practice_progress = (UIProgressBar *) ui_get_element_by_tag(&default_screen, "practiceprogress");
    level_card_practice_progress_val = (UILabel *) ui_get_element_by_tag(&default_screen, "practiceprogressvalue");
    level_card_2_practice_progress = (UIProgressBar *) ui_get_element_by_tag(&default_screen, "practiceprogress_2");
    level_card_2_practice_progress_val = (UILabel *) ui_get_element_by_tag(&default_screen, "practiceprogressvalue_2");
    
    level_card_coin_1 = (UIImage *) ui_get_element_by_tag(&default_screen, "coin_1");
    level_card_coin_2 = (UIImage *) ui_get_element_by_tag(&default_screen, "coin_2");
    level_card_coin_3 = (UIImage *) ui_get_element_by_tag(&default_screen, "coin_3");
    level_card_2_coin_1 = (UIImage *) ui_get_element_by_tag(&default_screen, "coin_1_2");
    level_card_2_coin_2 = (UIImage *) ui_get_element_by_tag(&default_screen, "coin_2_2");
    level_card_2_coin_3 = (UIImage *) ui_get_element_by_tag(&default_screen, "coin_3_2");

    ui_progress_bar_set_tint(level_card_normal_progress, C2D_Color32(0, 255, 0, 255));
    ui_progress_bar_set_tint(level_card_2_normal_progress, C2D_Color32(0, 255, 0, 255));

    ui_progress_bar_set_tint(level_card_practice_progress, C2D_Color32(0, 255, 255, 255));
    ui_progress_bar_set_tint(level_card_2_practice_progress, C2D_Color32(0, 255, 255, 255));

    update_level_name(curr_level_id, 0);
    update_level_stars(curr_level_id, 0);
    update_level_face(curr_level_id);
    update_level_top(curr_level_id);
    update_level_progress(curr_level_id, 0);
    
    ui_run_func_on_tag(&default_screen, "level_card_2", disable_card_2);

    u32 color = default_lvl_colors[curr_level_id % NUM_MENU_COLORS];
    upload_color_to_buffer(0, color, 0);

    channels[0].color.r = GET_R(color);
    channels[0].color.g = GET_G(color);
    channels[0].color.b = GET_B(color);

    dragging = false;
    dragDistance = 0;
    dragDir = 0;
    cardCorrection = false;

    scroll_dir = 0;

    set_fade_status(FADE_STATUS_IN);
        
    // Set bg color
    bg_gradient = (UIImage *) ui_get_element_by_tag(&default_screen, "gradient");
    bg_gradient_top = (UIImage *) ui_get_element_by_tag(&default_screen_top, "gradient");

    if (!playing_menu_loop) {
        play_mp3("romfs:/songs/menuLoop.mp3", true, 0);
        playing_menu_loop = true;
    }

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        u32 kUp = hidKeysUp();

        if (kDown & (KEY_START | KEY_A)) {
            action_open_level(NULL);
        }

        UIInput touch;
        touchPosition touchPos;
        hidTouchRead(&touchPos);
        touch.touchPosition = touchPos;
        touch.did_something = false;
        touch.interacted = false;

        handle_col_channel(0);

        ColorChannel channel = channels[0];

        ui_image_set_tint(bg_gradient, C2D_Color32(channel.color.r, channel.color.g, channel.color.b, 255));
        ui_image_set_tint(bg_gradient_top, C2D_Color32(channel.color.r, channel.color.g, channel.color.b, 255));
        ui_run_func_on_tag(&default_screen, "ground", tint_ground);

        if(cardCorrection){
            ui_run_func_on_tag(&default_screen, "level_card_2", disable_card_2);
            ui_set_pos_on_tag(&default_screen, 160, LEVEL_CARD_Y_POS, "level_card_2");
            cardCorrection = false;
        }

        if((kDown & KEY_TOUCH)){
            dragStartX = touch.touchPosition.px;
            dragging = false;
            dragDir = 0;
            dragDistance = 0;

            recenter();
        }
        if((kHeld & KEY_TOUCH)){
            if(dragging){
                int delta = touch.touchPosition.px - lastTouchX;
                dragDistance += delta;

                if(dragDistance > 15){
                    dragDir = -1;
                    peek_left();
                } else if(dragDistance < -15){
                    dragDir = 1;
                    peek_right();
                } else{
                    dragDir = 0;
                    ui_run_func_on_tag(&default_screen, "level_card_2", disable_card_2);
                }

                ui_set_pos_on_tag(&default_screen, 160 + dragDistance, LEVEL_CARD_Y_POS, "level_card");
                ui_set_pos_on_tag(&default_screen, 160 + dragDistance + (dragDir * 320), LEVEL_CARD_Y_POS, "level_card_2");
            } else {
                dragDir = 0;

                int dragXDiff = touch.touchPosition.px - dragStartX;
                if(abs(dragXDiff) > 10){
                    dragging = true;
                    scroll_dir = 0;
                    anim_duration = 1.f;
                    anim_time = anim_duration;
                    dragDistance = 0;
                    dragDir = 0;
                    cardCorrection = true;
                    recenter();
                }
            }
            lastTouchX = touch.touchPosition.px;
        }
        if((kUp & KEY_TOUCH)){
            if(dragging){
                if(dragDir == -1){
                    action_move_left(NULL);
                    anim_duration = 0.5f;
                } else if(dragDir == 1){
                    action_move_right(NULL);
                    anim_duration = 0.5f;
                } else{
                    scroll_dir = 0;
                    anim_time = 0.f;
                }
            }
            dragging = false;
            dragDir = 0;
        }
        
        handle_card_movement();

        //Buttons can't be tapped while dragging, left/right buttons cannot be pressed with keybinds while dragging
        if(dragging){
            touch.touchPosition.px = -99;
            touch.touchPosition.py = -99;
            ((UIButton *)ui_get_element_by_tag(&default_screen, "left"))->keyBinds = 0;
            ((UIButton *)ui_get_element_by_tag(&default_screen, "right"))->keyBinds = 0;
        } else{
            ((UIButton *)ui_get_element_by_tag(&default_screen, "left"))->keyBinds = (KEY_DLEFT | KEY_L | KEY_ZL | KEY_CPAD_LEFT | KEY_CSTICK_LEFT);
            ((UIButton *)ui_get_element_by_tag(&default_screen, "right"))->keyBinds = (KEY_DRIGHT | KEY_R | KEY_ZR | KEY_CPAD_RIGHT | KEY_CSTICK_RIGHT);
        }

#ifdef __PSP__
        soon_refresh();
#endif
        ui_screen_update(&default_screen, &touch);
        ui_screen_update(&default_screen_top, &touch);
#ifdef __PSP__
        // again: the arrows act during the update, and the frame drawn now
        // showed both a level and "Coming Soon!" on the card that came in
        soon_refresh();
#endif

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
#ifdef __PSP__
            draw_dots(curr_level_id);
#endif

            change_blending(true);
            draw_touch_effect();
            change_blending(false);

            // Top screen, drawn once per eye when 3D is on
            for (int eye = 0; begin_top_eye(eye); eye++) {
                begin_eye_layer(DEPTH_UI);
                ui_screen_draw(&default_screen_top);
#ifndef __PSP__
                draw_dots(curr_level_id);
#endif
                end_eye_layer();
            }
            C2D_ViewReset();
            C3D_FrameEnd(0);
        } while (handle_fading());

        if (start_level) {
            stop_mp3();
            game_state = STATE_GAME;
            playing_menu_loop = false;
            break;
        }

        if (exit_flag) {
            game_state = STATE_MAIN_MENU;
            break;
        }
    }
    C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
    
    ui_unload_screen(&default_screen);
    ui_unload_screen(&default_screen_top);
}
