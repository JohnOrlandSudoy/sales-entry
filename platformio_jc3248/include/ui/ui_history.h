#ifndef UI_HISTORY_H
#define UI_HISTORY_H

#include "lvgl.h"

void ui_history_create(lv_obj_t * parent, void (*on_back)(void *), void * user_data);
void ui_history_destroy(void);
void ui_history_reload(void);

#endif
