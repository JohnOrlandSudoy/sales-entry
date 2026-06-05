#ifndef UI_TEXT_KEYBOARD_H
#define UI_TEXT_KEYBOARD_H

#include "lvgl.h"

/** Height of the bottom text keyboard strip (px). */
#define UI_TEXT_KB_HEIGHT  118

/**
 * Create a compact bottom keyboard panel (hidden by default).
 * Parent should be the settings (or screen) root — panel is full width, bottom aligned.
 */
lv_obj_t * ui_text_kb_create(lv_obj_t * parent);

typedef void (*ui_text_kb_close_cb_t)(void * user_data);

void ui_text_kb_set_textarea(lv_obj_t * panel, lv_obj_t * ta);
void ui_text_kb_set_visible(lv_obj_t * panel, bool visible);
void ui_text_kb_set_uppercase(lv_obj_t * panel, bool upper);
void ui_text_kb_set_close_cb(lv_obj_t * panel, ui_text_kb_close_cb_t cb, void * user_data);

#endif /* UI_TEXT_KEYBOARD_H */
