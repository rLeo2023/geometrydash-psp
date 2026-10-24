#include "saving.h"
#include "main.h"
#include "utils/json_config.h"
#include <sys/stat.h>
#include <sys/types.h>

#include <stdint.h>
#include "level/main_levels.h"
#ifdef __PSP__
#include "menus/level_select.h"
#include "../psp/src/psp_port.h"
#endif

#include <stdio.h>
#include <dirent.h>

#include "math_helpers.h"

Config curr_level_config;
Config main_level_config[MAIN_LEVELS_NUM];

LevelData level_data;
LevelData main_level_data[MAIN_LEVELS_NUM];

int total_stars = 0;
int total_coins = 0;
int total_user_coins = 0;
int total_attempts = 0;
int total_jumps = 0;
int total_demons = 0;
int completed_main_levels = 0;
int completed_external_levels = 0;
int players_destroyed = 0;

uint64_t fnv1a64(const char* str) {
    uint64_t hash = 14695981039346656037ULL;

    while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 1099511628211ULL;
    }

    return hash;
}

static void init_default(Config *config) {
    config_init_int(config, DATA_ATTEMPTS, 0);
    config_init_int(config, DATA_JUMPS, 0);
    config_init_int(config, DATA_NORMAL, 0);
    config_init_int(config, DATA_PRACTICE, 0);
    config_init_int(config, DATA_STARS, 0);
    config_init_bool(config, DATA_COIN1, false);
    config_init_bool(config, DATA_COIN2, false);
    config_init_bool(config, DATA_COIN3, false);
}

static void init_values(Config *config, LevelData *level_data) {
    level_data->attempts = config_get_int(config, DATA_ATTEMPTS, 0);
    level_data->jumps = config_get_int(config, DATA_JUMPS, 0);
    level_data->normal_progress = config_get_int(config, DATA_NORMAL, 0);
    level_data->practice_progress = config_get_int(config, DATA_PRACTICE, 0);
    level_data->stars = config_get_int(config, DATA_STARS, 0);
    level_data->coin1 = config_get_bool(config, DATA_COIN1, false);
    level_data->coin2 = config_get_bool(config, DATA_COIN2, false);
    level_data->coin3 = config_get_bool(config, DATA_COIN3, false);
}

static void save_values(Config *config, LevelData *level_data) {
    config_set_int(config, DATA_ATTEMPTS, level_data->attempts);
    config_set_int(config, DATA_JUMPS, level_data->jumps);
    config_set_int(config, DATA_NORMAL, level_data->normal_progress);
    config_set_int(config, DATA_PRACTICE, level_data->practice_progress);
    config_set_int(config, DATA_STARS, level_data->stars);
    config_set_bool(config, DATA_COIN1, level_data->coin1);
    config_set_bool(config, DATA_COIN2, level_data->coin2);
    config_set_bool(config, DATA_COIN3, level_data->coin3);
}

void load_main_level_progress() {
    for (int i = 0; i < MAIN_LEVELS_NUM; i++) {
        char file[256];
        snprintf(file, sizeof(file), "main_%d", i);
        
        char tmp[512];
        mkdir(DATA_FOLDER, 0777);

        snprintf(tmp, sizeof(tmp), "%s%016llX.d", DATA_FOLDER, fnv1a64(file));

        config_load(&main_level_config[i], tmp);

        init_default(&main_level_config[i]);
        init_values(&main_level_config[i], &main_level_data[i]);

        config_save(&main_level_config[i]);
    }
}

void save_main_level_progress(int level) {
    save_values(&main_level_config[level], &main_level_data[level]);
    config_save(&main_level_config[level]);
}

void free_main_level_progress() {
    for (int i = 0; i < MAIN_LEVELS_NUM; i++) {
        config_free(&main_level_config[i]);
    }
}

void load_level_progress(char *filename) {
    char tmp[512];

    mkdir(DATA_FOLDER, 0777);

    snprintf(tmp, sizeof(tmp), "%s%016llX.d", DATA_FOLDER, fnv1a64(filename));

    // Do not load again
    if (curr_level_config.root) {
        free_level_progress();
    }

    config_load(&curr_level_config, tmp);

    init_default(&curr_level_config);
    init_values(&curr_level_config, &level_data);

    config_save(&curr_level_config);
}

void save_level_progress() {
    save_values(&curr_level_config, &level_data);

    config_save(&curr_level_config);
}

// A level's save, read into out and left alone: the level being played keeps
// its own (the selector's Extras cards show each level's progress)
void peek_level_progress(const char *filename, LevelData *out) {
    char tmp[512];
    Config config = { 0 };

    mkdir(DATA_FOLDER, 0777);

    snprintf(tmp, sizeof(tmp), "%s%016llX.d", DATA_FOLDER, fnv1a64(filename));

    config_load(&config, tmp);

    init_default(&config);
    init_values(&config, out);

    config_free(&config);
}

void free_level_progress() {
    config_free(&curr_level_config);
}

int is_main_level(const char *filename) {
    for (size_t i = 0; i < MAIN_LEVELS_NUM; i++) {
        char file[16];
        snprintf(file, sizeof(file), "main_%d", i);
        char tmp[17];
        snprintf(tmp, sizeof(tmp), "%016llX", fnv1a64(file));
        if (strncmp(filename, tmp, 16) == 0) {
            return 1;
        }
    }
    return 0;
}

#ifdef __PSP__
// the Extras level a save file belongs to, or -1 (they are not the user's)
static int extras_level_of_save(const char *filename) {
    int count = extras_levels_count();
    for (int i = 0; i < count; i++) {
        char key[512];
        char tmp[17];

        extras_level_save_key(i, key, sizeof(key));
        snprintf(tmp, sizeof(tmp), "%016llX", fnv1a64(key));

        if (strncmp(filename, tmp, 16) == 0) return i;
    }
    return -1;
}
#endif

void calculate_stats() {
    total_stars = 0;
    total_coins = 0;
    total_user_coins = 0;
    total_attempts = 0;
    total_jumps = 0;
    total_demons = 0;
    completed_main_levels = 0;
    completed_external_levels = 0;

    // Calculate main levels
    for (int i = 0; i < MAIN_LEVELS_NUM; i++) {
        LevelData *data = &main_level_data[i];
        total_attempts += data->attempts;
        total_jumps += data->jumps;

        if (data->normal_progress == 100) {
            completed_main_levels++;
            total_stars += main_levels[i].stars;
            total_coins += data->coin1;
            total_coins += data->coin2;
            total_coins += data->coin3;

            // Check for demon
            if (main_levels[i].difficulty == MAIN_DIFF_DEMON) {
                total_demons++;
            }
        }
    }

    // Now sdcard slop
    DIR *dir = opendir(DATA_FOLDER);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // Skip "." and ".."
            if (entry->d_name[0] == '.') continue;

            if (is_main_level(entry->d_name)) continue;
            
            char path[1024];
            snprintf(path, sizeof(path), "%s%s", DATA_FOLDER, entry->d_name);

            if (!config_load(&curr_level_config, path)) continue;

            init_default(&curr_level_config);
            init_values(&curr_level_config, &level_data);

            LevelData *data = &level_data;
            total_attempts += data->attempts;
            total_jumps += data->jumps;

            if (data->normal_progress == 100) {
                completed_external_levels++;

#ifdef __PSP__
                // the Extras levels are GD's own: gold coins, and the stars
                // from their definition (a save may predate them)
                int extras = extras_level_of_save(entry->d_name);
                if (extras >= 0) {
                    total_stars += extras_levels_stars(extras);
                    total_coins += data->coin1;
                    total_coins += data->coin2;
                    total_coins += data->coin3;
                } else
#endif
                {
                    total_stars += data->stars;

                    // the custom levels' coins are the user coins here
                    total_user_coins += data->coin1;
                    total_user_coins += data->coin2;
                    total_user_coins += data->coin3;

                    // Check for demon
                    if (data->stars == 10) {
                        total_demons++;
                    }
                }
            }

            config_free(&curr_level_config);
        }

        closedir(dir);
    }
}