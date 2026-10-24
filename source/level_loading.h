#pragma once
#include "objects.h"
#include <3ds.h>

#define MAX_GROUPS_PER_OBJECT 20

#define SECTION_HASH_SIZE 1024

#define SECTION_SIZE 128

typedef struct {
    float h, s, v;
    bool sChecked, vChecked;
} HSV;

typedef enum {
    GD_VAL_INT,
    GD_VAL_FLOAT,
    GD_VAL_BOOL,
    GD_VAL_HSV,
    GD_VAL_UNKNOWN
} GDValueType;

typedef union {
    int i;
    float f;
    bool b;
    HSV hsv;
} GDValue;

typedef struct {
    int count;

    int *random;

    int *id;
    float *x, *y;
    float *rotation;
    int *zlayer, *zorder;
    float *trig_duration;
    float *opacity;

    float *width, *height;

    unsigned short *v1p9_col_channel;
    unsigned short *col_channel;
    unsigned short *detail_col_channel;
    unsigned short *target_color_id;

#ifdef __PSP__
    bool *main_col_HSV_enabled;
    bool *detail_col_HSV_enabled;
    HSV *main_col_HSV;
    HSV *detail_col_HSV;
#endif

    unsigned char *transition_applied;
    unsigned char *trig_colorR, *trig_colorG, *trig_colorB;
    unsigned char *orientation;
    unsigned char *hitbox_counter;
    bool *tintGround;
    bool *p1_color, *p2_color;
    bool *blending;
    union {
        bool *touch_triggered;
        u8 *coin_id;
    };
    bool *flippedH, *flippedV;
    bool *toggled;

    u8 *activated;
    u8 *collided;
} ObjectsArray;

typedef struct {
    int fromRed;
    int fromGreen;
    int fromBlue;
    int playerColor;
    bool blending;
    int channelID;
    int toRed;
    int toGreen;
    int toBlue;
#ifdef __PSP__
    bool v2;                    // from a 2.0 color string: these are set
    float opacity;              // 7
    int copy_id;                // 9
    float h, s, v;              // 10: "h a s a v a s_add a v_add"
    bool s_add, v_add;
#endif
} GDColorChannel;


typedef struct Section {
    int *objects;
    int object_count;
    int object_capacity;

    int x, y; // Section coordinates
    struct Section *next; // For chaining in hash map
} Section;

typedef struct {
    float last_obj_x;
    float wall_x;
    float wall_y;

    int pulsing_type;
    int song_id;
    int custom_song_id;
    float song_offset;
    bool completing;
    int background_id;
    int ground_id;
    int ground_line_id;
    int initial_gamemode;
#ifdef __PSP__
    bool initial_robot;
#endif
    bool initial_mini;
    unsigned char initial_speed;
    bool initial_dual;
    bool initial_upsidedown;

    char level_name[256];
    char creator_name[256];
} LoadedLevelInfo;

extern LoadedLevelInfo level_info;

extern const char *default_name;

// GD 2.0 added backgrounds 8-13 (the images are in wiidash, halved here)
#define BG_COUNT 13
// grounds 8-11 come in two layers: the top strip (ground_0N) and the fill
// under it (ground_0N_2), already here, just never used
#define G_COUNT 11
// the ground line (kA17): 1.9's and the two 2.0 added
#define GROUND_LINES 3

extern ObjectsArray objects;

char *read_file(const char *filepath, size_t *out_size);
char *decompress_level(char *data);

int load_level(char *path);
void reload_level();
void unload_level();

void fix_base64_url(char *b64);
int base64_decode(const char *in, unsigned char *out);

Section *get_section(int x, int y);
Section *get_or_create_section(int x, int y);
bool obj_has_main(const GameObject *obj);
bool obj_has_detail(const GameObject *obj);

// called for every object on screen, every frame, and by the collisions at
// 240 Hz: a call across files cost more than the two compares it does
static inline bool is_valid_object(int id) {
    return id >= 1 && id < GAME_OBJECT_COUNT;
}

#ifdef __PSP__
// a teleport portal's (747) way out, the 749 made for it at load (-1: none)
int teleport_exit(int obj);
#endif

char *get_level_name(char *data_ptr);
char *load_user_song(int id, size_t *out_size);
bool check_song(int id);
char *extract_gmd_key(const char *data, const char *key, const char *type);

char **split_string(const char *str, char delimiter, int *outCount, bool ignoreZeroLength);
char **split_string_str_del(const char *str, const char *delimiter, int *outCount, bool ignoreZeroLength);
void free_string_array(char **arr, int count);
char *get_metadata_value(const char *levelString, const char *key);