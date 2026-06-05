#ifndef UI_HEADER_H
#define UI_HEADER_H

#include "app/app_types.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t * root;
    lv_obj_t * clock_lbl;
    lv_obj_t * date_lbl;
    lv_obj_t * tab_sales;
    lv_obj_t * tab_dash;
    lv_obj_t * tab_hist;
    lv_obj_t * btn_settings;
    lv_obj_t * btn_email;
    app_screen_t current;
    void (*on_nav)(app_screen_t screen, void * user);
    void (*on_settings)(void * user);
    void (*on_email)(void * user);
    void * user_data;
} ui_header_t;

ui_header_t ui_header_create(lv_obj_t * parent, app_screen_t current,
                             void (*on_nav)(app_screen_t, void *),
                             void (*on_settings)(void *),
                             void (*on_email)(void *),
                             void * user_data);

void ui_header_set_screen(ui_header_t * hdr, app_screen_t screen);
void ui_header_refresh(ui_header_t * hdr);
void ui_header_set_date_changed_cb(void (*cb)(void));

#endif
