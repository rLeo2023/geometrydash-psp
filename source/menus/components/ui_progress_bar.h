#pragma once
#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"

void ui_progress_bar_set_tint(UIProgressBar* e, u32 color);
void ui_progress_bar_clear_tint(UIProgressBar* e);
UIProgressBar *ui_create_progress_bar(const UIContext *ctx);
UIElement *ui_create_progress_bar_from_props(const UIContext *ctx, const UIPropertyList *props);