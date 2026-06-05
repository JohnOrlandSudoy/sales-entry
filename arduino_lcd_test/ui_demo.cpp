#include "ui_demo.h"
#include <lvgl.h>

static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_status = nullptr;
static lv_obj_t *s_bar = nullptr;
static int s_count = 0;

static void on_tap_btn(lv_event_t *e)
{
    (void)e;
    s_count++;
    if (s_count > 100) {
        s_count = 0;
    }

    lv_bar_set_value(s_bar, s_count, LV_ANIM_ON);

    char buf[48];
    lv_snprintf(buf, sizeof(buf), "Taps: %d", s_count);
    lv_label_set_text(s_status, buf);
}

static void on_switch(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    const bool on = (lv_obj_get_state(sw) & LV_STATE_CHECKED) != 0;

    if (on) {
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x1a2744), 0);
        lv_label_set_text(s_title, "Sales Entry - Test");
    } else {
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x202020), 0);
        lv_label_set_text(s_title, "DIYMORE LCD Test");
    }
}

void ui_demo_create(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    s_title = lv_label_create(lv_scr_act());
    lv_label_set_text(s_title, "DIYMORE LCD Test");
    lv_obj_set_style_text_color(s_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *hint = lv_label_create(lv_scr_act());
    lv_label_set_text(hint, "Touch Tap me below");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xBBBBBB), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 200, 48);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -30);
    lv_obj_add_event_cb(btn, on_tap_btn, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Tap me");
    lv_obj_center(btn_lbl);

    s_bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(s_bar, 220, 16);
    lv_obj_align(s_bar, LV_ALIGN_CENTER, 0, 30);
    lv_bar_set_range(s_bar, 0, 100);

    lv_obj_t *sw = lv_switch_create(lv_scr_act());
    lv_obj_align(sw, LV_ALIGN_CENTER, 0, 75);

    s_status = lv_label_create(lv_scr_act());
    lv_label_set_text(s_status, "Taps: 0");
    lv_obj_set_style_text_color(s_status, lv_color_hex(0x4ADE80), 0);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_MID, 0, -28);

    lv_obj_invalidate(lv_scr_act());
}
