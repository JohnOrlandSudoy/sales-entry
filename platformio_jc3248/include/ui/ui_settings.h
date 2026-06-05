#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include "lvgl.h"

void ui_settings_create(lv_obj_t * parent, void (*on_back)(void *), void * user_data);
void ui_settings_destroy(void);

#endif
