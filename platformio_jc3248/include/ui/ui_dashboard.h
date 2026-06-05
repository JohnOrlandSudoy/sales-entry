#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#include "lvgl.h"

void ui_dashboard_create(lv_obj_t * parent, void (*on_go_sales)(const char * employee_id));
void ui_dashboard_destroy(void);
void ui_dashboard_reload(void);

#endif
