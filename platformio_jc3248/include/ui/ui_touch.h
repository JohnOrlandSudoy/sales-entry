#ifndef UI_TOUCH_H
#define UI_TOUCH_H

#include "lvgl.h"

/** Larger tap area without changing visual size (LVGL 8). */
static inline void ui_touch_expand(lv_obj_t * obj, lv_coord_t pad)
{
    if(obj && lv_obj_is_valid(obj))
        lv_obj_set_ext_click_area(obj, pad);
}

#endif
