#ifndef UI_MASTER_PIN_H
#define UI_MASTER_PIN_H

#include "lvgl.h"

typedef void (*ui_master_pin_cb_t)(void * user_data);

typedef enum {
    UI_PIN_KIND_MASTER = 0,
    UI_PIN_KIND_SYSTEM,
} ui_pin_kind_t;

/**
 * Full-screen PIN overlay on @p parent (covers header + content).
 * @param kind UI_PIN_KIND_SYSTEM checks settings system_pin (boot lock);
 *             UI_PIN_KIND_MASTER checks master_pin (settings/email).
 */
void ui_master_pin_show_kind(lv_obj_t * parent, const char * label,
                             ui_master_pin_cb_t on_success, ui_master_pin_cb_t on_cancel,
                             void * user_data, ui_pin_kind_t kind);

void ui_master_pin_show(lv_obj_t * parent, const char * label,
                        ui_master_pin_cb_t on_success, ui_master_pin_cb_t on_cancel,
                        void * user_data);

void ui_master_pin_hide(void);

#endif
