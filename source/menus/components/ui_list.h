#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"

void ui_list_add(UIList* list, UIElement* item);
void ui_list_reset(UIList *list);
void ui_list_set_capacity(UIList *list, size_t capacity);

UIList *ui_create_list(const UIContext *ctx);
UIElement *ui_create_list_from_props(const UIContext *ctx, const UIPropertyList *props);