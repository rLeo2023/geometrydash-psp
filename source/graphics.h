#pragma once
#include <citro2d.h>
#include "level_loading.h"
#include "color_channels.h"

#define FADING_OBJ_PADDING 45
#define FADING_OBJ_WIDTH 180

#ifdef __PSP__
// the fade at the sides. Behind the player (left) 75 reached past him and
// blocks went see-through while still on screen: 30 there. At the right edge
// the level is coming in, far from the player, and the 3DS 75 keeps it from
// popping in too quickly
#define FADE_WIDTH_LEFT 30
#define FADE_WIDTH_RIGHT 75
#else
#define FADE_WIDTH_LEFT 75
#define FADE_WIDTH_RIGHT 75
#endif

#define BACKGROUND_SCALE 0.9f

#define GROUND_SIZE 128
#define LINE_WIDTH 444
#define LINE_HEIGHT 2

#ifdef __PSP__
// a heavy official level peaks around 600 (the log says, "peak: N sprites"):
// 8192 of them took 900 KB of a PSP-1000's RAM
#define MAX_SPRITES   4096
#else
#define MAX_SPRITES   8192
#endif

#define SCREEN_HEIGHT_AREA (320.f)
#define SCREEN_WIDTH_AREA ((SCREEN_HEIGHT_AREA / SCREEN_HEIGHT) * SCREEN_WIDTH)
#define SCREEN_WIDTH_AREA_BOT ((SCREEN_HEIGHT_AREA / SCREEN_HEIGHT) * SCREEN_BOT_WIDTH)

#define SCALE (SCREEN_HEIGHT / SCREEN_HEIGHT_AREA)


extern int current_pulserod_ball_image;

// Simple sprite struct
typedef struct
{
    C2D_Sprite spr;
    float dx, dy; // velocity
} Sprite;

typedef struct
{
    C2D_Sprite spr;
    C2D_ImageTint tint;
    int obj;
    int layer;
    int col_type;
    float opacity;
    int col_channel;
    int zlayer;
} SpriteObject;

typedef struct {
    SpriteObject *obj;
    uint32_t key;
} SortItem;

enum FadingEffects {
    FADE_NONE,
    FADE_UP,
    FADE_DOWN,
    FADE_RIGHT,
    FADE_LEFT,
    FADE_SCALE_IN,
    FADE_SCALE_OUT,
    FADE_INWARDS,
    FADE_OUTWARDS,
    FADE_CIRCLE_LEFT,
    FADE_CIRCLE_RIGHT,
    FADE_UP_SLOW_LEFT,
    FADE_DOWN_SLOW_LEFT,
    FADE_UP_SLOW_RIGHT,
    FADE_DOWN_SLOW_RIGHT,
    FADE_UP_STATIONARY,
    FADE_DOWN_STATIONARY,
    FADE_COUNT
};

typedef struct {
    C2D_Sprite parent_template;
    C2D_Sprite glow_template;
    int child_count;
    C2D_Sprite *child_templates;
} SpriteTemplate;

void cache_all_sprites();
void free_cached_sprites();
bool get_sprite_from_texture(int tex, C2D_Sprite *out);
void get_fade_vars(int obj, float x, int *fade_x, int *fade_y, float *fade_scale);
int obj_edge_fade(float x, int right_edge);
void get_special_fading_vars(int obj, float fade_val, float *calc_x);

extern bool p1_trail;
extern int current_fading_effect;

extern int sprite_count;
extern C2D_SpriteSheet spriteSheet;
extern C2D_SpriteSheet spriteSheet2;
extern C2D_SpriteSheet glowSheet;
extern C2D_SpriteSheet bgSheet;
extern C2D_SpriteSheet bg2Sheet;
extern C2D_SpriteSheet groundSheet;
extern C2D_SpriteSheet iconSheet;
extern C2D_SpriteSheet trailSheet;
extern C2D_SpriteSheet particleSheet;

extern SpriteTemplate sprite_templates[GAME_OBJECT_COUNT];

extern const Color white;

extern float object_creating_time;
extern float object_sorting_time;
extern float object_drawing_time;

inline float normalize_angle(float a)
{
    while (a < 0.0f)   a += 360.0f;
    while (a >= 360.0f) a -= 360.0f;
    return a;
}

void create_objects();
void change_blending(bool blending);
Color get_white_if_black(Color color);
Color get_p1_if_black(Color color);
Color get_p2_if_black(Color color);
void draw_objects();
void draw_attempt_text();
void draw_end_wall(float delta);
void draw_background(float x, float y);
void draw_ground(float cam_x, float cam_y, float y, bool is_ceiling, int screen_width);
void update_player_colors();
void set_player_colors(Color p1, Color p2, Color glow);
#ifdef __PSP__
void v2_sheet_need(bool need);
// the animated 2.0 objects' clock, while the level runs
void obj_anim_tick(float dt);
// the robot's animations (wiidash's, from GD's robot.plist): running, then
// a jump's start and its loop, a fall's start and its loop
enum { ROBOT_ANIM_RUN, ROBOT_ANIM_JUMP_START, ROBOT_ANIM_JUMP, ROBOT_ANIM_FALL_START, ROBOT_ANIM_FALL };
// draws the robot's parts posed for *anim at t seconds (a start that ended
// moves *anim on to its loop)
void spawn_robot_at(float x, float y, float deg, unsigned char flip_x, unsigned char flip_y,
                    float scale, u32 p1_color, u32 p2_color, u8 *anim, float t);
// the same, robot number id (1 to ROBOT_COUNT; the icon kit's) instead of the
// selected one
void spawn_robot_id_at(int id, float x, float y, float deg, unsigned char flip_x, unsigned char flip_y,
                       float scale, u32 p1_color, u32 p2_color, u8 *anim, float t);
// the icon kit's robots (robots.psheet: the levels with the robot and the icon
// kit load it)
void robots_sheet_need(bool need);
// GD's text objects: their text comes with the level (level_loading.c), they
// are drawn over the level's other objects
#define TEXT_OBJECT 914
void text_objects_clear(void);
void text_object_add(int obj, const char *b64, float scale_x, float scale_y);
void draw_text_objects(void);
#endif
void spawn_icon_at(
    int gamemode,
    int id,
    bool glow,
    float x,
    float y,
    float deg,
    unsigned char flip_x,
    unsigned char flip_y,
    float scale,
    u32 p1_color,
    u32 p2_color,
    u32 glow_color
);
void spawn_p1_layer_at(
    int gamemode,
    int id,
    float x,
    float y,
    float deg,
    unsigned char flip_x,
    unsigned char flip_y,
    float scale,
    u32 p1_color
);

void spawn_glow_layer_at(
    int gamemode,
    int id,
    float x,
    float y,
    float deg,
    unsigned char flip_x,
    unsigned char flip_y,
    float scale,
    u32 glow_color
);

Color get_color_abgr8(u32 color);

void handle_mirror_transition();

void draw_player_effects();
void draw_post_player_effects();
void draw_player_graphics();
int get_coin_texture(int tex, int ticks);

void update_touch_effect(float delta);
void draw_touch_effect();

void update_bottom_particles(float delta);
void draw_bottom_particles();