#pragma once

#include <stdbool.h>
#include "level/main_levels.h"

#define DATA_ATTEMPTS "attempts"
#define DATA_JUMPS "jumps"
#define DATA_NORMAL "normal"
#define DATA_PRACTICE "practice"
#define DATA_COIN1 "coin1"
#define DATA_COIN2 "coin2"
#define DATA_COIN3 "coin3"
#define DATA_STARS "stars"

typedef struct LevelData {
    int attempts;
    int jumps;
    int normal_progress;
    int practice_progress;
    int stars;
    bool coin1;
    bool coin2;
    bool coin3;
} LevelData;

extern LevelData level_data;
extern LevelData main_level_data[MAIN_LEVELS_NUM];

extern int total_stars;
extern int total_coins;
extern int total_attempts;
extern int total_jumps;
extern int total_demons;
extern int completed_main_levels;
extern int completed_external_levels;
extern int players_destroyed;

void save_level_progress();
void load_level_progress(char *filename);
void free_level_progress();

void load_main_level_progress();
void save_main_level_progress(int level);
void free_main_level_progress();

void calculate_stats();