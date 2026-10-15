#pragma once
#include "menus/core/ui_element.h"
#include <citro2d.h>
#include "menus/core/ui_screen.h"

UIPaletteIcons *ui_create_palette_icons(const UIContext *ctx);
UIElement *ui_create_palette_icons_from_props(const UIContext *ctx, const UIPropertyList *props);