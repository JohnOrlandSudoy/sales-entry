#ifndef UI_SALES_H
#define UI_SALES_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create sales entry screen inside parent (use full content area). */
void ui_sales_create(lv_obj_t * parent);

/** Stop timers and detach keypad before destroying parent (tab switch). */
void ui_sales_destroy(void);

/** Reload when date, employee, or external data changes. */
void ui_sales_reload(void);

/** Pause 200ms keypad refresh while a modal overlay is open. */
void ui_sales_pause_refresh(bool pause);

#ifdef __cplusplus
}
#endif

#endif
