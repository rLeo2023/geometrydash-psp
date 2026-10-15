#pragma once
#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"
#define TEXTBOX_STYLE 2
#define TEXTBOX_MARGIN 10

UITextbox *ui_create_textbox(const UIContext *ctx);
UIElement *ui_create_textbox_from_props(const UIContext *ctx, const UIPropertyList *props);