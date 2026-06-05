#include "ui_wifi.h"
#include "ui_colors.h"
#include "ui_text_kb.h"
#include "ui_keypad.h"
#include "app_wifi.h"
#include "app_display.h"
#include <stdio.h>
#include <string.h>

#if LV_FONT_MONTSERRAT_10
  #define FONT_SM  (&lv_font_montserrat_10)
#else
  #define FONT_SM  (&lv_font_montserrat_12)
#endif

typedef struct {
    lv_obj_t * root;
    lv_obj_t * head;
    lv_obj_t * body;
    lv_obj_t * list;
    lv_obj_t * status_lbl;
    lv_obj_t * sel_lbl;
    lv_obj_t * pass_ta;
    lv_obj_t * text_kb;
    lv_obj_t * msg_lbl;
    char selected[APP_WIFI_SSID_LEN];
    char pass_buf[APP_WIFI_PASS_LEN];
    ui_wifi_close_cb_t on_close;
    void * user_data;
} wifi_ui_t;

static wifi_ui_t s;

static void pin_head_top(void)
{
    if(s.head && lv_obj_is_valid(s.head))
        lv_obj_move_foreground(s.head);
}

static void back_cb(lv_event_t * e);

static void set_msg(const char * txt, bool ok)
{
    if(!s.msg_lbl) return;
    lv_label_set_text(s.msg_lbl, txt ? txt : "");
    lv_obj_set_style_text_color(s.msg_lbl, lv_color_hex(ok ? COL_GREEN : COL_RED), 0);
}

static void refresh_status(void)
{
    if(!s.status_lbl) return;
    char line[80];
    app_wifi_status_line(line, sizeof(line));
    lv_label_set_text(s.status_lbl, line);
}

static void hide_pass_kb(void)
{
    if(s.text_kb) ui_text_kb_set_visible(s.text_kb, false);
    if(s.body && lv_obj_is_valid(s.body))
        lv_obj_set_style_pad_bottom(s.body, 0, 0);
    else if(s.root && lv_obj_is_valid(s.root))
        lv_obj_set_style_pad_bottom(s.root, 0, 0);
}

static void pass_kb_closed(void * ud)
{
    (void)ud;
    hide_pass_kb();
}

static void style_pass_ta(void)
{
    if(!s.pass_ta) return;
    lv_obj_set_style_bg_color(s.pass_ta, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_bg_opa(s.pass_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s.pass_ta, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_border_color(s.pass_ta, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_border_width(s.pass_ta, 1, 0);
    lv_obj_set_style_pad_hor(s.pass_ta, 4, 0);
}

static void show_pass_kb(void)
{
    if(!s.pass_ta) return;
    if(!s.text_kb && s.root) {
        s.text_kb = ui_text_kb_create(s.root);
        ui_text_kb_set_close_cb(s.text_kb, pass_kb_closed, NULL);
    }
    if(s.text_kb) {
        ui_text_kb_set_textarea(s.text_kb, s.pass_ta);
        ui_text_kb_set_uppercase(s.text_kb, false);
        ui_text_kb_set_visible(s.text_kb, true);
        lv_obj_move_foreground(s.text_kb);
    }
    pin_head_top();
    if(s.body && lv_obj_is_valid(s.body)) {
        lv_obj_set_style_pad_bottom(s.body, UI_TEXT_KB_HEIGHT + 4, 0);
        lv_obj_update_layout(s.body);
    }
}

static void select_network(const char * ssid)
{
    strncpy(s.selected, ssid, sizeof(s.selected) - 1);
    s.selected[sizeof(s.selected) - 1] = '\0';

    char saved_pass[APP_WIFI_PASS_LEN];
    char saved_ssid[APP_WIFI_SSID_LEN];
    app_wifi_get_saved(saved_ssid, sizeof(saved_ssid), saved_pass, sizeof(saved_pass));
    if(strcmp(saved_ssid, ssid) == 0)
        strncpy(s.pass_buf, saved_pass, sizeof(s.pass_buf) - 1);
    else
        s.pass_buf[0] = '\0';
    s.pass_buf[sizeof(s.pass_buf) - 1] = '\0';

    if(s.sel_lbl) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Network: %s", ssid);
        lv_label_set_text(s.sel_lbl, buf);
    }
    if(s.pass_ta) lv_textarea_set_text(s.pass_ta, s.pass_buf);
    show_pass_kb();
    set_msg("Enter password, tap Connect", true);
}

static void net_row_cb(lv_event_t * e)
{
    const char * ssid = lv_event_get_user_data(e);
    if(ssid) select_network(ssid);
}

static int rebuild_list(void)
{
    if(!s.list) return 0;
    lv_obj_clean(s.list);

    app_wifi_network_t nets[APP_WIFI_MAX_SCAN];
    int n = app_wifi_scan(nets, APP_WIFI_MAX_SCAN);

    if(n == 0) {
        lv_obj_t * empty = lv_label_create(s.list);
        lv_label_set_text(empty, "No networks found.\nTry Scan again near hotspot.");
        lv_obj_set_style_text_color(empty, lv_color_hex(COL_GRAY), 0);
        lv_obj_set_style_text_font(empty, FONT_SM, 0);
        lv_obj_center(empty);
        return 0;
    }

    for(int i = 0; i < n; i++) {
        lv_obj_t * row = lv_btn_create(s.list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 28);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x111827), 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x164E63), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(row, lv_color_hex(COL_BORDER), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_hor(row, 6, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        static char ssid_store[APP_WIFI_MAX_SCAN][APP_WIFI_SSID_LEN];
        strncpy(ssid_store[i], nets[i].ssid, APP_WIFI_SSID_LEN - 1);
        ssid_store[i][APP_WIFI_SSID_LEN - 1] = '\0';
        lv_obj_add_event_cb(row, net_row_cb, LV_EVENT_CLICKED, ssid_store[i]);

        lv_obj_t * nm = lv_label_create(row);
        lv_label_set_text(nm, nets[i].ssid);
        lv_obj_set_style_text_color(nm, lv_color_hex(COL_CYAN), 0);
        lv_obj_set_style_text_font(nm, FONT_SM, 0);
        lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);

        char sig[16];
        snprintf(sig, sizeof(sig), "%s %ddBm", nets[i].secure ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_OK,
                 (int)nets[i].rssi);
        lv_obj_t * rs = lv_label_create(row);
        lv_label_set_text(rs, sig);
        lv_obj_set_style_text_color(rs, lv_color_hex(COL_GRAY), 0);
        lv_obj_set_style_text_font(rs, FONT_SM, 0);
    }
    pin_head_top();
    return n;
}

static void scan_cb(lv_event_t * e)
{
    (void)e;
    set_msg("Scanning 2.4 GHz...", true);
    int n = rebuild_list();
    refresh_status();
    if(n <= 0)
        set_msg("No AP found. Check 2.4G hotspot is ON.", false);
    else
        set_msg("", true);
}

static void forget_cb(lv_event_t * e)
{
    (void)e;
    char err[64];
    if(app_wifi_forget(err, sizeof(err))) {
        s.selected[0] = '\0';
        s.pass_buf[0] = '\0';
        if(s.pass_ta) lv_textarea_set_text(s.pass_ta, "");
        if(s.sel_lbl) lv_label_set_text(s.sel_lbl, "Network: -");
        refresh_status();
        set_msg("Saved WiFi cleared", true);
        rebuild_list();
    } else {
        set_msg(err[0] ? err : "Could not clear WiFi", false);
    }
}

static void connect_cb(lv_event_t * e)
{
    (void)e;
    if(!s.selected[0]) {
        set_msg("Select a network first", false);
        return;
    }
    hide_pass_kb();
    pin_head_top();
    const char * pass = s.pass_ta ? lv_textarea_get_text(s.pass_ta) : s.pass_buf;
    if(pass) {
        strncpy(s.pass_buf, pass, sizeof(s.pass_buf) - 1);
        s.pass_buf[sizeof(s.pass_buf) - 1] = '\0';
    }
    set_msg("Connecting...", true);
    char err[64];
    if(app_wifi_connect(s.selected, s.pass_buf, err, sizeof(err))) {
        refresh_status();
        set_msg("Connected! Tap Back when done.", true);
    }
    else {
        set_msg(err[0] ? err : "Connect failed", false);
        show_pass_kb();
    }
}

static void back_cb(lv_event_t * e)
{
    (void)e;
    hide_pass_kb();
    ui_wifi_hide();
}

static void pass_ta_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED)
        show_pass_kb();
}

void ui_wifi_hide(void)
{
    ui_wifi_close_cb_t cb = s.on_close;
    void * ud = s.user_data;
    ui_keypad_release();
    hide_pass_kb();
    if(s.root && lv_obj_is_valid(s.root)) {
        lv_obj_del(s.root);
        s.root = NULL;
    }
    memset(&s, 0, sizeof(s));
    if(cb) cb(ud);
}

bool ui_wifi_is_visible(void)
{
    return s.root && lv_obj_is_valid(s.root);
}

void ui_wifi_show(lv_obj_t * parent, ui_wifi_close_cb_t on_close, void * user_data)
{
    app_wifi_prepare_ui();
    ui_wifi_hide();
    s.on_close = on_close;
    s.user_data = user_data;
    s.selected[0] = '\0';

    char saved_ssid[APP_WIFI_SSID_LEN];
    app_wifi_get_saved(saved_ssid, sizeof(saved_ssid), s.pass_buf, sizeof(s.pass_buf));
    if(saved_ssid[0]) strncpy(s.selected, saved_ssid, sizeof(s.selected) - 1);

    s.root = lv_obj_create(parent);
    lv_obj_set_size(s.root, APP_SCREEN_W, APP_SCREEN_H);
    lv_obj_align(s.root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s.root, lv_color_hex(0x030712), 0);
    lv_obj_set_style_bg_opa(s.root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s.root, 0, 0);
    lv_obj_set_style_pad_all(s.root, 0, 0);
    lv_obj_set_style_radius(s.root, 0, 0);
    lv_obj_clear_flag(s.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s.root, LV_OBJ_FLAG_FLOATING);

    /* Pinned header — never covered by network list */
    s.head = lv_obj_create(s.root);
    lv_obj_set_width(s.head, APP_SCREEN_W);
    lv_obj_set_height(s.head, 32);
    lv_obj_align(s.head, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(s.head, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(s.head, lv_color_hex(0x030712), 0);
    lv_obj_set_style_bg_opa(s.head, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s.head, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(s.head, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.head, 1, 0);
    lv_obj_set_style_pad_hor(s.head, 6, 0);
    lv_obj_set_style_pad_ver(s.head, 4, 0);
    lv_obj_set_flex_flow(s.head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s.head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s.head, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = lv_label_create(s.head);
    lv_label_set_text(title, LV_SYMBOL_WIFI " WI-FI (2.4 GHz)");
    lv_obj_set_style_text_color(title, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_text_font(title, FONT_SM, 0);

    /* Body below pinned header */
    s.body = lv_obj_create(s.root);
    lv_obj_set_width(s.body, APP_SCREEN_W);
    lv_obj_set_height(s.body, APP_SCREEN_H - 32);
    lv_obj_align(s.body, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(s.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.body, 0, 0);
    lv_obj_set_style_pad_all(s.body, 6, 0);
    lv_obj_set_style_pad_row(s.body, 4, 0);
    lv_obj_set_flex_flow(s.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.body, LV_OBJ_FLAG_SCROLLABLE);

    s.status_lbl = lv_label_create(s.body);
    lv_obj_set_width(s.status_lbl, LV_PCT(100));
    lv_obj_set_style_text_font(s.status_lbl, FONT_SM, 0);
    lv_obj_set_style_text_color(s.status_lbl, lv_color_hex(COL_GREEN), 0);
    refresh_status();

    lv_obj_t * action_row = lv_obj_create(s.body);
    lv_obj_set_width(action_row, LV_PCT(100));
    lv_obj_set_height(action_row, 24);
    lv_obj_set_style_bg_opa(action_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(action_row, 0, 0);
    lv_obj_set_style_pad_all(action_row, 0, 0);
    lv_obj_set_style_pad_column(action_row, 4, 0);
    lv_obj_set_flex_flow(action_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(action_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * scan_btn = lv_btn_create(action_row);
    lv_obj_set_size(scan_btn, 56, 22);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x1E3A5F), 0);
    lv_obj_add_event_cb(scan_btn, scan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * sc_l = lv_label_create(scan_btn);
    lv_label_set_text(sc_l, "Scan");
    lv_obj_set_style_text_font(sc_l, FONT_SM, 0);
    lv_obj_center(sc_l);

    lv_obj_t * forget_btn = lv_btn_create(action_row);
    lv_obj_set_size(forget_btn, 72, 22);
    lv_obj_set_style_bg_color(forget_btn, lv_color_hex(0x7F1D1D), 0);
    lv_obj_set_style_border_color(forget_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_border_width(forget_btn, 1, 0);
    lv_obj_add_event_cb(forget_btn, forget_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * fg_l = lv_label_create(forget_btn);
    lv_label_set_text(fg_l, "Reset WiFi");
    lv_obj_set_style_text_font(fg_l, FONT_SM, 0);
    lv_obj_center(fg_l);

    lv_obj_t * back_btn = lv_btn_create(action_row);
    lv_obj_set_size(back_btn, 64, 22);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(back_btn, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_add_event_cb(back_btn, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * bl = lv_label_create(back_btn);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(bl, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_text_font(bl, FONT_SM, 0);
    lv_obj_center(bl);

    lv_obj_t * hint = lv_label_create(s.body);
    lv_label_set_text(hint, "Tap network (phone hotspot 2.4G)");
    lv_obj_set_style_text_color(hint, lv_color_hex(COL_GRAY), 0);
    lv_obj_set_style_text_font(hint, FONT_SM, 0);

    s.list = lv_obj_create(s.body);
    lv_obj_set_width(s.list, LV_PCT(100));
    lv_obj_set_flex_grow(s.list, 1);
    lv_obj_set_style_min_height(s.list, 60, 0);
    lv_obj_set_style_max_height(s.list, APP_SCREEN_H - 32 - 120, 0);
    lv_obj_set_style_bg_color(s.list, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(s.list, LV_OPA_50, 0);
    lv_obj_set_style_border_color(s.list, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.list, 1, 0);
    lv_obj_set_style_radius(s.list, 4, 0);
    lv_obj_set_style_pad_all(s.list, 2, 0);
    lv_obj_set_style_pad_row(s.list, 2, 0);
    lv_obj_set_flex_flow(s.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s.list, LV_SCROLLBAR_MODE_AUTO);

    s.sel_lbl = lv_label_create(s.body);
    lv_label_set_text(s.sel_lbl, s.selected[0] ? "Network: (selected)" : "Network: —");
    lv_obj_set_style_text_color(s.sel_lbl, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(s.sel_lbl, FONT_SM, 0);
    lv_obj_set_width(s.sel_lbl, LV_PCT(100));

    lv_obj_t * pass_row = lv_obj_create(s.body);
    lv_obj_set_width(pass_row, LV_PCT(100));
    lv_obj_set_height(pass_row, 28);
    lv_obj_set_style_bg_opa(pass_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pass_row, 0, 0);
    lv_obj_set_style_pad_all(pass_row, 0, 0);
    lv_obj_set_style_pad_column(pass_row, 4, 0);
    lv_obj_set_flex_flow(pass_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(pass_row, LV_OBJ_FLAG_SCROLLABLE);

    s.pass_ta = lv_textarea_create(pass_row);
    lv_obj_set_flex_grow(s.pass_ta, 1);
    lv_obj_set_height(s.pass_ta, 26);
    lv_textarea_set_one_line(s.pass_ta, true);
    lv_textarea_set_max_length(s.pass_ta, APP_WIFI_PASS_LEN - 1);
    lv_textarea_set_password_mode(s.pass_ta, false);
    lv_textarea_set_placeholder_text(s.pass_ta, "WiFi password...");
    style_pass_ta();
    lv_obj_add_event_cb(s.pass_ta, pass_ta_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s.pass_ta, pass_ta_cb, LV_EVENT_CLICKED, NULL);
    if(s.pass_buf[0]) lv_textarea_set_text(s.pass_ta, s.pass_buf);

    lv_obj_t * conn_btn = lv_btn_create(pass_row);
    lv_obj_set_size(conn_btn, 72, 26);
    lv_obj_set_style_bg_color(conn_btn, lv_color_hex(0x166534), 0);
    lv_obj_add_event_cb(conn_btn, connect_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * cl = lv_label_create(conn_btn);
    lv_label_set_text(cl, "Connect");
    lv_obj_set_style_text_font(cl, FONT_SM, 0);
    lv_obj_center(cl);

    s.msg_lbl = lv_label_create(s.body);
    lv_label_set_text(s.msg_lbl, "");
    lv_obj_set_style_text_font(s.msg_lbl, FONT_SM, 0);
    lv_obj_set_style_text_align(s.msg_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s.msg_lbl, LV_PCT(100));

    s.text_kb = ui_text_kb_create(s.root);
    ui_text_kb_set_close_cb(s.text_kb, pass_kb_closed, NULL);
    ui_text_kb_set_visible(s.text_kb, false);

    int n = rebuild_list();
    if(n <= 0)
        set_msg("No AP found. Tap Scan, keep hotspot near.", false);
    refresh_status();
    if(s.selected[0]) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Network: %s", s.selected);
        lv_label_set_text(s.sel_lbl, buf);
    } else {
        lv_label_set_text(s.sel_lbl, "Network: -");
    }

    lv_obj_move_foreground(s.root);
    pin_head_top();
}
