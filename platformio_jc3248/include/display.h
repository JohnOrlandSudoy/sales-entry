#pragma once

#include <Arduino_GFX_Library.h>

bool display_begin(void);
Arduino_GFX *display_get_gfx(void);
void display_flush(void);
void display_backlight_on(void);
void display_backlight_off(void);
void display_touch_wire_begin(void);

/** Logical size after rotation — must match LVGL hor_res/ver_res */
uint16_t display_width(void);
uint16_t display_height(void);
