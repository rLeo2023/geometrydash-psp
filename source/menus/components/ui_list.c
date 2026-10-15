#include "main.h"
#include "menus/core/ui_element.h"
#include <citro2d.h>
#include "ui_image.h"
#include "text.h"
#include "fonts/bigFont.h"
#include "ui_button.h"
#include "easing.h"
#include "math_helpers.h"
#include "menus/core/ui_screen.h"
#include "menus/core/ui_props.h"
#include "ui_list.h"
#include "utils/gfx.h"
#include <stdlib.h>

#ifdef __PSP__
#include "../../../psp/src/psp_port.h"
#endif

void ui_list_reset(UIList *list) {
    if (!list) return;
    
    list->scrollY = 0;

    UIElement *child = list->base.first_child;

    while (child) {
        UIElement *next = child->next_sibling;
        ui_destroy_tree(child);
        child = next;
    }

    list->base.first_child = NULL;
    list->base.last_child = NULL;
    
    list->contentHeight = 0;
    list->lastTouchY = 0;
}


void ui_list_add(UIList* list, UIElement* item) {
    if (!list) return;

    ui_element_add_child((UIElement *) list, item);

    list->contentHeight += item->h;
}

void ui_list_set_bg_color(UIList *list, u32 color) {
    if (!list) return;

    list->background_color = color;
}

static void ui_list_forward_touch(UIList *list, UIInput *input, UITransform *transform) {
    UITransform child = *transform;
    child.y += list->scrollY;

    float y = list->scrollY - list->base.h * 0.5f;

#ifdef __PSP__
    // the d-pad only goes to the rows that show
    float hw = list->base.w * 0.5f * transform->scaleX, hh = list->base.h * 0.5f * transform->scaleY;
    psp_list_clip(transform->x - hw, transform->y - hh, transform->x + hw, transform->y + hh);
#endif

    for (UIElement *item = list->base.first_child; item; item = item->next_sibling) {
        UITransform t = *transform;
#ifdef __PSP__
        // where ui_list_draw puts it: scaled like the list (a PSP layout's
        // @scale), or the touches land on the wrong rows
        t.y += (y + (item->h * 0.5f)) * transform->scaleY;
#else
        t.y += y + (item->h * 0.5f);
#endif

        ui_update_tree(item, input, &t);

        y += item->h;
    }

#ifdef __PSP__
    psp_list_clip_end();
#endif
}

static void ui_list_update(UIElement* e, UIInput* touch, UITransform *transform) {
    UIList* l = (UIList *) e;

    bool inside = ui_element_basic_bound_check(e, touch, transform);

    ui_list_forward_touch(l, touch, transform);
    if (inside) {  
        touch->did_something = true;

        // Exit if used something
        if (touch->interacted) return;
    }

    // Start dragging
    if (inside && (hidKeysDown() & KEY_TOUCH)) {
        l->dragging = true;
        l->lastTouchY = touch->touchPosition.py;
    }

    // Handle dragging
    if (l->dragging && (hidKeysHeld() & KEY_TOUCH)) {
        // Apply touch movement
        int delta = touch->touchPosition.py - l->lastTouchY;
        l->scrollY += delta;
        l->lastTouchY = touch->touchPosition.py;
    }

    circlePosition circlePad;
    //Joystick movement
    hidCircleRead(&circlePad);
    if(abs(circlePad.dy) > 48){
        l->scrollY += (circlePad.dy / 25);
    }

    if(hidKeysDown() & (KEY_UP | KEY_DOWN)){
        l->dpadHeldTime = 0;
    }

    if(l->dpadHeldTime > 60){
        l->dpadHeldTime = 60;
    }

    if(hidKeysHeld() & KEY_UP){
        l->scrollY += 4;
        if(l->dpadHeldTime >= 60){
            l->scrollY += 4;
        }

        l->dpadHeldTime++;
    }
    if(hidKeysHeld() & KEY_DOWN){
        l->scrollY -= 4;
        if(l->dpadHeldTime >= 60){
            l->scrollY -= 4;
        }

        l->dpadHeldTime++;
    }

    // Handle releasing dragging
    if (hidKeysUp() & KEY_TOUCH) {
        l->dragging = false;
    }

    // Clamp scrolling
    int minScroll = e->h - l->contentHeight;
    if (l->contentHeight <= e->h) {
        l->scrollY = 0;
    } else {
        if (l->scrollY > 0) l->scrollY = 0;
        if (l->scrollY < minScroll) l->scrollY = minScroll;
    }
}

static void ui_list_draw(UIElement* e, UITransform *transform) {
    UIList* l = (UIList *) e;

    float width = e->w * transform->scaleX;
    float height = e->h * transform->scaleY;

    float scissor_x = transform->x - (width / 2);
    float scissor_y = transform->y - (height / 2);

    // Draw background
    C2D_DrawRectSolid(scissor_x, scissor_y, 0, width, height, l->background_color);
    
    // Enable clipping
    set_scissor(GPU_SCISSOR_NORMAL, scissor_x, scissor_y, width, height);

    float y = l->scrollY - e->h * 0.5f;

    for (UIElement *item = e->first_child; item; item = item->next_sibling) {
        UITransform t = *transform;
        t.y += (y + (item->h * 0.5f)) * transform->scaleY;

        item->w = e->w;

        ui_draw_tree(item, &t);

        y += item->h;
    }

    // Disable scissor
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0,0,0,0);
    C2D_Prepare();
}

static void ui_list_destroy(UIElement *e) {
    if (e) {
        free(e);
        e = NULL;
    }
}

UIList *ui_create_list(const UIContext *ctx) {
    UIList *e = malloc(sizeof(UIList));

    if (!e) return NULL;

    memset(e, 0, sizeof(UIList));
    e->base.type = UI_LIST;
    e->base.enabled = true;

    e->base.update = ui_list_update;
    e->base.draw = ui_list_draw;
    e->base.destroy = ui_list_destroy;

    e->base.draws_children = true;
    
    ui_list_reset(e);

    return e;
}

UIElement *ui_create_list_from_props(const UIContext *ctx, const UIPropertyList *props) {
    UIList *list = ui_create_list(ctx);

    if (!list) return NULL;
    
    ui_element_apply_properties(&list->base, ctx, props);

    ui_list_set_bg_color(list, ui_prop_color(props, "bgColor", ABGR8(0, 0, 0, 0)));

    return &list->base;
}