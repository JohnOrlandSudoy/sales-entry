#include "ui_settings.h"
#include "ui_colors.h"
#include "ui_keypad.h"
#include "ui_master_pin.h"
#include "ui_text_kb.h"
#include "ui_wifi.h"
#include "app_data.h"
#include "app_storage.h"
#include "app_wifi.h"
#include "app_display.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

#if LV_FONT_MONTSERRAT_10
  #define FONT_SM  (&lv_font_montserrat_10)
#else
  #define FONT_SM  (&lv_font_montserrat_12)
#endif

#define PIN_W       160
#define PIN_BUF_SZ  8
#if APP_PORTRAIT
  #define PIN_PANEL_H  200
#endif

typedef enum {
    PIN_EDIT_NONE = 0,
    PIN_EDIT_SYSTEM,
    PIN_EDIT_MASTER,
} pin_edit_t;

typedef struct {
    lv_obj_t * root;
    lv_obj_t * left;
    lv_obj_t * scroll;
    lv_obj_t * scroll_body;
    lv_obj_t * text_kb_panel;
    lv_obj_t * right_panel;
    lv_obj_t * emp_list;
    lv_obj_t * dates_wrap;
    lv_obj_t * dates_list;
    lv_obj_t * promo_ta;
    lv_obj_t * email_ta;
    lv_obj_t * smtp_pass_ta;
    lv_obj_t * recipient_ta;
    lv_obj_t * new_emp_ta;
    lv_obj_t * wifi_btn;
    lv_obj_t * sys_pin_btn;
    lv_obj_t * master_pin_btn;
    lv_obj_t * msg_lbl;
    lv_obj_t * active_ta;
    ui_keypad_t keypad;
    app_settings_t * draft;
    char pin_buf[PIN_BUF_SZ];
    pin_edit_t editing_pin;
    pin_edit_t pending_pin;
    void (*on_back)(void *);
    void * user_data;
} settings_ui_t;

static settings_ui_t s;
static bool s_kb_guard;

static app_settings_t * draft_alloc(void)
{
#ifdef ESP_PLATFORM
    app_settings_t * p = (app_settings_t *)heap_caps_malloc(sizeof(app_settings_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if(!p) p = (app_settings_t *)malloc(sizeof(app_settings_t));
#else
    app_settings_t * p = (app_settings_t *)malloc(sizeof(app_settings_t));
#endif
    return p;
}

static void draft_free(app_settings_t * p)
{
    if(!p) return;
#ifdef ESP_PLATFORM
    heap_caps_free(p);
#else
    free(p);
#endif
}

static void sanitize_draft(void)
{
    if(!s.draft) return;
    if(s.draft->employee_count < 0 || s.draft->employee_count > APP_MAX_EMPLOYEES)
        s.draft->employee_count = 2;
    if(s.draft->sent_date_count < 0 || s.draft->sent_date_count > APP_MAX_SENT_DATES)
        s.draft->sent_date_count = 0;
    if(s.draft->sent_key_count < 0 || s.draft->sent_key_count > APP_MAX_SENT_KEYS)
        s.draft->sent_key_count = 0;
}

static void pin_done_cb(lv_event_t * e);
static void emp_del_cb(lv_event_t * e);
static void unlock_date_cb(lv_event_t * e);
static void refresh_pin_labels(void);
static void refresh_pin_panel(void);
static void refresh_employees(void);
static void refresh_dates(void);

static void set_msg(const char * txt, bool ok)
{
    if(!s.msg_lbl) return;
    lv_label_set_text(s.msg_lbl, txt ? txt : "");
    lv_obj_set_style_text_color(s.msg_lbl, lv_color_hex(ok ? COL_GREEN : COL_RED), 0);
}

static void refresh_layout(void);
static void hide_text_kb(void);
static void show_text_kb(lv_obj_t * ta);
static void ta_tap_cb(lv_event_t * e);
static void ensure_text_kb(void);
static void pin_master_ok(void * ud);
static void pin_master_cancel(void * ud);
static void text_kb_closed(void * ud);
static void wifi_open_cb(lv_event_t * e);
static void wifi_closed_cb(void * ud);
static void refresh_wifi_btn_label(void);

static void refresh_layout(void)
{
#if APP_PORTRAIT
    lv_obj_set_width(s.left, APP_SCREEN_W);
    int pad_bottom = 0;
    if(s.active_ta && s.text_kb_panel &&
       !lv_obj_has_flag(s.text_kb_panel, LV_OBJ_FLAG_HIDDEN)) {
        pad_bottom = UI_TEXT_KB_HEIGHT;
    }
    else if(s.editing_pin != PIN_EDIT_NONE) {
        pad_bottom = PIN_PANEL_H;
    }
    lv_obj_set_style_pad_bottom(s.left, pad_bottom, 0);
#else
    int side = (s.editing_pin != PIN_EDIT_NONE) ? PIN_W : 0;
    lv_obj_set_width(s.left, APP_SCREEN_W - side);
    if(s.active_ta && s.text_kb_panel &&
       !lv_obj_has_flag(s.text_kb_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_set_style_pad_bottom(s.left, UI_TEXT_KB_HEIGHT, 0);
    }
    else {
        lv_obj_set_style_pad_bottom(s.left, 0, 0);
    }
#endif
}

static void text_kb_closed(void * ud)
{
    (void)ud;
    hide_text_kb();
}

static void ensure_text_kb(void)
{
    if(s.text_kb_panel || !s.root) return;
    s.text_kb_panel = ui_text_kb_create(s.root);
    ui_text_kb_set_close_cb(s.text_kb_panel, text_kb_closed, NULL);
}

static void hide_text_kb(void)
{
    if(s_kb_guard) return;
    s_kb_guard = true;
    s.active_ta = NULL;
    if(s.text_kb_panel) ui_text_kb_set_visible(s.text_kb_panel, false);
    if(s.editing_pin == PIN_EDIT_NONE) refresh_layout();
    s_kb_guard = false;
}

static void show_text_kb(lv_obj_t * ta)
{
    if(s_kb_guard || s.editing_pin != PIN_EDIT_NONE || !ta) return;
    s_kb_guard = true;
    ensure_text_kb();
    if(s.active_ta == ta && s.text_kb_panel &&
       !lv_obj_has_flag(s.text_kb_panel, LV_OBJ_FLAG_HIDDEN)) {
        s_kb_guard = false;
        return;
    }
    s.active_ta = ta;
    if(s.text_kb_panel) {
        ui_text_kb_set_uppercase(s.text_kb_panel,
            ta != s.email_ta && ta != s.recipient_ta && ta != s.smtp_pass_ta);
        ui_text_kb_set_textarea(s.text_kb_panel, ta);
        ui_text_kb_set_visible(s.text_kb_panel, true);
        lv_obj_move_foreground(s.text_kb_panel);
    }
    refresh_layout();
    s_kb_guard = false;
}

static void ta_tap_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED)
        show_text_kb(lv_event_get_target(e));
}

static lv_obj_t * make_ta(lv_obj_t * parent, const char * init, const char * ph, lv_color_t text_col)
{
    lv_obj_t * ta = lv_textarea_create(parent);
    lv_obj_set_width(ta, LV_PCT(100));
    lv_obj_set_height(ta, 24);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_radius(ta, 4, 0);
    lv_obj_set_style_text_color(ta, text_col, 0);
    lv_obj_set_style_text_font(ta, FONT_SM, 0);
    lv_obj_set_style_pad_all(ta, 4, 0);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 48);
    if(init && init[0]) lv_textarea_set_text(ta, init);
    if(ph) lv_textarea_set_placeholder_text(ta, ph);
    lv_obj_add_flag(ta, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ta, ta_tap_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ta, ta_tap_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ta, ta_tap_cb, LV_EVENT_FOCUSED, NULL);
    return ta;
}

static void refresh_pin_labels(void)
{
    if(!s.sys_pin_btn || !s.master_pin_btn) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "System: %s", s.draft->system_pin);
    lv_obj_t * sl = lv_obj_get_child(s.sys_pin_btn, 0);
    if(sl) lv_label_set_text(sl, buf);
    snprintf(buf, sizeof(buf), "Master: %s", s.draft->master_pin);
    lv_obj_t * ml = lv_obj_get_child(s.master_pin_btn, 0);
    if(ml) lv_label_set_text(ml, buf);
}

static void refresh_pin_panel(void)
{
    bool show = s.editing_pin != PIN_EDIT_NONE;
    if(show) {
        hide_text_kb();
        lv_obj_clear_flag(s.right_panel, LV_OBJ_FLAG_HIDDEN);
#if APP_PORTRAIT
        lv_obj_move_foreground(s.right_panel);
#endif
    }
    else {
        lv_obj_add_flag(s.right_panel, LV_OBJ_FLAG_HIDDEN);
        ui_keypad_release();
    }
    refresh_layout();

    if(!show) {
        lv_obj_set_style_bg_color(s.sys_pin_btn, lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_border_color(s.sys_pin_btn, lv_color_hex(COL_BORDER), 0);
        lv_obj_set_style_bg_color(s.master_pin_btn, lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_border_color(s.master_pin_btn, lv_color_hex(COL_BORDER), 0);
        return;
    }

    const char * ph = (s.editing_pin == PIN_EDIT_SYSTEM) ? "SYSTEM PIN" : "MASTER PIN";
    char * pin_dst = (s.editing_pin == PIN_EDIT_SYSTEM) ? s.draft->system_pin : s.draft->master_pin;
    strncpy(s.pin_buf, pin_dst, sizeof(s.pin_buf) - 1);
    s.pin_buf[sizeof(s.pin_buf) - 1] = '\0';

    lv_obj_clean(s.right_panel);
    lv_obj_set_style_pad_all(s.right_panel, 6, 0);
    lv_obj_set_flex_flow(s.right_panel, LV_FLEX_FLOW_COLUMN);

    lv_obj_t * kp_wrap = lv_obj_create(s.right_panel);
    lv_obj_set_width(kp_wrap, LV_PCT(100));
    lv_obj_set_flex_grow(kp_wrap, 1);
    lv_obj_set_style_bg_opa(kp_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kp_wrap, 0, 0);
    lv_obj_set_style_pad_all(kp_wrap, 0, 0);
    lv_obj_clear_flag(kp_wrap, LV_OBJ_FLAG_SCROLLABLE);

    s.keypad = ui_keypad_create(kp_wrap, s.pin_buf, sizeof(s.pin_buf), false, 4, ph, true, true);

    lv_obj_t * done_btn = lv_btn_create(s.right_panel);
    lv_obj_set_width(done_btn, LV_PCT(100));
    lv_obj_set_height(done_btn, 26);
    lv_obj_set_style_bg_color(done_btn, lv_color_hex(0x14532D), 0);
    lv_obj_set_style_border_color(done_btn, lv_color_hex(0x166534), 0);
    lv_obj_set_style_border_width(done_btn, 1, 0);
    lv_obj_add_event_cb(done_btn, pin_done_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * dl = lv_label_create(done_btn);
    lv_label_set_text(dl, "ENTER");
    lv_obj_set_style_text_font(dl, FONT_SM, 0);
    lv_obj_center(dl);

    if(s.editing_pin == PIN_EDIT_SYSTEM) {
        lv_obj_set_style_bg_color(s.sys_pin_btn, lv_color_hex(0x164E63), 0);
        lv_obj_set_style_border_color(s.sys_pin_btn, lv_color_hex(COL_CYAN), 0);
        lv_obj_set_style_bg_color(s.master_pin_btn, lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_border_color(s.master_pin_btn, lv_color_hex(COL_BORDER), 0);
    }
    else {
        lv_obj_set_style_bg_color(s.master_pin_btn, lv_color_hex(0x164E63), 0);
        lv_obj_set_style_border_color(s.master_pin_btn, lv_color_hex(COL_CYAN), 0);
        lv_obj_set_style_bg_color(s.sys_pin_btn, lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_border_color(s.sys_pin_btn, lv_color_hex(COL_BORDER), 0);
    }
}

static void pin_done_cb(lv_event_t * e)
{
    (void)e;
    if(s.editing_pin == PIN_EDIT_SYSTEM) {
        strncpy(s.draft->system_pin, s.pin_buf, sizeof(s.draft->system_pin) - 1);
        s.draft->system_pin[sizeof(s.draft->system_pin) - 1] = '\0';
        if(s.draft->system_pin[0] == '\0') strcpy(s.draft->system_pin, "1234");
    }
    else if(s.editing_pin == PIN_EDIT_MASTER) {
        strncpy(s.draft->master_pin, s.pin_buf, sizeof(s.draft->master_pin) - 1);
        s.draft->master_pin[sizeof(s.draft->master_pin) - 1] = '\0';
        if(s.draft->master_pin[0] == '\0') strcpy(s.draft->master_pin, "9999");
    }
    s.editing_pin = PIN_EDIT_NONE;
    refresh_pin_panel();
    refresh_pin_labels();
}

static void pin_master_ok(void * ud)
{
    (void)ud;
    s.editing_pin = s.pending_pin;
    s.pending_pin = PIN_EDIT_NONE;
    refresh_pin_panel();
    refresh_pin_labels();
}

static void pin_master_cancel(void * ud)
{
    (void)ud;
    s.pending_pin = PIN_EDIT_NONE;
}

static void pin_toggle_cb(lv_event_t * e)
{
    pin_edit_t which = (pin_edit_t)(intptr_t)lv_event_get_user_data(e);
    if(s.editing_pin == which) {
        s.editing_pin = PIN_EDIT_NONE;
        refresh_pin_panel();
        return;
    }
    s.pending_pin = which;
    lv_obj_t * scr = lv_obj_get_screen(s.root);
    ui_master_pin_show(scr, "HEAD ONLY - MASTER PIN FOR PASSWORDS",
                       pin_master_ok, pin_master_cancel, NULL);
}

static void refresh_employees(void)
{
    if(!s.emp_list) return;
    lv_obj_clean(s.emp_list);

    if(s.draft->employee_count == 0) {
        lv_obj_t * empty = lv_label_create(s.emp_list);
        lv_label_set_text(empty, "No employees");
        lv_obj_set_style_text_color(empty, lv_color_hex(COL_GRAY), 0);
        lv_obj_set_style_text_font(empty, FONT_SM, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty, LV_PCT(100));
        return;
    }

    for(int i = 0; i < s.draft->employee_count; i++) {
        lv_obj_t * row = lv_obj_create(s.emp_list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 18);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * nm = lv_label_create(row);
        lv_label_set_text(nm, s.draft->employees[i].name);
        lv_obj_set_style_text_color(nm, lv_color_hex(COL_GREEN), 0);
        lv_obj_set_style_text_font(nm, FONT_SM, 0);

        lv_obj_t * del = lv_btn_create(row);
        lv_obj_set_size(del, 22, 16);
        lv_obj_set_style_bg_opa(del, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(del, 0, 0);
        lv_obj_add_event_cb(del, emp_del_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t * dl = lv_label_create(del);
        lv_label_set_text(dl, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(dl, lv_color_hex(COL_RED), 0);
        lv_obj_set_style_text_font(dl, FONT_SM, 0);
        lv_obj_center(dl);
    }
}

static void refresh_dates(void)
{
    if(!s.dates_wrap || !s.dates_list) return;
    if(s.draft->sent_date_count == 0) {
        lv_obj_add_flag(s.dates_wrap, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s.dates_wrap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(s.dates_list);

    for(int i = 0; i < s.draft->sent_date_count; i++) {
        lv_obj_t * row = lv_obj_create(s.dates_list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 18);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * dt = lv_label_create(row);
        lv_label_set_text(dt, s.draft->sent_dates[i]);
        lv_obj_set_style_text_color(dt, lv_color_hex(COL_GREEN), 0);
        lv_obj_set_style_text_font(dt, FONT_SM, 0);

        lv_obj_t * un = lv_btn_create(row);
        lv_obj_set_size(un, LV_SIZE_CONTENT, 16);
        lv_obj_set_style_bg_opa(un, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(un, 0, 0);
        lv_obj_add_event_cb(un, unlock_date_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t * ul = lv_label_create(un);
        lv_label_set_text(ul, "Unlock");
        lv_obj_set_style_text_color(ul, lv_color_hex(COL_ORANGE), 0);
        lv_obj_set_style_text_font(ul, FONT_SM, 0);
        lv_obj_center(ul);
    }
}

static void emp_del_cb(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if(idx < 0 || idx >= s.draft->employee_count) return;
    for(int j = idx; j < s.draft->employee_count - 1; j++) {
        s.draft->employees[j] = s.draft->employees[j + 1];
    }
    s.draft->employee_count--;
    refresh_employees();
}

static void unlock_date_cb(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if(idx < 0 || idx >= s.draft->sent_date_count) return;
    char date[APP_DATE_LEN];
    strncpy(date, s.draft->sent_dates[idx], sizeof(date) - 1);
    date[sizeof(date) - 1] = '\0';
    app_unlock_sent_date(s.draft, date);
    char msg[48];
    snprintf(msg, sizeof(msg), "Unlocked %s - save to apply", date);
    set_msg(msg, false);
    refresh_dates();
}

static void add_emp_cb(lv_event_t * e)
{
    (void)e;
    const char * name = lv_textarea_get_text(s.new_emp_ta);
    while(name[0] == ' ') name++;
    if(name[0] == '\0') return;
    if(app_employee_name_exists(s.draft, name, NULL)) {
        set_msg("Name exists", false);
        return;
    }
    if(s.draft->employee_count >= APP_MAX_EMPLOYEES) {
        set_msg("Max employees", false);
        return;
    }
    app_employee_t * emp = &s.draft->employees[s.draft->employee_count++];
    app_gen_id(emp->id, sizeof(emp->id));
    strncpy(emp->name, name, APP_NAME_LEN - 1);
    emp->name[APP_NAME_LEN - 1] = '\0';
    lv_textarea_set_text(s.new_emp_ta, "");
    set_msg("", true);
    refresh_employees();
}

static void save_cb(lv_event_t * e)
{
    (void)e;
    const char * promo = lv_textarea_get_text(s.promo_ta);
    const char * email = lv_textarea_get_text(s.email_ta);
    strncpy(s.draft->promo_head_name, promo, APP_NAME_LEN - 1);
    s.draft->promo_head_name[APP_NAME_LEN - 1] = '\0';
    if(s.draft->promo_head_name[0] == '\0') strcpy(s.draft->promo_head_name, "Ms. Helen");
    strncpy(s.draft->sender_email, email, sizeof(s.draft->sender_email) - 1);
    s.draft->sender_email[sizeof(s.draft->sender_email) - 1] = '\0';
    const char * recip = lv_textarea_get_text(s.recipient_ta);
    strncpy(s.draft->recipient_email, recip, sizeof(s.draft->recipient_email) - 1);
    s.draft->recipient_email[sizeof(s.draft->recipient_email) - 1] = '\0';
    const char * smtp_pass = lv_textarea_get_text(s.smtp_pass_ta);
    strncpy(s.draft->smtp_app_password, smtp_pass, sizeof(s.draft->smtp_app_password) - 1);
    s.draft->smtp_app_password[sizeof(s.draft->smtp_app_password) - 1] = '\0';
    if(s.draft->system_pin[0] == '\0') strcpy(s.draft->system_pin, "1234");
    if(s.draft->master_pin[0] == '\0') strcpy(s.draft->master_pin, "9999");

    if(app_apply_settings(s.draft)) {
        if(app_storage_last_settings_on_sd()) {
            set_msg("All saved to SD card!", true);
        } else {
            set_msg("Saved OK (device memory — SD not seen)", true);
        }
    }
    else {
        char err[72];
        snprintf(err, sizeof(err), "Save failed: %s", app_storage_last_error());
        set_msg(err, false);
    }
}

static void back_cb(lv_event_t * e)
{
    (void)e;
    if(s.on_back) s.on_back(s.user_data);
}

static void wifi_closed_cb(void * ud)
{
    (void)ud;
    if(s.draft) {
        app_wifi_get_saved(s.draft->wifi_ssid, sizeof(s.draft->wifi_ssid),
                           s.draft->wifi_password, sizeof(s.draft->wifi_password));
    }
    refresh_wifi_btn_label();
}

static void refresh_wifi_btn_label(void)
{
    if(!s.wifi_btn) return;
    lv_obj_t * lbl = lv_obj_get_child(s.wifi_btn, 0);
    if(!lbl) return;
    char st[72];
    app_wifi_status_line(st, sizeof(st));
    char buf[96];
    snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s", st);
    lv_label_set_text(lbl, buf);
}

static void wifi_open_cb(lv_event_t * e)
{
    (void)e;
    hide_text_kb();
    s.editing_pin = PIN_EDIT_NONE;
    refresh_pin_panel();
    lv_obj_t * scr = lv_obj_get_screen(s.root);
    ui_wifi_show(scr, wifi_closed_cb, NULL);
}

void ui_settings_destroy(void)
{
    ui_wifi_hide();
    ui_keypad_release();
    s_kb_guard = false;
    s.active_ta = NULL;
    draft_free(s.draft);
    s.draft = NULL;
    if(s.root && lv_obj_is_valid(s.root)) {
        lv_obj_del(s.root);
        s.root = NULL;
    }
    memset(&s, 0, sizeof(s));
}

void ui_settings_create(lv_obj_t * parent, void (*on_back)(void *), void * user_data)
{
    ui_settings_destroy();
    s.on_back = on_back;
    s.user_data = user_data;
    s.draft = draft_alloc();
    if(s.draft) *s.draft = *app_settings();
    sanitize_draft();
    s.editing_pin = PIN_EDIT_NONE;
    s.pin_buf[0] = '\0';

    s.root = lv_obj_create(parent);
    lv_obj_set_size(s.root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s.root, lv_color_hex(0x030712), 0);
    lv_obj_set_style_bg_opa(s.root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s.root, 0, 0);
    lv_obj_set_style_pad_all(s.root, 0, 0);
    lv_obj_set_style_radius(s.root, 0, 0);
    lv_obj_set_flex_flow(s.root, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(s.root, LV_OBJ_FLAG_SCROLLABLE);

    s.left = lv_obj_create(s.root);
    lv_obj_set_width(s.left, APP_SCREEN_W);
    lv_obj_set_height(s.left, LV_PCT(100));
    lv_obj_set_style_bg_opa(s.left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.left, 0, 0);
    lv_obj_set_style_pad_all(s.left, 4, 0);
    lv_obj_set_style_pad_row(s.left, 2, 0);
    lv_obj_set_flex_flow(s.left, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.left, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * head = lv_obj_create(s.left);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, 16);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * ht = lv_label_create(head);
    lv_label_set_text(ht, LV_SYMBOL_SETTINGS " SETTINGS");
    lv_obj_set_style_text_color(ht, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(ht, FONT_SM, 0);

    lv_obj_t * back_btn = lv_btn_create(head);
    lv_obj_set_size(back_btn, LV_SIZE_CONTENT, 16);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_add_event_cb(back_btn, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * bl = lv_label_create(back_btn);
    lv_label_set_text(bl, "Back");
    lv_obj_set_style_text_color(bl, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(bl, FONT_SM, 0);
    lv_obj_center(bl);

    s.scroll = lv_obj_create(s.left);
    lv_obj_set_width(s.scroll, LV_PCT(100));
    lv_obj_set_flex_grow(s.scroll, 1);
    lv_obj_set_style_bg_opa(s.scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.scroll, 0, 0);
    lv_obj_set_style_pad_all(s.scroll, 0, 0);
    lv_obj_add_flag(s.scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s.scroll, LV_SCROLLBAR_MODE_AUTO);

    s.scroll_body = lv_obj_create(s.scroll);
    lv_obj_set_width(s.scroll_body, LV_PCT(100));
    lv_obj_set_height(s.scroll_body, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s.scroll_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.scroll_body, 0, 0);
    lv_obj_set_style_pad_all(s.scroll_body, 2, 0);
    lv_obj_set_style_pad_row(s.scroll_body, 4, 0);
    lv_obj_set_flex_flow(s.scroll_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.scroll_body, LV_OBJ_FLAG_SCROLLABLE);

    s.wifi_btn = lv_btn_create(s.scroll_body);
    lv_obj_set_width(s.wifi_btn, LV_PCT(100));
    lv_obj_set_height(s.wifi_btn, 28);
    lv_obj_set_style_bg_color(s.wifi_btn, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_color(s.wifi_btn, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_border_width(s.wifi_btn, 1, 0);
    lv_obj_set_style_radius(s.wifi_btn, 4, 0);
    lv_obj_add_event_cb(s.wifi_btn, wifi_open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * wl = lv_label_create(s.wifi_btn);
    lv_label_set_text(wl, LV_SYMBOL_WIFI " Wi-Fi");
    lv_obj_set_style_text_font(wl, FONT_SM, 0);
    lv_obj_set_style_text_color(wl, lv_color_hex(COL_CYAN), 0);
    lv_obj_center(wl);

    lv_obj_t * inv_lbl = lv_label_create(s.scroll_body);
    lv_label_set_text(inv_lbl, "INVOICE / EMAIL");
    lv_obj_set_style_text_color(inv_lbl, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(inv_lbl, FONT_SM, 0);

    s.promo_ta = make_ta(s.scroll_body, s.draft->promo_head_name, "Promo head name...",
                         lv_color_hex(COL_YELLOW));
    s.email_ta = make_ta(s.scroll_body, s.draft->sender_email, "SMTP sender email...",
                         lv_color_hex(COL_CYAN));
    s.smtp_pass_ta = make_ta(s.scroll_body, s.draft->smtp_app_password, "SMTP app password...",
                             lv_color_hex(COL_ORANGE));
    lv_textarea_set_password_mode(s.smtp_pass_ta, true);
    s.recipient_ta = make_ta(s.scroll_body, s.draft->recipient_email, "Invoice recipient email...",
                             lv_color_hex(COL_GREEN));

    s.dates_wrap = lv_obj_create(s.scroll_body);
    lv_obj_set_width(s.dates_wrap, LV_PCT(100));
    lv_obj_set_height(s.dates_wrap, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s.dates_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.dates_wrap, 0, 0);
    lv_obj_set_style_pad_all(s.dates_wrap, 0, 0);
    lv_obj_set_flex_flow(s.dates_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.dates_wrap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * dlbl = lv_label_create(s.dates_wrap);
    lv_label_set_text(dlbl, "LOCKED DATES (INVOICE SENT)");
    lv_obj_set_style_text_color(dlbl, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(dlbl, FONT_SM, 0);

    s.dates_list = lv_obj_create(s.dates_wrap);
    lv_obj_set_width(s.dates_list, LV_PCT(100));
    lv_obj_set_height(s.dates_list, 52);
    lv_obj_set_style_bg_color(s.dates_list, lv_color_hex(0x052E16), 0);
    lv_obj_set_style_bg_opa(s.dates_list, LV_OPA_20, 0);
    lv_obj_set_style_border_color(s.dates_list, lv_color_hex(0x14532D), 0);
    lv_obj_set_style_border_width(s.dates_list, 1, 0);
    lv_obj_set_style_radius(s.dates_list, 4, 0);
    lv_obj_set_style_pad_all(s.dates_list, 0, 0);
    lv_obj_set_flex_flow(s.dates_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.dates_list, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * emp_lbl = lv_label_create(s.scroll_body);
    lv_label_set_text(emp_lbl, "EMPLOYEES");
    lv_obj_set_style_text_color(emp_lbl, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(emp_lbl, FONT_SM, 0);

    s.emp_list = lv_obj_create(s.scroll_body);
    lv_obj_set_width(s.emp_list, LV_PCT(100));
    lv_obj_set_height(s.emp_list, 56);
    lv_obj_set_style_bg_color(s.emp_list, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(s.emp_list, LV_OPA_50, 0);
    lv_obj_set_style_border_color(s.emp_list, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.emp_list, 1, 0);
    lv_obj_set_style_radius(s.emp_list, 4, 0);
    lv_obj_set_style_pad_all(s.emp_list, 0, 0);
    lv_obj_set_flex_flow(s.emp_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.emp_list, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * add_row = lv_obj_create(s.scroll_body);
    lv_obj_set_width(add_row, LV_PCT(100));
    lv_obj_set_height(add_row, 24);
    lv_obj_set_style_bg_opa(add_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(add_row, 0, 0);
    lv_obj_set_style_pad_all(add_row, 0, 0);
    lv_obj_set_style_pad_column(add_row, 4, 0);
    lv_obj_set_flex_flow(add_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(add_row, LV_OBJ_FLAG_SCROLLABLE);

    s.new_emp_ta = make_ta(add_row, "", "New name...", lv_color_hex(COL_GREEN));
    lv_obj_set_flex_grow(s.new_emp_ta, 1);

    lv_obj_t * add_btn = lv_btn_create(add_row);
    lv_obj_set_size(add_btn, 48, 22);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(0x166534), 0);
    lv_obj_add_event_cb(add_btn, add_emp_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * al = lv_label_create(add_btn);
    lv_label_set_text(al, "+ Add");
    lv_obj_set_style_text_font(al, FONT_SM, 0);
    lv_obj_center(al);

    lv_obj_t * pin_lbl = lv_label_create(s.scroll_body);
    lv_label_set_text(pin_lbl, "PIN CODES (HEAD ONLY)");
    lv_obj_set_style_text_color(pin_lbl, lv_color_hex(COL_ORANGE), 0);
    lv_obj_set_style_text_font(pin_lbl, FONT_SM, 0);

    lv_obj_t * pin_hint = lv_label_create(s.scroll_body);
    lv_label_set_text(pin_hint, "Master PIN required to edit");
    lv_obj_set_style_text_color(pin_hint, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(pin_hint, FONT_SM, 0);
    lv_obj_set_width(pin_hint, LV_PCT(100));

    lv_obj_t * pin_row = lv_obj_create(s.scroll_body);
    lv_obj_set_width(pin_row, LV_PCT(100));
    lv_obj_set_height(pin_row, 22);
    lv_obj_set_style_bg_opa(pin_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pin_row, 0, 0);
    lv_obj_set_style_pad_all(pin_row, 0, 0);
    lv_obj_set_style_pad_column(pin_row, 4, 0);
    lv_obj_set_flex_flow(pin_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(pin_row, LV_OBJ_FLAG_SCROLLABLE);

    s.sys_pin_btn = lv_btn_create(pin_row);
    lv_obj_set_flex_grow(s.sys_pin_btn, 1);
    lv_obj_set_height(s.sys_pin_btn, 22);
    lv_obj_set_style_bg_color(s.sys_pin_btn, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(s.sys_pin_btn, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.sys_pin_btn, 1, 0);
    lv_obj_add_event_cb(s.sys_pin_btn, pin_toggle_cb, LV_EVENT_CLICKED, (void *)(intptr_t)PIN_EDIT_SYSTEM);
    lv_obj_t * sl = lv_label_create(s.sys_pin_btn);
    lv_label_set_text(sl, "System: ----");
    lv_obj_set_style_text_font(sl, FONT_SM, 0);
    lv_obj_center(sl);

    s.master_pin_btn = lv_btn_create(pin_row);
    lv_obj_set_flex_grow(s.master_pin_btn, 1);
    lv_obj_set_height(s.master_pin_btn, 22);
    lv_obj_set_style_bg_color(s.master_pin_btn, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(s.master_pin_btn, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.master_pin_btn, 1, 0);
    lv_obj_add_event_cb(s.master_pin_btn, pin_toggle_cb, LV_EVENT_CLICKED, (void *)(intptr_t)PIN_EDIT_MASTER);
    lv_obj_t * ml = lv_label_create(s.master_pin_btn);
    lv_label_set_text(ml, "Master: ----");
    lv_obj_set_style_text_font(ml, FONT_SM, 0);
    lv_obj_center(ml);

    lv_obj_t * save_btn = lv_btn_create(s.left);
    lv_obj_set_width(save_btn, LV_PCT(100));
    lv_obj_set_height(save_btn, 26);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x0E7490), 0);
    lv_obj_add_event_cb(save_btn, save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * svl = lv_label_create(save_btn);
    lv_label_set_text(svl, "SAVE SETTINGS");
    lv_obj_set_style_text_font(svl, FONT_SM, 0);
    lv_obj_center(svl);

    s.msg_lbl = lv_label_create(s.left);
    lv_label_set_text(s.msg_lbl, "");
    lv_obj_set_style_text_font(s.msg_lbl, FONT_SM, 0);
    lv_obj_set_style_text_align(s.msg_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s.msg_lbl, LV_PCT(100));

#if APP_PORTRAIT
    s.right_panel = lv_obj_create(s.root);
    lv_obj_set_width(s.right_panel, APP_SCREEN_W);
    lv_obj_set_height(s.right_panel, PIN_PANEL_H);
    lv_obj_align(s.right_panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s.right_panel, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(s.right_panel, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(s.right_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s.right_panel, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(s.right_panel, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.right_panel, 1, 0);
    lv_obj_set_style_pad_all(s.right_panel, 0, 0);
    lv_obj_add_flag(s.right_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s.right_panel, LV_OBJ_FLAG_SCROLLABLE);
#else
    s.right_panel = lv_obj_create(s.root);
    lv_obj_set_width(s.right_panel, PIN_W);
    lv_obj_set_height(s.right_panel, LV_PCT(100));
    lv_obj_set_style_bg_color(s.right_panel, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(s.right_panel, LV_OPA_30, 0);
    lv_obj_set_style_border_side(s.right_panel, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(s.right_panel, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.right_panel, 1, 0);
    lv_obj_set_style_pad_all(s.right_panel, 0, 0);
    lv_obj_add_flag(s.right_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s.right_panel, LV_OBJ_FLAG_SCROLLABLE);
#endif

    ensure_text_kb();

    refresh_layout();
    refresh_pin_labels();
    refresh_employees();
    refresh_dates();
    refresh_wifi_btn_label();
}
