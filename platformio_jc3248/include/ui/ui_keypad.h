#ifndef UI_KEYPAD_H
#define UI_KEYPAD_H

#include "lvgl.h"
#include <stdbool.h>

/** Return true to consume the key (skip default append/delete). */
typedef bool (*ui_keypad_intercept_fn)(const char * key);

typedef struct {
    lv_obj_t * root;
    lv_obj_t * display_lbl;
    char * value_buf;
    size_t value_buf_sz;
    int max_len;
    bool show_decimal;
    const char * placeholder;
} ui_keypad_t;

/**
 * Numeric keypad matching React NumericKeypad (1–9, ., 0, backspace).
 * @param fill_height true = stretch 4 rows to parent (Sales screen); false = fixed key size
 */
ui_keypad_t ui_keypad_create(lv_obj_t * parent, char * value_buf, size_t buf_sz,
                             bool show_decimal, int max_len, const char * placeholder,
                             bool fill_height, bool show_display);

void ui_keypad_refresh(ui_keypad_t * kp);

/** Point keypad input at a different buffer (required when switching fields). */
void ui_keypad_bind(char * value_buf, int max_len, const char * placeholder);

/** Detach from screen buffers when screen is destroyed (prevents crash on tab switch). */
void ui_keypad_release(void);

/** Optional: barcode→price handoff, PIN-only keys, etc. */
void ui_keypad_set_intercept(ui_keypad_intercept_fn fn);

/** Called after each key updates the bound buffer (e.g. refresh BC/PRICE labels). */
typedef void (*ui_keypad_change_fn)(void);
void ui_keypad_set_change_cb(ui_keypad_change_fn fn);

#endif
