#pragma once
#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"

void ui_window_set_atlas(UIWindow* e, int index);
void ui_window_set_tint(UIWindow* e, u32 color);
UIWindow *ui_create_window(const UIContext *ctx);
UIElement *ui_create_window_from_props(const UIContext *ctx, const UIPropertyList *props);