#include "app_shell.h"
#include "ui_header.h"
#include "ui_keypad.h"
#include "ui_sales.h"
#include "ui_dashboard.h"
#include "ui_history.h"
#include "ui_master_pin.h"
#include "ui_email.h"
#include "ui_settings.h"
#include "ui_colors.h"
#include "app_data.h"
#include "app_storage.h"
#include "app_settings_defaults.h"
#include "app_display.h"
#include "app_sd_mount.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

#if LV_FONT_MONTSERRAT_10
  #define FONT_SM  (&lv_font_montserrat_10)
#else
  #define FONT_SM  (&lv_font_montserrat_12)
#endif

static lv_obj_t * s_scr;
static lv_obj_t * s_content;
static lv_obj_t * s_toast;
static lv_obj_t * s_toast_top;
static lv_timer_t * s_toast_top_timer;
static ui_header_t s_hdr;
static lv_timer_t * s_clk_timer;
static lv_timer_t * s_toast_timer;
static app_screen_t s_screen = APP_SCREEN_SALES;
static app_screen_t s_email_origin = APP_SCREEN_SALES;
static int s_email_result_sent_count;
static bool s_shell_ready;
static bool s_ui_unlocked;
static bool s_modal_open;

static void modal_overlay_begin(void)
{
    ui_keypad_release();
    ui_sales_pause_refresh(true);
    s_modal_open = true;
}

static void modal_overlay_end(void)
{
    s_modal_open = false;
    if(s_screen == APP_SCREEN_SALES)
        ui_sales_pause_refresh(false);
}

static void reload_active_screen(void);
static void show_toast(const char * msg);
static void go_to_sales(const char * employee_id);
static void settings_back_cb(void * ud);
static void settings_pin_success(void * ud);
static void settings_pin_cancel(void * ud);

static void destroy_current_screen(void)
{
    if(s_screen == APP_SCREEN_SALES) {
        ui_sales_destroy();
    }
    else if(s_screen == APP_SCREEN_DASH) {
        ui_dashboard_destroy();
    }
    else if(s_screen == APP_SCREEN_SETTINGS) {
        ui_settings_destroy();
    }
    else if(s_screen == APP_SCREEN_HISTORY) {
        ui_history_destroy();
    }
}

static void reload_active_screen(void)
{
    if(s_screen == APP_SCREEN_SALES) {
        ui_sales_reload();
    }
    else if(s_screen == APP_SCREEN_DASH) {
        ui_dashboard_reload();
    }
    else if(s_screen == APP_SCREEN_HISTORY) {
        ui_history_reload();
    }
}

static void toast_hide_cb(lv_timer_t * t)
{
    (void)t;
    if(s_toast) lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
}

static void toast_top_hide_cb(lv_timer_t * t)
{
    (void)t;
    if(s_toast_top) lv_obj_add_flag(s_toast_top, LV_OBJ_FLAG_HIDDEN);
}

/** Toast on LVGL top layer — visible above modals and tabs. */
static void show_toast_top(const char * msg)
{
    if(!msg || msg[0] == '\0') return;
    lv_obj_t * layer = lv_layer_top();
    if(!s_toast_top || !lv_obj_is_valid(s_toast_top)) {
        s_toast_top = lv_label_create(layer);
        lv_obj_set_width(s_toast_top, APP_SCREEN_W - 24);
        lv_obj_align(s_toast_top, LV_ALIGN_TOP_MID, 0, APP_HDR_H + 6);
        lv_obj_add_flag(s_toast_top, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_style_bg_color(s_toast_top, lv_color_hex(0x166534), 0);
        lv_obj_set_style_bg_opa(s_toast_top, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_toast_top, lv_color_hex(0x16A34A), 0);
        lv_obj_set_style_border_width(s_toast_top, 1, 0);
        lv_obj_set_style_radius(s_toast_top, 4, 0);
        lv_obj_set_style_pad_all(s_toast_top, 6, 0);
        lv_obj_set_style_text_color(s_toast_top, lv_color_hex(0x86EFAC), 0);
        lv_obj_set_style_text_font(s_toast_top, FONT_SM, 0);
        lv_obj_set_style_text_align(s_toast_top, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_label_set_text(s_toast_top, msg);
    lv_obj_clear_flag(s_toast_top, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_toast_top);
    if(s_toast_top_timer) lv_timer_reset(s_toast_top_timer);
    else s_toast_top_timer = lv_timer_create(toast_top_hide_cb, 6000, NULL);
}

static void show_toast(const char * msg)
{
    if(!s_toast || !msg || msg[0] == '\0') return;
    lv_label_set_text(s_toast, msg);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_toast);
    if(s_toast_timer) lv_timer_reset(s_toast_timer);
    else s_toast_timer = lv_timer_create(toast_hide_cb, 5000, NULL);
    show_toast_top(msg);
}

static void go_to_sales(const char * employee_id)
{
    if(employee_id && employee_id[0]) {
        app_set_sales_employee_id(employee_id);
    }
    destroy_current_screen();
    s_screen = APP_SCREEN_SALES;
    ui_header_set_screen(&s_hdr, APP_SCREEN_SALES);
    lv_obj_clean(s_content);
    ui_sales_create(s_content);
}

static void history_back_cb(void * ud)
{
    (void)ud;
    ui_history_destroy();
    destroy_current_screen();
    s_screen = APP_SCREEN_DASH;
    ui_header_set_screen(&s_hdr, APP_SCREEN_DASH);
    lv_obj_clean(s_content);
    ui_dashboard_create(s_content, go_to_sales);
}

static void shell_nav(app_screen_t screen, void * ud)
{
    (void)ud;
    if(screen == s_screen) return;

    destroy_current_screen();
    s_screen = screen;
    ui_header_set_screen(&s_hdr, screen);
    lv_obj_clean(s_content);

    if(screen == APP_SCREEN_SALES) {
        ui_sales_create(s_content);
    }
    else if(screen == APP_SCREEN_DASH) {
        ui_dashboard_create(s_content, go_to_sales);
    }
    else if(screen == APP_SCREEN_HISTORY) {
        ui_history_create(s_content, history_back_cb, NULL);
    }
}

static void shell_settings(void * ud)
{
    (void)ud;
    if(s_modal_open) return;
    modal_overlay_begin();
    ui_master_pin_show(s_scr, "MASTER PIN REQUIRED",
                       settings_pin_success, settings_pin_cancel, NULL);
}

static void settings_back_cb(void * ud)
{
    (void)ud;
    ui_settings_destroy();
    destroy_current_screen();
    s_screen = APP_SCREEN_DASH;
    ui_header_set_screen(&s_hdr, APP_SCREEN_DASH);
    lv_obj_clean(s_content);
    ui_dashboard_create(s_content, go_to_sales);
}

static void settings_pin_success(void * ud)
{
    (void)ud;
    modal_overlay_end();

    if(s_screen == APP_SCREEN_SALES)
        ui_sales_destroy();
    else
        destroy_current_screen();

    if(s_content && lv_obj_is_valid(s_content))
        lv_obj_clean(s_content);

    s_screen = APP_SCREEN_SETTINGS;
    ui_settings_create(s_content, settings_back_cb, NULL);
    ui_header_set_screen(&s_hdr, APP_SCREEN_SETTINGS);
}

static void settings_pin_cancel(void * ud)
{
    (void)ud;
    modal_overlay_end();
}

/** Restore tab that was active when invoice review opened (overlay hides underneath). */
static void email_restore_origin_screen(void)
{
    app_screen_t target = s_email_origin;
    if(target == APP_SCREEN_SETTINGS)
        target = APP_SCREEN_SALES;

    if(s_screen == APP_SCREEN_SETTINGS)
        ui_settings_destroy();

    if(s_screen == target) {
        reload_active_screen();
        return;
    }

    destroy_current_screen();
    s_screen = target;
    ui_header_set_screen(&s_hdr, target);
    lv_obj_clean(s_content);
    if(target == APP_SCREEN_SALES)
        ui_sales_create(s_content);
    else if(target == APP_SCREEN_DASH)
        ui_dashboard_create(s_content, go_to_sales);
}

static void email_result_apply(void)
{
    if(s_email_result_sent_count > 0) {
        if(s_screen == s_email_origin) {
            reload_active_screen();
        }
        else {
            email_restore_origin_screen();
        }
    }
    else {
        email_restore_origin_screen();
    }
}

static void email_result_ok_cb(void * ud)
{
    (void)ud;
    email_result_apply();
}

static void email_sent_cb(int sent_count, const char * msg, void * ud)
{
    (void)ud;
    s_email_result_sent_count = sent_count;

    printf("[email] UI result sent_count=%d msg=%s\n", sent_count,
           msg && msg[0] ? msg : "(none)");

    const char * title = (sent_count > 0) ? "EMAIL SENT" : "EMAIL FAILED";
    char body[160];
    if(msg && msg[0]) {
        strncpy(body, msg, sizeof(body) - 1);
        body[sizeof(body) - 1] = '\0';
    }
    else if(sent_count > 0) {
        snprintf(body, sizeof(body), "Sent %d invoice%s successfully.",
                 sent_count, sent_count > 1 ? "s" : "");
    }
    else {
        strcpy(body, "Check WiFi and SMTP settings, then try again.");
    }

    ui_email_review_hide_before_result();
    ui_email_result_show(NULL, sent_count > 0, title, body, email_result_ok_cb, NULL);
    show_toast_top(body);
    lv_refr_now(NULL);
}

static void email_review_close_cb(void * ud)
{
    (void)ud;
    email_restore_origin_screen();
}

/** True when sender, recipient (and SMTP password on device) are set in app settings. */
static bool email_config_ready(char * msg, size_t msg_sz)
{
    app_settings_t * st = app_settings();
    (void)app_settings_apply_defaults(st);
    if(msg && msg_sz > 0) msg[0] = '\0';

    if(!st->sender_email[0]) {
        if(msg && msg_sz > 0)
            snprintf(msg, msg_sz, "Settings: set sender email, then SAVE");
        return false;
    }
    if(!st->recipient_email[0]) {
        if(msg && msg_sz > 0)
            snprintf(msg, msg_sz, "Settings: set recipient email, then SAVE");
        return false;
    }
#if !APP_EMAIL_SIMULATE
    if(!st->smtp_app_password[0]) {
        if(msg && msg_sz > 0)
            snprintf(msg, msg_sz, "Settings: set SMTP app password, then SAVE");
        return false;
    }
#endif
    return true;
}

static void email_pin_success(void * ud)
{
    (void)ud;
    char hint[80];
    if(!email_config_ready(hint, sizeof(hint))) {
        modal_overlay_end();
        show_toast(hint);
        return;
    }
    s_email_origin = s_screen;
    modal_overlay_end();
    ui_email_review_show(s_scr, email_sent_cb, email_review_close_cb, NULL);
}

static void email_pin_cancel(void * ud)
{
    (void)ud;
    modal_overlay_end();
}

static void shell_email(void * ud)
{
    (void)ud;
    if(s_modal_open) return;
    char hint[80];
    if(!email_config_ready(hint, sizeof(hint))) {
        if(s_screen == APP_SCREEN_SETTINGS)
            show_toast(hint[0] ? hint : "Set emails below, then SAVE");
        else
            show_toast(hint);
        return;
    }
    modal_overlay_begin();
    ui_master_pin_show(s_scr, "MASTER PIN FOR EMAIL/INVOICE",
                       email_pin_success, email_pin_cancel, NULL);
}

static void clk_tick(lv_timer_t * t)
{
    (void)t;
    static char s_last_day[APP_DATE_LEN] = "";
    const char * today = app_today_date();
    if(s_last_day[0] && strcmp(s_last_day, today) != 0)
        reload_active_screen();
    strncpy(s_last_day, today, sizeof(s_last_day) - 1);
    s_last_day[sizeof(s_last_day) - 1] = '\0';
    app_clamp_selected_date();
    ui_header_refresh(&s_hdr);
}

static void shell_unlock_success(void * ud)
{
    (void)ud;
    s_ui_unlocked = true;
    app_shell_show();
}

static void shell_build_main_ui(void)
{
    s_hdr = ui_header_create(s_scr, APP_SCREEN_SALES, shell_nav, shell_settings, shell_email, NULL);
    ui_header_refresh(&s_hdr);

    s_content = lv_obj_create(s_scr);
    lv_obj_set_size(s_content, APP_SCREEN_W, APP_SCREEN_H - APP_HDR_H);
    lv_obj_set_pos(s_content, 0, APP_HDR_H);
    lv_obj_set_style_bg_color(s_content, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 0, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    s_toast = lv_label_create(s_scr);
    lv_obj_set_width(s_toast, APP_SCREEN_W - 24);
    lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, APP_HDR_H + 4);
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(0x166534), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_toast, lv_color_hex(0x16A34A), 0);
    lv_obj_set_style_border_width(s_toast, 1, 0);
    lv_obj_set_style_radius(s_toast, 4, 0);
    lv_obj_set_style_pad_all(s_toast, 6, 0);
    lv_obj_set_style_text_color(s_toast, lv_color_hex(0x86EFAC), 0);
    lv_obj_set_style_text_font(s_toast, FONT_SM, 0);
    lv_obj_set_style_text_align(s_toast, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);

    ui_sales_create(s_content);

    if(!app_sd_is_mounted()) {
        show_toast(app_sd_user_message());
        printf("[UI] %s\n", app_sd_user_message());
    }

    s_clk_timer = lv_timer_create(clk_tick, 1000, NULL);
}

void app_shell_show(void)
{
    if(s_shell_ready) return;

    s_scr = lv_scr_act();
    lv_obj_clean(s_scr);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);

    if(!s_ui_unlocked) {
        ui_master_pin_show_kind(s_scr, "SYSTEM LOCKED",
                                shell_unlock_success, NULL, NULL, UI_PIN_KIND_SYSTEM);
        return;
    }

    s_shell_ready = true;
    shell_build_main_ui();
}
