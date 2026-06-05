#ifndef UI_WIFI_H
#define UI_WIFI_H

#include "lvgl.h"

typedef void (*ui_wifi_close_cb_t)(void * user_data);

/** Full-screen WiFi picker overlay (scan, password, connect). */
void ui_wifi_show(lv_obj_t * parent, ui_wifi_close_cb_t on_close, void * user_data);

void ui_wifi_hide(void);

bool ui_wifi_is_visible(void);

#endif
