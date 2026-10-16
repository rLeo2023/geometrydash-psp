#pragma once
#include <3ds.h>

void info_card_init();
void info_card_init_layout(const char *layout);
void set_info_content(const char *text);
int info_card_loop();
