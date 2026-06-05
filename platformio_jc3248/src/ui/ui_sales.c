#include "ui_sales.h"
#include "ui_colors.h"
#include "ui_keypad.h"
#include "app_data.h"
#include "app_display.h"
#include "debug_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_attr.h>
static EXT_RAM_ATTR app_sales_entry_t s_stash_entry;
#else
static app_sales_entry_t s_stash_entry;
#endif

#if LV_FONT_MONTSERRAT_10
  #define FONT_SM  (&lv_font_montserrat_10)
#else
  #define FONT_SM  (&lv_font_montserrat_12)
#endif

#define CONTENT_H   (APP_SCREEN_H - APP_HDR_H)
#if APP_SCREEN_H > APP_SCREEN_W
  #define APP_PORTRAIT        1
  /*
   * Portrait UX — scrollable items + review footer on top;
   * bottom dock = keypad, then disc + ADD ITEM directly below keys.
   */
  #define PORTR_ACTION_H      38
  #define PORTR_KP_ROW_H      22
  #define PORTR_KP_GRID_H     (PORTR_KP_ROW_H * 4 + 2 * 3)
  #define PORTR_KP_OVERLAY_H  (PORTR_KP_GRID_H + 8)
  #define PORTR_ITEMS_FOOT_H  56
  #define PORTR_DOCK_H        (PORTR_KP_OVERLAY_H + PORTR_ACTION_H)
  #define PORTR_BOTTOM_H      PORTR_ACTION_H
  #define PORTR_PD_ROW_H      PORTR_ACTION_H
  #define PORTR_ADD_BTN_H     PORTR_ACTION_H
  #define PORTR_RIGHT_PAD_T   0
  #define PORTR_RIGHT_PAD_B   0
  #define PORTR_BOTTOM_PAD_T  0
  #define PORTR_BOTTOM_GAP    0
  #define PORTR_LEFT_H        0
  #define KP_PANEL_H          0
#else
  #define APP_PORTRAIT        0
  /* Landscape 480x320 — compact so keypad + form fit in 288px content */
  #define RIGHT_W             156
  #define KP_WRAP_MIN_H       92
  #define LAND_PD_ROW_H       36
  #define LAND_REVIEW_FOOT_H  84
  #define LAND_EMP_ROW_H      20
  #define LAND_BC_BTN_H       22
  #define LAND_REVIEW_BTN_H   24
#endif
#define MAX_BARCODE 14
#define MAX_PRICE   11
#define MAX_MANUAL  11

typedef enum {
    FIELD_BARCODE = 0,
    FIELD_PRICE,
    FIELD_CONFIRM,
} sales_field_t;

typedef struct {
    lv_obj_t * root;
    lv_obj_t * left;
    lv_obj_t * left_body;
    lv_obj_t * right;
    lv_obj_t * right_bottom;
    lv_obj_t * emp_dd;
    lv_obj_t * barcode_btn_lbl;
    lv_obj_t * price_btn;
    lv_obj_t * price_btn_lbl;
    lv_obj_t * disc_box;
    lv_obj_t * items_cont;
    lv_obj_t * grand_lbl;
    lv_obj_t * msg_lbl;
    lv_obj_t * review_btn;
    lv_obj_t * review_btn_lbl;
    lv_obj_t * sent_badge;
    lv_obj_t * review_overlay;
    lv_obj_t * review_title_lbl;
    lv_obj_t * review_list;
    lv_obj_t * review_sys_lbl;
    lv_obj_t * review_qty_lbl;
    lv_obj_t * review_manual_lbl;
    lv_obj_t * review_actions_panel;
    lv_obj_t * review_submit_btn;
    lv_obj_t * review_success_panel;
    lv_obj_t * review_close_btn;
    lv_obj_t * review_done_btn;
    lv_obj_t * confirm_overlay;
    lv_obj_t * confirm_body_lbl;
    lv_obj_t * confirm_amt_lbl;
    lv_obj_t * mismatch_overlay;
    lv_obj_t * mismatch_info;
    lv_obj_t * disc_dd;
    lv_obj_t * add_btn_lbl;
    lv_obj_t * cancel_edit_btn;
    ui_keypad_t keypad;

    app_sales_item_t items[APP_MAX_ITEMS];
    int item_count;
    char entry_id[APP_ID_LEN];
    bool entry_was_closed;

    char barcode[MAX_BARCODE];
    char price[MAX_PRICE];
    char manual_total[MAX_MANUAL];
    int discount_pct; /* 0, 10, 20 */
    char editing_id[APP_ID_LEN];

    sales_field_t active_field;
    bool is_reviewing;
    bool show_mismatch;
    bool show_submit_confirm;
    bool is_success;
    bool is_submitting;
    float confirm_amount;
} sales_ui_t;

static sales_ui_t s;
static lv_timer_t * s_kp_timer;

static void refresh_keypad_target(void);
static bool sales_key_intercept(const char * key);
static void refresh_review_mode_ui(void);

static void touch_expand_btn(lv_obj_t * btn, lv_coord_t pad)
{
    if(btn && lv_obj_is_valid(btn))
        lv_obj_set_ext_click_area(btn, pad);
}
static void refresh_submit_btn_state(void);
static void set_entry_panel_locked(bool locked);

#if APP_PORTRAIT
static void portrait_kp_show(void)
{
    if(!s.right || !lv_obj_is_valid(s.right)) return;
    lv_obj_clear_flag(s.right, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s.right);
}

static void portrait_kp_hide(void)
{
    if(!s.right || !lv_obj_is_valid(s.right)) return;
    lv_obj_add_flag(s.right, LV_OBJ_FLAG_HIDDEN);
}

static void portrait_review_overlay_fit(void)
{
    if(!s.review_overlay || !s.left || !lv_obj_is_valid(s.review_overlay)) return;
    lv_obj_update_layout(s.left);
    lv_coord_t lh = lv_obj_get_height(s.left);
    lv_obj_set_height(s.review_overlay, lh);
    lv_obj_align(s.review_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_move_foreground(s.review_overlay);
}
#else
static void portrait_kp_show(void) {}
static void portrait_kp_hide(void) {}
#endif

static void dbg_log_layout_dims(const char * phase)
{
    if(!s.root || !lv_obj_is_valid(s.root)) return;
    lv_obj_update_layout(s.root);

    lv_obj_t * kp_wrap = lv_obj_get_child(s.right, 0);
    int32_t kw = kp_wrap ? (int32_t)lv_obj_get_height(kp_wrap) : -1;
    int32_t rb = (int32_t)lv_obj_get_height(s.right_bottom);
    int32_t rh = (int32_t)lv_obj_get_height(s.right);
    int32_t rpad = (int32_t)(lv_obj_get_style_pad_top(s.right, 0) +
                             lv_obj_get_style_pad_bottom(s.right, 0));
    int32_t need = kw + rb + rpad;

    // #region agent log
    dbg_log("H1", phase, "right_fit", need, rh, need - rh);
    dbg_log("H2", phase, "columns",
            (int32_t)lv_obj_get_height(s.left), rh, (int32_t)lv_obj_get_height(s.root));
    dbg_log("H4", phase, "root",
            (int32_t)lv_obj_get_width(s.root), (int32_t)lv_obj_get_height(s.root), CONTENT_H);
    dbg_log("H3", phase, "pos",
            (int32_t)lv_obj_get_x(s.root), (int32_t)lv_obj_get_x(s.right),
            (int32_t)lv_obj_get_width(s.right));
    // #endregion
}

static float calc_grand(void)
{
    float t = 0;
    for(int i = 0; i < s.item_count; i++) t += s.items[i].line_total;
    return roundf(t * 100.0f) / 100.0f;
}

static bool is_sent(void)
{
    return app_is_entry_sent(app_sales_employee_id(), app_today_date()) ||
           app_is_date_sent(app_today_date());
}

static bool items_match_saved(const app_sales_entry_t * e)
{
    if(!e) return s.item_count == 0;
    if(e->item_count != s.item_count) return false;
    for(int i = 0; i < s.item_count; i++) {
        if(strcmp(e->items[i].id, s.items[i].id) != 0) return false;
        if(strcmp(e->items[i].barcode, s.items[i].barcode) != 0) return false;
        if(fabsf(e->items[i].price - s.items[i].price) > 0.001f) return false;
        if(fabsf(e->items[i].discount - s.items[i].discount) > 0.001f) return false;
        if(e->items[i].quantity != s.items[i].quantity) return false;
    }
    return true;
}

static bool is_already_closed(void)
{
    app_sales_entry_t * e = app_find_entry(app_sales_employee_id(), app_today_date());
    if(!e || !e->closed) return false;
    if(s.item_count == 0) return true;
    return items_match_saved(e);
}

static void set_msg(const char * txt, bool ok)
{
    if(!txt || !txt[0]) {
        lv_obj_add_flag(s.msg_lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(s.msg_lbl, txt);
    lv_obj_set_style_text_color(s.msg_lbl, lv_color_hex(ok ? COL_GREEN : COL_RED), 0);
    lv_obj_clear_flag(s.msg_lbl, LV_OBJ_FLAG_HIDDEN);
}

static void build_entry(app_sales_entry_t * out, bool closed)
{
    memset(out, 0, sizeof(*out));
    if(s.entry_id[0]) strncpy(out->id, s.entry_id, sizeof(out->id) - 1);
    else app_gen_id(out->id, sizeof(out->id));

    strncpy(out->employee_id, app_sales_employee_id(), sizeof(out->employee_id) - 1);
    strncpy(out->employee_name, app_employee_name(app_sales_employee_id()), sizeof(out->employee_name) - 1);
    out->item_count = s.item_count;
    memcpy(out->items, s.items, sizeof(app_sales_item_t) * (size_t)s.item_count);
    out->grand_total = calc_grand();
    strncpy(out->date, app_today_date(), sizeof(out->date) - 1);
    app_week_key(out->date, out->week_key, sizeof(out->week_key));
    out->closed = closed;
    if(closed) {
        out->manual_total = strtof(s.manual_total, NULL);
        out->has_manual_total = true;
    }
}

static void save_draft(void)
{
    if(is_sent()) return;
    if(s.item_count == 0 && !s.entry_id[0]) return;
    build_entry(&s_stash_entry, false);
    app_sales_entry_t * existing = app_find_entry(app_sales_employee_id(), app_today_date());
    if(existing && existing->closed && !items_match_saved(existing)) {
        s_stash_entry.closed = false;
    }
    else if(existing && existing->closed) {
        s_stash_entry.closed = true;
    }
    strncpy(s.entry_id, s_stash_entry.id, sizeof(s.entry_id) - 1);
    app_upsert_entry(&s_stash_entry);
}

static void style_field_box(lv_obj_t * box, bool active)
{
    lv_obj_set_style_bg_color(box, lv_color_hex(active ? 0x164E63 : 0x1F2937), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(active ? COL_CYAN : 0x374151), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_shadow_width(box, 0, 0);
}

static void style_disc_dropdown(lv_obj_t * dd)
{
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x111827), LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_hex(COL_GREEN), LV_PART_MAIN);
    lv_obj_set_style_border_width(dd, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(dd, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_left(dd, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_right(dd, 22, LV_PART_MAIN);
    lv_obj_set_style_pad_top(dd, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(dd, 3, LV_PART_MAIN);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_hex(COL_GRAY_TEXT), LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(dd, lv_color_hex(0x030712), LV_PART_ITEMS);
    lv_obj_set_style_text_color(dd, lv_color_hex(COL_GREEN), LV_PART_ITEMS);
    lv_obj_set_style_border_color(dd, lv_color_hex(0x374151), LV_PART_ITEMS);
    lv_obj_set_style_border_width(dd, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(dd, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_row(dd, 2, LV_PART_ITEMS);
    lv_obj_set_style_max_height(dd, 72, LV_PART_ITEMS);

    lv_obj_set_style_bg_color(dd, lv_color_hex(0x164E63), LV_PART_SELECTED);
    lv_obj_set_style_text_color(dd, lv_color_hex(COL_GREEN), LV_PART_SELECTED);
}

static void sync_disc_dropdown(void)
{
    if(!s.disc_dd) return;
    uint32_t sel = s.discount_pct == 10 ? 1 : s.discount_pct == 20 ? 2 : 0;
    lv_dropdown_set_selected(s.disc_dd, sel);
}

static void disc_dd_event(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * dd = lv_event_get_target(e);

    if(code == LV_EVENT_VALUE_CHANGED) {
        uint32_t sel = lv_dropdown_get_selected(dd);
        s.discount_pct = sel == 1 ? 10 : sel == 2 ? 20 : 0;
    }
    if(code == LV_EVENT_READY) {
        lv_obj_t * list = lv_dropdown_get_list(dd);
        if(list) {
            lv_obj_set_style_bg_color(list, lv_color_hex(0x030712), 0);
            lv_obj_set_style_border_color(list, lv_color_hex(0x374151), 0);
            lv_obj_set_style_border_width(list, 1, 0);
            lv_obj_set_style_radius(list, 4, 0);
            lv_obj_set_style_text_color(list, lv_color_hex(COL_GREEN), 0);
            lv_obj_set_style_text_font(list, &lv_font_montserrat_12, 0);
            lv_obj_set_style_pad_all(list, 2, 0);
        }
    }
}

static void refresh_field_styles(void)
{
    lv_obj_t * bb = lv_obj_get_parent(s.barcode_btn_lbl);
    style_field_box(bb, s.active_field == FIELD_BARCODE);
    if(s.price_btn) style_field_box(s.price_btn, s.active_field == FIELD_PRICE);

    char padded[16];
    if(s.barcode[0]) {
        app_pad_barcode(s.barcode, padded, sizeof(padded));
        lv_label_set_text(s.barcode_btn_lbl, padded);
    }
    else lv_label_set_text(s.barcode_btn_lbl, "-");

    if(s.price[0]) {
        char buf[24];
        snprintf(buf, sizeof(buf), "P%s", s.price);
        lv_label_set_text(s.price_btn_lbl, buf);
    }
    else lv_label_set_text(s.price_btn_lbl, "-");
}

static void refresh_grand(void)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "P%.2f", calc_grand());
    lv_label_set_text(s.grand_lbl, buf);
}

static void clear_form(void)
{
    s.barcode[0] = '\0';
    s.price[0] = '\0';
    s.editing_id[0] = '\0';
    s.active_field = FIELD_BARCODE;
    s.discount_pct = 0;
    sync_disc_dropdown();
    if(s.cancel_edit_btn) lv_obj_add_flag(s.cancel_edit_btn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s.add_btn_lbl, "ADD ITEM");
    refresh_field_styles();
    refresh_keypad_target();
}

static void rebuild_items_list(void)
{
    lv_obj_clean(s.items_cont);
    if(s.item_count == 0) {
        lv_obj_t * empty = lv_label_create(s.items_cont);
        lv_label_set_text(empty, is_sent() ? "Invoice sent" : "No items added");
        lv_obj_set_style_text_color(empty, lv_color_hex(COL_GRAY), 0);
        lv_obj_set_style_text_font(empty, FONT_SM, 0);
        lv_obj_center(empty);
        return;
    }
    for(int i = 0; i < s.item_count; i++) {
        app_sales_item_t * it = &s.items[i];
        lv_obj_t * row = lv_obj_create(s.items_cont);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 4, 0);
        if(s.editing_id[0] && strcmp(s.editing_id, it->id) == 0) {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x164E63), 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_40, 0);
        }
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * top = lv_obj_create(row);
        lv_obj_set_width(top, LV_PCT(100));
        lv_obj_set_height(top, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(top, 0, 0);
        lv_obj_set_style_pad_all(top, 0, 0);
        lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * bc = lv_label_create(top);
        lv_label_set_text(bc, it->barcode);
        lv_obj_set_style_text_color(bc, lv_color_hex(COL_CYAN), 0);
        lv_obj_set_style_text_font(bc, FONT_SM, 0);

        lv_obj_t * acts = lv_obj_create(top);
        lv_obj_set_size(acts, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(acts, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(acts, 0, 0);
        lv_obj_set_style_pad_all(acts, 0, 0);
        lv_obj_set_flex_flow(acts, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(acts, 8, 0);
        lv_obj_clear_flag(acts, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * edit_btn = lv_btn_create(acts);
        lv_obj_set_size(edit_btn, 36, 28);
        lv_obj_set_style_pad_all(edit_btn, 2, 0);
        lv_obj_set_style_radius(edit_btn, 4, 0);
        lv_obj_t * el = lv_label_create(edit_btn);
        lv_label_set_text(el, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_font(el, &lv_font_montserrat_14, 0);
        lv_obj_center(el);
        touch_expand_btn(edit_btn, 10);

        lv_obj_t * del_btn = lv_btn_create(acts);
        lv_obj_set_size(del_btn, 36, 28);
        lv_obj_set_style_pad_all(del_btn, 2, 0);
        lv_obj_set_style_radius(del_btn, 4, 0);
        lv_obj_t * dl = lv_label_create(del_btn);
        lv_label_set_text(dl, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(dl, lv_color_hex(COL_RED), 0);
        lv_obj_set_style_text_font(dl, &lv_font_montserrat_14, 0);
        lv_obj_center(dl);
        touch_expand_btn(del_btn, 10);
    }
}

/* Forward declarations for item row events */
static void begin_edit_item(int idx);
static void edit_item_cb(lv_event_t * e);
static void del_item_cb(lv_event_t * e);

static void wire_item_buttons(void)
{
    uint32_t n = lv_obj_get_child_cnt(s.items_cont);
    int idx = 0;
    for(uint32_t c = 0; c < n && idx < s.item_count; c++) {
        lv_obj_t * row = lv_obj_get_child(s.items_cont, c);
        if(!lv_obj_get_child_cnt(row)) continue;
        lv_obj_t * top = lv_obj_get_child(row, 0);
        lv_obj_t * acts = lv_obj_get_child(top, 1);
        if(!acts) continue;
        lv_obj_t * edit_btn = lv_obj_get_child(acts, 0);
        lv_obj_t * del_btn = lv_obj_get_child(acts, 1);
        lv_obj_remove_event_cb(edit_btn, edit_item_cb);
        lv_obj_remove_event_cb(del_btn, del_item_cb);
        lv_obj_add_event_cb(edit_btn, edit_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
        lv_obj_add_event_cb(del_btn, del_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
        idx++;
    }
}

static void refresh_review_btn(void)
{
    bool closed = is_already_closed();
    if(closed) {
        lv_label_set_text(s.review_btn_lbl, "VIEW SUMMARY");
        lv_obj_clear_state(s.review_btn, LV_STATE_DISABLED);
    }
    else {
        lv_label_set_text(s.review_btn_lbl, "REVIEW");
        if(s.item_count == 0 || is_sent())
            lv_obj_add_state(s.review_btn, LV_STATE_DISABLED);
        else
            lv_obj_clear_state(s.review_btn, LV_STATE_DISABLED);
    }
}

static void refresh_sent_ui(void)
{
    bool sent = is_sent();
    if(sent) lv_obj_clear_flag(s.sent_badge, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s.sent_badge, LV_OBJ_FLAG_HIDDEN);
    if(sent) {
        lv_obj_add_state(s.emp_dd, LV_STATE_DISABLED);
        lv_obj_add_state(s.add_btn_lbl, LV_STATE_DISABLED);
    }
    else {
        lv_obj_clear_state(s.emp_dd, LV_STATE_DISABLED);
    }
    refresh_review_btn();
}

static void refresh_keypad_target(void)
{
    char * buf = s.barcode;
    int max_len = 13;
    const char * ph = "BARCODE";
    if(s.active_field == FIELD_PRICE) { buf = s.price; max_len = 10; ph = "PRICE"; }
    if(s.active_field == FIELD_CONFIRM) { buf = s.manual_total; max_len = 10; ph = "TOTAL"; }
#if APP_PORTRAIT
    (void)ph;
    ph = "";
#endif
    s.keypad.value_buf = buf;
    s.keypad.max_len = max_len;
    s.keypad.placeholder = ph;
    ui_keypad_bind(buf, max_len, ph);
}

/** Keep price digits out of barcode: '.' or digit after full barcode → price field. */
static bool sales_key_intercept(const char * key)
{
    if(s.active_field != FIELD_BARCODE || strcmp(key, "BK") == 0) return false;

    if(strcmp(key, ".") == 0) {
        s.active_field = FIELD_PRICE;
        refresh_keypad_target();
        refresh_field_styles();
        return true;
    }

    if(strlen(s.barcode) >= 13) {
        size_t plen = strlen(s.price);
        if(plen < 10) {
            s.price[plen] = key[0];
            s.price[plen + 1] = '\0';
        }
        s.active_field = FIELD_PRICE;
        refresh_keypad_target();
        refresh_field_styles();
        return true;
    }
    return false;
}

static void refresh_all(void)
{
    refresh_field_styles();
    refresh_grand();
    rebuild_items_list();
    wire_item_buttons();
    refresh_sent_ui();
    refresh_keypad_target();
}

static void sync_employee_dropdown(void)
{
    app_settings_t * st = app_settings();
    for(int i = 0; i < st->employee_count; i++) {
        if(strcmp(st->employees[i].id, app_sales_employee_id()) == 0) {
            lv_dropdown_set_selected(s.emp_dd, (uint32_t)i);
            return;
        }
    }
}

static void load_from_storage(void)
{
    s.item_count = 0;
    s.entry_id[0] = '\0';
    s.entry_was_closed = false;
    clear_form();
    sync_employee_dropdown();

    app_sales_entry_t * e = app_find_entry(app_sales_employee_id(), app_today_date());
    if(e) {
        strncpy(s.entry_id, e->id, sizeof(s.entry_id) - 1);
        s.entry_was_closed = e->closed;
        s.item_count = e->item_count;
        if(s.item_count > APP_MAX_ITEMS)
            s.item_count = APP_MAX_ITEMS;
        if(s.item_count > 0)
            memcpy(s.items, e->items, sizeof(app_sales_item_t) * (size_t)s.item_count);
    }
    refresh_all();
    if(app_take_sales_open_for_edit() && s.item_count > 0 && !is_sent())
        begin_edit_item(0);
}

static void add_item_cb(lv_event_t * e)
{
    (void)e;
    if(is_sent()) { set_msg("Date locked (Invoice Sent)", false); return; }
    float p = strtof(s.price, NULL);
    if(!s.barcode[0] || p <= 0.0f) { set_msg("Invalid entry", false); return; }

    app_sales_item_t it;
    if(s.editing_id[0]) strncpy(it.id, s.editing_id, sizeof(it.id) - 1);
    else app_gen_id(it.id, sizeof(it.id));
    app_pad_barcode(s.barcode, it.barcode, sizeof(it.barcode));
    it.price = p;
    it.discount = (float)s.discount_pct;
    it.quantity = 1;
    it.line_total = roundf((p * it.quantity) * (1.0f - it.discount / 100.0f) * 100.0f) / 100.0f;

    bool editing = s.editing_id[0] != '\0';
    if(editing) {
        for(int i = 0; i < s.item_count; i++) {
            if(strcmp(s.items[i].id, s.editing_id) == 0) s.items[i] = it;
        }
        set_msg("Item updated", true);
    }
    else {
        if(s.item_count < APP_MAX_ITEMS) s.items[s.item_count++] = it;
        set_msg("", true);
    }
    save_draft();
    clear_form();
    refresh_all();
}

static void cancel_edit_cb(lv_event_t * e)
{
    (void)e;
    clear_form();
    set_msg("", true);
    refresh_all();
}

static void begin_edit_item(int idx)
{
    if(idx < 0 || idx >= s.item_count) return;
    if(is_sent() || s.is_reviewing) return;
    app_sales_item_t * it = &s.items[idx];
    strncpy(s.editing_id, it->id, sizeof(s.editing_id) - 1);
    const char * b = it->barcode;
    while(*b == '0' && b[1]) b++;
    strncpy(s.barcode, b, sizeof(s.barcode) - 1);
    snprintf(s.price, sizeof(s.price), "%.2f", it->price);
    s.discount_pct = (int)it->discount;
    sync_disc_dropdown();
    s.active_field = FIELD_BARCODE;
    if(s.add_btn_lbl) lv_label_set_text(s.add_btn_lbl, "UPDATE ITEM");
    if(s.cancel_edit_btn) lv_obj_clear_flag(s.cancel_edit_btn, LV_OBJ_FLAG_HIDDEN);
    set_msg("Editing item", true);
    refresh_all();
}

static void edit_item_cb(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    begin_edit_item(idx);
}

static void del_item_cb(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if(is_sent()) { set_msg("Date locked", false); return; }
    char rm[APP_ID_LEN];
    strncpy(rm, s.items[idx].id, sizeof(rm) - 1);
    for(int i = idx; i < s.item_count - 1; i++) s.items[i] = s.items[i + 1];
    s.item_count--;
    if(strcmp(s.editing_id, rm) == 0) clear_form();
    save_draft();
    refresh_all();
}

static void field_barcode_cb(lv_event_t * e)
{
    (void)e;
    if(is_sent()) return;
    s.active_field = FIELD_BARCODE;
    refresh_keypad_target();
    refresh_field_styles();
    portrait_kp_show();
}

static void field_price_cb(lv_event_t * e)
{
    (void)e;
    if(is_sent()) return;
    s.active_field = FIELD_PRICE;
    refresh_keypad_target();
    refresh_field_styles();
    portrait_kp_show();
}

static void emp_changed(lv_event_t * e)
{
    (void)e;
    if(s.is_reviewing) return;
    uint16_t sel = lv_dropdown_get_selected(s.emp_dd);
    app_settings_t * st = app_settings();
    if(sel >= (uint16_t)st->employee_count) return;
    const char * new_id = st->employees[sel].id;
    if(strcmp(new_id, app_sales_employee_id()) == 0) return;

    if(!is_sent() && s.item_count > 0) save_draft();
    app_set_sales_employee_id(new_id);
    load_from_storage();
}

static void show_review_overlay(bool show)
{
    if(show) {
        lv_obj_clear_flag(s.review_overlay, LV_OBJ_FLAG_HIDDEN);
#if APP_PORTRAIT
        portrait_review_overlay_fit();
        portrait_kp_show();
#else
        lv_obj_move_foreground(s.review_overlay);
#endif
    }
    else {
        lv_obj_add_flag(s.review_overlay, LV_OBJ_FLAG_HIDDEN);
        if(!is_sent()) portrait_kp_show();
    }
}

static void set_entry_panel_locked(bool locked)
{
    /* Prototype: only price/disc/add dimmed during review; keypad stays active for confirm total */
    if(!s.right_bottom) return;
    if(locked) {
        lv_obj_add_state(s.right_bottom, LV_STATE_DISABLED);
        lv_obj_set_style_opa(s.right_bottom, LV_OPA_40, 0);
    }
    else {
        lv_obj_clear_state(s.right_bottom, LV_STATE_DISABLED);
        lv_obj_set_style_opa(s.right_bottom, LV_OPA_COVER, 0);
    }
}

static void overlay_prepare(lv_obj_t * overlay)
{
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_move_foreground(overlay);
}

static void refresh_submit_btn_state(void)
{
    if(!s.review_submit_btn) return;
    if(s.manual_total[0] != '\0' && !s.is_submitting && !s.is_success)
        lv_obj_clear_state(s.review_submit_btn, LV_STATE_DISABLED);
    else
        lv_obj_add_state(s.review_submit_btn, LV_STATE_DISABLED);
}

static void refresh_review_mode_ui(void)
{
    bool summary = s.is_success || (is_already_closed() && s.is_reviewing);

    if(s.review_title_lbl) {
        lv_label_set_text(s.review_title_lbl,
                          summary ? "SUBMISSION SUMMARY" : "REVIEW SALES");
    }
    if(s.review_actions_panel && s.review_success_panel) {
        if(summary) {
            lv_obj_add_flag(s.review_actions_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s.review_success_panel, LV_OBJ_FLAG_HIDDEN);
#if APP_PORTRAIT
            portrait_kp_hide();
#endif
        }
        else {
            lv_obj_clear_flag(s.review_actions_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s.review_success_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if(s.review_close_btn && lv_obj_is_valid(s.review_close_btn)) {
        lv_obj_move_foreground(s.review_close_btn);
        touch_expand_btn(s.review_close_btn, summary ? 16 : 10);
    }
    if(s.review_done_btn && lv_obj_is_valid(s.review_done_btn)) {
        lv_obj_t * dl = lv_obj_get_child(s.review_done_btn, 0);
        if(dl)
            lv_label_set_text(dl, summary ? "CLOSE" : "DONE / CLOSE REVIEW");
        touch_expand_btn(s.review_done_btn, summary ? 14 : 8);
    }
    refresh_submit_btn_state();
}

static void rebuild_review_list(const app_sales_item_t * items, int count)
{
    lv_obj_clean(s.review_list);

    lv_obj_t * hdr = lv_obj_create(s.review_list);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_obj_set_height(hdr, 14);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_pad_all(hdr, 2, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * h1 = lv_label_create(hdr);
    lv_label_set_text(h1, "ITEM");
    lv_obj_set_style_text_color(h1, lv_color_hex(COL_GRAY), 0);
    lv_obj_set_style_text_font(h1, FONT_SM, 0);
    lv_obj_t * h2 = lv_label_create(hdr);
    lv_label_set_text(h2, "TOTAL");
    lv_obj_set_style_text_color(h2, lv_color_hex(COL_GRAY), 0);
    lv_obj_set_style_text_font(h2, FONT_SM, 0);

    int qty = 0;
    for(int i = 0; i < count; i++) {
        qty += items[i].quantity;
        lv_obj_t * row = lv_obj_create(s.review_list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 18);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t * a = lv_label_create(row);
        lv_label_set_text(a, items[i].barcode);
        lv_obj_set_style_text_color(a, lv_color_hex(COL_CYAN), 0);
        lv_obj_set_style_text_font(a, FONT_SM, 0);
        char buf[16];
        snprintf(buf, sizeof(buf), "P%.2f", items[i].line_total);
        lv_obj_t * b = lv_label_create(row);
        lv_label_set_text(b, buf);
        lv_obj_set_style_text_color(b, lv_color_hex(COL_GREEN), 0);
        lv_obj_set_style_text_font(b, FONT_SM, 0);
    }
    char q[16];
    snprintf(q, sizeof(q), "%d", qty);
    lv_label_set_text(s.review_qty_lbl, q);
}

static void open_submission_summary(void)
{
    app_sales_entry_t * saved = app_find_entry(app_sales_employee_id(), app_today_date());
    s.is_reviewing = true;
    s.is_success = true;
    s.show_mismatch = false;
    s.show_submit_confirm = false;
    show_review_overlay(true);
    lv_obj_add_flag(s.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s.mismatch_overlay, LV_OBJ_FLAG_HIDDEN);
    set_entry_panel_locked(true);
    if(saved) {
        char buf[24];
        snprintf(buf, sizeof(buf), "P%.2f", saved->grand_total);
        lv_label_set_text(s.review_sys_lbl, buf);
        rebuild_review_list(saved->items, saved->item_count);
    }
    refresh_review_mode_ui();
    if(s.review_close_btn)
        lv_obj_move_foreground(s.review_close_btn);
}

static void start_review_cb(lv_event_t * e)
{
    (void)e;
    if(is_sent()) { set_msg("Date locked (Invoice Sent)", false); return; }
    if(is_already_closed()) {
        open_submission_summary();
        return;
    }
    if(s.item_count == 0) {
        set_msg("Add items first", false);
        return;
    }

    s.is_reviewing = true;
    s.is_success = false;
    s.show_mismatch = false;
    s.show_submit_confirm = false;
    s.manual_total[0] = '\0';
    s.active_field = FIELD_CONFIRM;
    show_review_overlay(true);
    lv_obj_add_flag(s.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s.mismatch_overlay, LV_OBJ_FLAG_HIDDEN);
    set_entry_panel_locked(true);
    lv_obj_add_flag(s.msg_lbl, LV_OBJ_FLAG_HIDDEN);

    rebuild_review_list(s.items, s.item_count);
    char buf[24];
    snprintf(buf, sizeof(buf), "P%.2f", calc_grand());
    lv_label_set_text(s.review_sys_lbl, buf);
    lv_label_set_text(s.review_manual_lbl, "P0.00");
    refresh_keypad_target();
    refresh_review_mode_ui();
    set_msg("Review your items and confirm total", true);
}

static void cancel_review_cb(lv_event_t * e)
{
    (void)e;
    bool submitted = s.is_success;
    s.is_reviewing = false;
    s.is_success = false;
    s.show_mismatch = false;
    s.show_submit_confirm = false;
    s.active_field = FIELD_BARCODE;
    show_review_overlay(false);
    lv_obj_add_flag(s.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s.mismatch_overlay, LV_OBJ_FLAG_HIDDEN);
    set_entry_panel_locked(false);
    set_msg("", true);
    refresh_keypad_target();
    refresh_review_btn();
    if(submitted) {
        rebuild_items_list();
        refresh_grand();
    }
}

static void final_submit_cb(lv_event_t * e)
{
    (void)e;
    if(is_sent()) { set_msg("Date locked (Invoice Sent)", false); return; }
    if(s.is_submitting || s.is_success) return;
    if(!s.manual_total[0]) return;

    float input = strtof(s.manual_total, NULL);
    float sys = roundf(calc_grand() * 100.0f) / 100.0f;
    if(fabsf(input - sys) > 0.01f) {
        s.show_mismatch = true;
        char info[80];
        snprintf(info, sizeof(info), "System: P%.2f\nInput: P%.2f", sys, input);
        lv_label_set_text(s.mismatch_info, info);
        lv_obj_move_foreground(s.mismatch_overlay);
        lv_obj_clear_flag(s.mismatch_overlay, LV_OBJ_FLAG_HIDDEN);
        set_msg("Mismatch!", false);
#if APP_PORTRAIT
        portrait_kp_hide();
#endif
        return;
    }
    s.confirm_amount = sys;
    char body[96];
    snprintf(body, sizeof(body),
             "Are you sure you want to submit\nP%.2f total sales?", sys);
    lv_label_set_text(s.confirm_body_lbl, body);
    lv_obj_move_foreground(s.confirm_overlay);
    lv_obj_clear_flag(s.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
#if APP_PORTRAIT
    portrait_kp_hide();
#endif
}

static void confirm_yes_cb(lv_event_t * e)
{
    (void)e;
    lv_obj_add_flag(s.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    if(is_sent()) { set_msg("Date locked (Invoice Sent)", false); return; }
    if(s.is_submitting) return;

    s.is_submitting = true;
    refresh_submit_btn_state();

    build_entry(&s_stash_entry, true);
    s_stash_entry.manual_total = strtof(s.manual_total, NULL);
    s_stash_entry.has_manual_total = true;
    app_upsert_entry(&s_stash_entry);
    strncpy(s.entry_id, s_stash_entry.id, sizeof(s.entry_id) - 1);

    s.item_count = 0;
    s.is_success = true;
    s.is_reviewing = true;
    clear_form();
    rebuild_items_list();
    set_msg("Submitted!", true);
    s.is_submitting = false;

    open_submission_summary();
    refresh_grand();
    refresh_review_btn();
}

static void confirm_no_cb(lv_event_t * e)
{
    (void)e;
    lv_obj_add_flag(s.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
#if APP_PORTRAIT
    if(s.is_reviewing && !s.is_success) portrait_kp_show();
#endif
}

static void mismatch_ok_cb(lv_event_t * e)
{
    (void)e;
    lv_obj_add_flag(s.mismatch_overlay, LV_OBJ_FLAG_HIDDEN);
#if APP_PORTRAIT
    if(s.is_reviewing && !s.is_success) portrait_kp_show();
#endif
}

static void review_manual_refresh(void)
{
    if(!s.is_reviewing || s.is_success) return;
    if(s.manual_total[0]) {
        char buf[24];
        snprintf(buf, sizeof(buf), "P%s", s.manual_total);
        lv_label_set_text(s.review_manual_lbl, buf);
    }
    else {
        lv_label_set_text(s.review_manual_lbl, "P0.00");
    }
    refresh_submit_btn_state();
}

/* Hook keypad refresh to also update review manual label */
static void keypad_value_changed(void);

static void sales_kp_timer(lv_timer_t * t)
{
    (void)t;
    if(!s.root || !lv_obj_is_valid(s.root)) return;
    keypad_value_changed();
}

void ui_sales_destroy(void)
{
    if(s_kp_timer) {
        lv_timer_del(s_kp_timer);
        s_kp_timer = NULL;
    }
    ui_keypad_release();
    if(s.root && lv_obj_is_valid(s.root)) {
        lv_obj_del(s.root);
        s.root = NULL;
    }
    memset(&s, 0, sizeof(s));
}

static void keypad_value_changed(void)
{
    if(!s.root || !lv_obj_is_valid(s.root)) return;
    if(s.active_field == FIELD_BARCODE) {
        char * dot = strchr(s.barcode, '.');
        if(dot) {
            *dot = '\0';
            const char * rest = dot + 1;
            if(rest[0]) {
                strncpy(s.price, rest, sizeof(s.price) - 1);
                s.price[sizeof(s.price) - 1] = '\0';
            }
            s.active_field = FIELD_PRICE;
            refresh_keypad_target();
        }
    }
    review_manual_refresh();
    refresh_field_styles();
}

void ui_sales_reload(void)
{
    if(!s.root || !lv_obj_is_valid(s.root)) return;
    load_from_storage();
}

void ui_sales_pause_refresh(bool pause)
{
    if(!s_kp_timer) return;
    if(pause) lv_timer_pause(s_kp_timer);
    else lv_timer_resume(s_kp_timer);
}

void ui_sales_create(lv_obj_t * parent)
{
    // #region agent log
    dbg_log("H1", "ui_sales.c:create", "enter",
            (int32_t)lv_obj_get_width(parent), (int32_t)lv_obj_get_height(parent), APP_PORTRAIT);
    dbg_log_heap("H1", "ui_sales.c:create", "enter_heap");
    // #endregion
    ui_sales_destroy();
    s.discount_pct = 0;

    s.root = lv_obj_create(parent);
#if APP_PORTRAIT
    lv_obj_set_size(s.root, LV_PCT(100), LV_PCT(100));
#else
    lv_obj_set_size(s.root, LV_PCT(100), LV_PCT(100));
#endif
    lv_obj_set_pos(s.root, 0, 0);
    lv_obj_set_style_bg_color(s.root, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(s.root, 0, 0);
    lv_obj_set_style_pad_all(s.root, 0, 0);
    lv_obj_set_style_radius(s.root, 0, 0);
#if APP_PORTRAIT
    lv_obj_set_flex_flow(s.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
#else
    lv_obj_set_flex_flow(s.root, LV_FLEX_FLOW_ROW);
#endif
    lv_obj_clear_flag(s.root, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Left column (form + items) ---- */
    s.left = lv_obj_create(s.root);
#if APP_PORTRAIT
    lv_obj_set_width(s.left, LV_PCT(100));
    lv_obj_set_flex_grow(s.left, 1);
    lv_obj_set_style_min_height(s.left, 0, 0);
#else
    lv_obj_set_height(s.left, LV_PCT(100));
    lv_obj_set_flex_grow(s.left, 1);
    lv_obj_set_style_min_width(s.left, (APP_SCREEN_W - RIGHT_W - 4), 0);
#endif
    lv_obj_set_style_bg_opa(s.left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.left, 0, 0);
    lv_obj_set_style_pad_all(s.left, 4, 0);
    lv_obj_set_flex_flow(s.left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s.left, 3, 0);
    lv_obj_clear_flag(s.left, LV_OBJ_FLAG_SCROLLABLE);

    s.left_body = lv_obj_create(s.left);
    lv_obj_set_width(s.left_body, LV_PCT(100));
    lv_obj_set_flex_grow(s.left_body, 1);
#if APP_PORTRAIT
    lv_obj_set_style_min_height(s.left_body, 0, 0);
#endif
    lv_obj_set_style_bg_opa(s.left_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.left_body, 0, 0);
    lv_obj_set_style_pad_all(s.left_body, 0, 0);
    lv_obj_set_flex_flow(s.left_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s.left_body, 4, 0);
    lv_obj_clear_flag(s.left_body, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * emp_row = lv_obj_create(s.left_body);
    lv_obj_set_width(emp_row, LV_PCT(100));
#if APP_PORTRAIT
    lv_obj_set_height(emp_row, 22);
#else
    lv_obj_set_height(emp_row, LAND_EMP_ROW_H);
#endif
    lv_obj_set_style_bg_opa(emp_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(emp_row, 0, 0);
    lv_obj_set_style_pad_all(emp_row, 0, 0);
    lv_obj_set_flex_flow(emp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(emp_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(emp_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * emp_lbl = lv_label_create(emp_row);
    lv_label_set_text(emp_lbl, "Salesman");
    lv_obj_set_style_text_color(emp_lbl, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(emp_lbl, FONT_SM, 0);
    s.emp_dd = lv_dropdown_create(emp_row);
    lv_obj_set_flex_grow(s.emp_dd, 1);
    {
        app_settings_t * st = app_settings();
        char opts[128] = "";
        for(int i = 0; i < st->employee_count; i++) {
            if(i) strcat(opts, "\n");
            strcat(opts, st->employees[i].name);
        }
        lv_dropdown_set_options(s.emp_dd, opts);
    }
    lv_obj_add_event_cb(s.emp_dd, emp_changed, LV_EVENT_VALUE_CHANGED, NULL);

    s.sent_badge = lv_label_create(emp_row);
    lv_label_set_text(s.sent_badge, "SENT");
    lv_obj_set_style_text_color(s.sent_badge, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(s.sent_badge, FONT_SM, 0);
    lv_obj_add_flag(s.sent_badge, LV_OBJ_FLAG_HIDDEN);

#if APP_PORTRAIT
    /* Barcode + Price side-by-side — tap to select field for keypad */
    lv_obj_t * fld_row = lv_obj_create(s.left_body);
    lv_obj_set_width(fld_row, LV_PCT(100));
    lv_obj_set_height(fld_row, 28);
    lv_obj_set_style_bg_opa(fld_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fld_row, 0, 0);
    lv_obj_set_style_pad_all(fld_row, 0, 0);
    lv_obj_set_flex_flow(fld_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(fld_row, 4, 0);
    lv_obj_clear_flag(fld_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * bc_btn = lv_btn_create(fld_row);
    lv_obj_set_flex_grow(bc_btn, 1);
    lv_obj_set_height(bc_btn, 28);
    lv_obj_set_style_radius(bc_btn, 4, 0);
    lv_obj_add_event_cb(bc_btn, field_barcode_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * bc_l = lv_label_create(bc_btn);
    lv_label_set_text(bc_l, "BC");
    lv_obj_set_style_text_color(bc_l, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(bc_l, FONT_SM, 0);
    lv_obj_align(bc_l, LV_ALIGN_LEFT_MID, 4, 0);
    s.barcode_btn_lbl = lv_label_create(bc_btn);
    lv_obj_align(s.barcode_btn_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_text_color(s.barcode_btn_lbl, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(s.barcode_btn_lbl, FONT_SM, 0);

    s.price_btn = lv_btn_create(fld_row);
    lv_obj_set_flex_grow(s.price_btn, 1);
    lv_obj_set_height(s.price_btn, 28);
    lv_obj_set_style_radius(s.price_btn, 4, 0);
    lv_obj_set_style_pad_all(s.price_btn, 4, 0);
    lv_obj_add_event_cb(s.price_btn, field_price_cb, LV_EVENT_CLICKED, NULL);
    style_field_box(s.price_btn, false);
    lv_obj_t * pr_t = lv_label_create(s.price_btn);
    lv_label_set_text(pr_t, "PRICE");
    lv_obj_set_style_text_color(pr_t, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(pr_t, FONT_SM, 0);
    lv_obj_align(pr_t, LV_ALIGN_LEFT_MID, 0, 0);
    s.price_btn_lbl = lv_label_create(s.price_btn);
    lv_label_set_text(s.price_btn_lbl, "-");
    lv_obj_align(s.price_btn_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(s.price_btn_lbl, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(s.price_btn_lbl, FONT_SM, 0);
#else
    lv_obj_t * bc_btn = lv_btn_create(s.left_body);
    lv_obj_set_width(bc_btn, LV_PCT(100));
    lv_obj_set_height(bc_btn, LAND_BC_BTN_H);
    lv_obj_set_style_radius(bc_btn, 4, 0);
    lv_obj_add_event_cb(bc_btn, field_barcode_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * bc_l = lv_label_create(bc_btn);
    lv_label_set_text(bc_l, "BARCODE");
    lv_obj_set_style_text_color(bc_l, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(bc_l, FONT_SM, 0);
    lv_obj_align(bc_l, LV_ALIGN_LEFT_MID, 4, 0);
    s.barcode_btn_lbl = lv_label_create(bc_btn);
    lv_obj_align(s.barcode_btn_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_text_color(s.barcode_btn_lbl, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(s.barcode_btn_lbl, FONT_SM, 0);
#endif

    s.items_cont = lv_obj_create(s.left_body);
    lv_obj_set_width(s.items_cont, LV_PCT(100));
    lv_obj_set_flex_grow(s.items_cont, 1);
#if APP_PORTRAIT
    lv_obj_set_style_min_height(s.items_cont, 0, 0);
    lv_obj_set_scroll_dir(s.items_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s.items_cont, LV_SCROLLBAR_MODE_AUTO);
#endif
    lv_obj_set_style_bg_color(s.items_cont, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s.items_cont, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.items_cont, 1, 0);
    lv_obj_set_style_radius(s.items_cont, 6, 0);
    lv_obj_set_style_pad_all(s.items_cont, 2, 0);
    lv_obj_set_flex_flow(s.items_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s.items_cont, 2, 0);
    lv_obj_add_flag(s.items_cont, LV_OBJ_FLAG_SCROLLABLE);

#if !APP_PORTRAIT
    lv_obj_t * foot = lv_obj_create(s.left_body);
    lv_obj_set_width(foot, LV_PCT(100));
    lv_obj_set_height(foot, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(foot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(foot, 0, 0);
    lv_obj_set_style_pad_all(foot, 0, 0);
    lv_obj_set_flex_flow(foot, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * gt_row = lv_obj_create(foot);
    lv_obj_set_width(gt_row, LV_PCT(100));
    lv_obj_set_height(gt_row, 20);
    lv_obj_set_style_bg_opa(gt_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gt_row, 0, 0);
    lv_obj_set_flex_flow(gt_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gt_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(gt_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * gt_t = lv_label_create(gt_row);
    lv_label_set_text(gt_t, "GRAND TOTAL");
    lv_obj_set_style_text_color(gt_t, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(gt_t, FONT_SM, 0);
    s.grand_lbl = lv_label_create(gt_row);
    lv_obj_set_style_text_color(s.grand_lbl, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(s.grand_lbl, &lv_font_montserrat_14, 0);

    s.review_btn = lv_btn_create(foot);
    lv_obj_set_width(s.review_btn, LV_PCT(100));
    lv_obj_set_height(s.review_btn, LAND_REVIEW_BTN_H);
    lv_obj_set_style_bg_color(s.review_btn, lv_color_hex(0x0891B2), 0);
    lv_obj_set_style_radius(s.review_btn, 6, 0);
    lv_obj_add_event_cb(s.review_btn, start_review_cb, LV_EVENT_CLICKED, NULL);
    s.review_btn_lbl = lv_label_create(s.review_btn);
    lv_label_set_text(s.review_btn_lbl, "REVIEW");
    lv_obj_set_style_text_font(s.review_btn_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(s.review_btn_lbl);
#endif

    s.msg_lbl = lv_label_create(s.left_body);
    lv_obj_set_style_text_font(s.msg_lbl, FONT_SM, 0);
    lv_obj_add_flag(s.msg_lbl, LV_OBJ_FLAG_HIDDEN);

#if APP_PORTRAIT
    /* Grand total + REVIEW — pinned below item list, above keypad (always visible) */
    lv_obj_t * foot = lv_obj_create(s.left);
    lv_obj_set_width(foot, LV_PCT(100));
    lv_obj_set_height(foot, PORTR_ITEMS_FOOT_H);
    lv_obj_set_flex_grow(foot, 0);
    lv_obj_set_style_bg_color(foot, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(foot, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(foot, 1, 0);
    lv_obj_set_style_pad_all(foot, 4, 0);
    lv_obj_set_style_pad_row(foot, 4, 0);
    lv_obj_set_flex_flow(foot, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * gt_row = lv_obj_create(foot);
    lv_obj_set_width(gt_row, LV_PCT(100));
    lv_obj_set_height(gt_row, 22);
    lv_obj_set_style_bg_opa(gt_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gt_row, 0, 0);
    lv_obj_set_flex_flow(gt_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gt_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(gt_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * gt_t = lv_label_create(gt_row);
    lv_label_set_text(gt_t, "GRAND TOTAL");
    lv_obj_set_style_text_color(gt_t, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(gt_t, FONT_SM, 0);
    s.grand_lbl = lv_label_create(gt_row);
    lv_obj_set_style_text_color(s.grand_lbl, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(s.grand_lbl, &lv_font_montserrat_14, 0);

    s.review_btn = lv_btn_create(foot);
    lv_obj_set_width(s.review_btn, LV_PCT(100));
    lv_obj_set_flex_grow(s.review_btn, 1);
    lv_obj_set_style_bg_color(s.review_btn, lv_color_hex(0x0891B2), 0);
    lv_obj_set_style_radius(s.review_btn, 6, 0);
    lv_obj_add_event_cb(s.review_btn, start_review_cb, LV_EVENT_CLICKED, NULL);
    s.review_btn_lbl = lv_label_create(s.review_btn);
    lv_label_set_text(s.review_btn_lbl, "REVIEW");
    lv_obj_set_style_text_font(s.review_btn_lbl, FONT_SM, 0);
    lv_obj_center(s.review_btn_lbl);
#endif

    /* ---- Portrait: bottom dock = keypad + disc/add row ---- */
#if APP_PORTRAIT
    s.right = lv_obj_create(s.root);
    lv_obj_set_width(s.right, LV_PCT(100));
    lv_obj_set_height(s.right, PORTR_DOCK_H);
    lv_obj_set_flex_grow(s.right, 0);
    lv_obj_set_style_bg_color(s.right, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(s.right, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s.right, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(s.right, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.right, 1, 0);
    lv_obj_set_style_pad_all(s.right, 4, 0);
    lv_obj_set_style_pad_row(s.right, 2, 0);
    lv_obj_set_flex_flow(s.right, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.right, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * kp_panel = lv_obj_create(s.right);
    lv_obj_set_width(kp_panel, LV_PCT(100));
    lv_obj_set_height(kp_panel, PORTR_KP_OVERLAY_H - 4);
    lv_obj_set_flex_grow(kp_panel, 0);
    lv_obj_set_style_bg_opa(kp_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kp_panel, 0, 0);
    lv_obj_set_style_pad_all(kp_panel, 0, 0);
    lv_obj_clear_flag(kp_panel, LV_OBJ_FLAG_SCROLLABLE);

    s.keypad = ui_keypad_create(kp_panel, s.barcode, sizeof(s.barcode), true, 13, "", false, false);
    ui_keypad_set_intercept(sales_key_intercept);
    ui_keypad_set_change_cb(keypad_value_changed);
    dbg_log("H1", "ui_sales.c:create", "after_keypad", 0, 0, 0);

    s.right_bottom = lv_obj_create(s.right);
    lv_obj_set_width(s.right_bottom, LV_PCT(100));
    lv_obj_set_height(s.right_bottom, PORTR_ACTION_H);
    lv_obj_set_flex_grow(s.right_bottom, 0);
    lv_obj_set_style_bg_opa(s.right_bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.right_bottom, 0, 0);
    lv_obj_set_style_pad_all(s.right_bottom, 0, 0);
    lv_obj_set_flex_flow(s.right_bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s.right_bottom, 4, 0);
    lv_obj_clear_flag(s.right_bottom, LV_OBJ_FLAG_SCROLLABLE);

    s.disc_box = lv_obj_create(s.right_bottom);
    lv_obj_set_width(s.disc_box, 72);
    lv_obj_set_height(s.disc_box, PORTR_ACTION_H - 8);
    lv_obj_set_style_pad_all(s.disc_box, 2, 0);
    lv_obj_clear_flag(s.disc_box, LV_OBJ_FLAG_SCROLLABLE);
    style_field_box(s.disc_box, false);
    s.disc_dd = lv_dropdown_create(s.disc_box);
    lv_dropdown_set_options(s.disc_dd, "0%\n10%\n20%");
    lv_dropdown_set_symbol(s.disc_dd, LV_SYMBOL_DOWN);
    lv_dropdown_set_dir(s.disc_dd, LV_DIR_BOTTOM);
    lv_obj_set_width(s.disc_dd, LV_PCT(100));
    lv_obj_set_height(s.disc_dd, PORTR_ACTION_H - 12);
    lv_obj_center(s.disc_dd);
    style_disc_dropdown(s.disc_dd);
    lv_obj_add_event_cb(s.disc_dd, disc_dd_event, LV_EVENT_ALL, NULL);
    sync_disc_dropdown();

    lv_obj_t * add_btn = lv_btn_create(s.right_bottom);
    lv_obj_set_flex_grow(add_btn, 1);
    lv_obj_set_height(add_btn, PORTR_ACTION_H - 8);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(COL_GREEN_DIM), 0);
    lv_obj_add_event_cb(add_btn, add_item_cb, LV_EVENT_CLICKED, NULL);
    s.add_btn_lbl = lv_label_create(add_btn);
    lv_label_set_text(s.add_btn_lbl, "ADD ITEM");
    lv_obj_center(s.add_btn_lbl);

    s.cancel_edit_btn = NULL;
#else
    /* ---- Landscape: right column keypad + price/disc/add ---- */
    s.right = lv_obj_create(s.root);
    lv_obj_set_size(s.right, RIGHT_W, LV_PCT(100));
    lv_obj_set_style_bg_color(s.right, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s.right, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.right, 1, 0);
    lv_obj_set_style_border_side(s.right, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_pad_all(s.right, 4, 0);
    lv_obj_set_flex_flow(s.right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(s.right, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * kp_wrap = lv_obj_create(s.right);
    lv_obj_set_width(kp_wrap, LV_PCT(100));
    lv_obj_set_flex_grow(kp_wrap, 1);
    lv_obj_set_style_min_height(kp_wrap, KP_WRAP_MIN_H, 0);
    lv_obj_set_style_max_height(kp_wrap, (CONTENT_H - 100), 0);
    lv_obj_set_style_bg_opa(kp_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kp_wrap, 0, 0);
    lv_obj_set_style_pad_all(kp_wrap, 0, 0);
    lv_obj_clear_flag(kp_wrap, LV_OBJ_FLAG_SCROLLABLE);

    s.keypad = ui_keypad_create(kp_wrap, s.barcode, sizeof(s.barcode), true, 13, "BARCODE", true, true);
    ui_keypad_set_intercept(sales_key_intercept);
    ui_keypad_set_change_cb(keypad_value_changed);
    dbg_log("H1", "ui_sales.c:create", "after_keypad", 0, 0, 0);

    s.right_bottom = lv_obj_create(s.right);
    lv_obj_set_width(s.right_bottom, LV_PCT(100));
    lv_obj_set_height(s.right_bottom, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s.right_bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(s.right_bottom, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(s.right_bottom, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.right_bottom, 1, 0);
    lv_obj_set_flex_flow(s.right_bottom, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.right_bottom, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * pd_row = lv_obj_create(s.right_bottom);
    lv_obj_set_width(pd_row, LV_PCT(100));
    lv_obj_set_height(pd_row, LAND_PD_ROW_H);
    lv_obj_set_style_bg_opa(pd_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pd_row, 0, 0);
    lv_obj_set_style_pad_all(pd_row, 0, 0);
    lv_obj_set_flex_flow(pd_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(pd_row, 4, 0);
    lv_obj_clear_flag(pd_row, LV_OBJ_FLAG_SCROLLABLE);

    s.price_btn = lv_btn_create(pd_row);
    lv_obj_set_flex_grow(s.price_btn, 1);
    lv_obj_set_height(s.price_btn, LAND_PD_ROW_H);
    lv_obj_set_style_pad_all(s.price_btn, 6, 0);
    lv_obj_set_style_shadow_width(s.price_btn, 0, 0);
    lv_obj_set_style_bg_color(s.price_btn, lv_color_hex(0x1F2937), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(s.price_btn, lv_color_hex(COL_CYAN), LV_STATE_PRESSED);
    lv_obj_add_event_cb(s.price_btn, field_price_cb, LV_EVENT_CLICKED, NULL);
    style_field_box(s.price_btn, false);
    lv_obj_t * pr_t = lv_label_create(s.price_btn);
    lv_label_set_text(pr_t, "PRICE");
    lv_obj_set_style_text_color(pr_t, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(pr_t, FONT_SM, 0);
    lv_obj_align(pr_t, LV_ALIGN_TOP_LEFT, 0, 0);
    s.price_btn_lbl = lv_label_create(s.price_btn);
    lv_label_set_text(s.price_btn_lbl, "-");
    lv_obj_align(s.price_btn_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_color(s.price_btn_lbl, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(s.price_btn_lbl, &lv_font_montserrat_12, 0);

    s.disc_box = lv_obj_create(pd_row);
    lv_obj_set_flex_grow(s.disc_box, 1);
    lv_obj_set_height(s.disc_box, LAND_PD_ROW_H);
    lv_obj_set_style_pad_all(s.disc_box, 6, 0);
    lv_obj_clear_flag(s.disc_box, LV_OBJ_FLAG_SCROLLABLE);
    style_field_box(s.disc_box, false);
    lv_obj_t * dt = lv_label_create(s.disc_box);
    lv_label_set_text(dt, "DISC%");
    lv_obj_set_style_text_color(dt, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(dt, FONT_SM, 0);
    lv_obj_align(dt, LV_ALIGN_TOP_LEFT, 0, 0);
    s.disc_dd = lv_dropdown_create(s.disc_box);
    lv_dropdown_set_options(s.disc_dd, "0%\n10%\n20%");
    lv_dropdown_set_symbol(s.disc_dd, LV_SYMBOL_DOWN);
    lv_dropdown_set_dir(s.disc_dd, LV_DIR_BOTTOM);
    lv_obj_set_width(s.disc_dd, LV_PCT(100));
    lv_obj_set_height(s.disc_dd, 22);
    lv_obj_align(s.disc_dd, LV_ALIGN_BOTTOM_MID, 0, 0);
    style_disc_dropdown(s.disc_dd);
    lv_obj_add_event_cb(s.disc_dd, disc_dd_event, LV_EVENT_ALL, NULL);
    sync_disc_dropdown();

    lv_obj_t * add_btn = lv_btn_create(s.right_bottom);
    lv_obj_set_width(add_btn, LV_PCT(100));
    lv_obj_set_height(add_btn, 26);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(COL_GREEN_DIM), 0);
    lv_obj_add_event_cb(add_btn, add_item_cb, LV_EVENT_CLICKED, NULL);
    s.add_btn_lbl = lv_label_create(add_btn);
    lv_label_set_text(s.add_btn_lbl, "ADD ITEM");
    lv_obj_center(s.add_btn_lbl);

    s.cancel_edit_btn = lv_btn_create(s.right_bottom);
    lv_obj_set_width(s.cancel_edit_btn, LV_PCT(100));
    lv_obj_set_height(s.cancel_edit_btn, 18);
    lv_obj_add_event_cb(s.cancel_edit_btn, cancel_edit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * cel = lv_label_create(s.cancel_edit_btn);
    lv_label_set_text(cel, "Cancel edit");
    lv_obj_set_style_text_font(cel, FONT_SM, 0);
    lv_obj_center(cel);
    lv_obj_add_flag(s.cancel_edit_btn, LV_OBJ_FLAG_HIDDEN);
#endif

    /* Review overlay — full left panel (prototype: absolute inset-0 on left column) */
    s.review_overlay = lv_obj_create(s.left);
    lv_obj_set_style_bg_color(s.review_overlay, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s.review_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s.review_overlay, 0, 0);
    lv_obj_set_style_pad_all(s.review_overlay, 6, 0);
    lv_obj_set_style_pad_row(s.review_overlay, 2, 0);
    lv_obj_set_flex_flow(s.review_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.review_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s.review_overlay, LV_OBJ_FLAG_SCROLLABLE);
    overlay_prepare(s.review_overlay);

    lv_obj_t * rh = lv_obj_create(s.review_overlay);
    lv_obj_set_width(rh, LV_PCT(100));
#if APP_PORTRAIT
    lv_obj_set_height(rh, 32);
#else
    lv_obj_set_height(rh, 24);
#endif
    lv_obj_set_style_bg_opa(rh, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rh, 0, 0);
    lv_obj_set_flex_flow(rh, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rh, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(rh, LV_OBJ_FLAG_SCROLLABLE);
    s.review_title_lbl = lv_label_create(rh);
    lv_label_set_text(s.review_title_lbl, "REVIEW SALES");
    lv_obj_set_style_text_color(s.review_title_lbl, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(s.review_title_lbl, FONT_SM, 0);
    s.review_close_btn = lv_btn_create(rh);
#if APP_PORTRAIT
    lv_obj_set_size(s.review_close_btn, 44, 30);
#else
    lv_obj_set_size(s.review_close_btn, 36, 26);
#endif
    lv_obj_set_style_bg_color(s.review_close_btn, lv_color_hex(COL_PANEL2), 0);
    lv_obj_set_style_border_color(s.review_close_btn, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_border_width(s.review_close_btn, 1, 0);
    lv_obj_set_style_radius(s.review_close_btn, 6, 0);
    lv_obj_t * rxl = lv_label_create(s.review_close_btn);
    lv_label_set_text(rxl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(rxl, &lv_font_montserrat_14, 0);
    lv_obj_center(rxl);
    lv_obj_add_event_cb(s.review_close_btn, cancel_review_cb, LV_EVENT_CLICKED, NULL);
    touch_expand_btn(s.review_close_btn, 12);

    s.review_list = lv_obj_create(s.review_overlay);
    lv_obj_set_width(s.review_list, LV_PCT(100));
    lv_obj_set_flex_grow(s.review_list, 1);
    lv_obj_set_style_min_height(s.review_list, 40, 0);
    lv_obj_set_style_bg_color(s.review_list, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s.review_list, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.review_list, 1, 0);
    lv_obj_set_style_radius(s.review_list, 4, 0);
    lv_obj_set_style_pad_all(s.review_list, 2, 0);
    lv_obj_set_flex_flow(s.review_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.review_list, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * rfoot = lv_obj_create(s.review_overlay);
    lv_obj_set_width(rfoot, LV_PCT(100));
    lv_obj_set_height(rfoot, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(rfoot, 0);
#if APP_PORTRAIT
    lv_obj_set_style_min_height(rfoot, 108, 0);
#else
    lv_obj_set_style_min_height(rfoot, LAND_REVIEW_FOOT_H, 0);
#endif
    lv_obj_set_style_bg_color(rfoot, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(rfoot, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(rfoot, 1, 0);
    lv_obj_set_style_radius(rfoot, 6, 0);
    lv_obj_set_style_pad_all(rfoot, 6, 0);
    lv_obj_set_flex_flow(rfoot, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(rfoot, 4, 0);
    lv_obj_clear_flag(rfoot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * sys_row = lv_obj_create(rfoot);
    lv_obj_set_width(sys_row, LV_PCT(100));
    lv_obj_set_height(sys_row, 16);
    lv_obj_set_style_bg_opa(sys_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sys_row, 0, 0);
    lv_obj_set_flex_flow(sys_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sys_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(sys_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * st = lv_label_create(sys_row);
    lv_label_set_text(st, "SYSTEM TOTAL");
    lv_obj_set_style_text_font(st, FONT_SM, 0);
    s.review_sys_lbl = lv_label_create(sys_row);
    lv_obj_set_style_text_color(s.review_sys_lbl, lv_color_hex(COL_YELLOW), 0);

    lv_obj_t * qty_row = lv_obj_create(rfoot);
    lv_obj_set_width(qty_row, LV_PCT(100));
    lv_obj_set_height(qty_row, 16);
    lv_obj_set_style_bg_opa(qty_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(qty_row, 0, 0);
    lv_obj_set_flex_flow(qty_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(qty_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(qty_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * qt = lv_label_create(qty_row);
    lv_label_set_text(qt, "TOTAL QTY");
    lv_obj_set_style_text_font(qt, FONT_SM, 0);
    s.review_qty_lbl = lv_label_create(qty_row);
    lv_obj_set_style_text_color(s.review_qty_lbl, lv_color_hex(COL_CYAN), 0);

    s.review_actions_panel = lv_obj_create(rfoot);
    lv_obj_set_width(s.review_actions_panel, LV_PCT(100));
    lv_obj_set_height(s.review_actions_panel, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s.review_actions_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.review_actions_panel, 0, 0);
    lv_obj_set_style_pad_all(s.review_actions_panel, 0, 0);
    lv_obj_set_flex_flow(s.review_actions_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s.review_actions_panel, 4, 0);
    lv_obj_clear_flag(s.review_actions_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * in_row = lv_obj_create(s.review_actions_panel);
    lv_obj_set_width(in_row, LV_PCT(100));
    lv_obj_set_height(in_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(in_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(in_row, 0, 0);
    lv_obj_set_flex_flow(in_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(in_row, 2, 0);
    lv_obj_clear_flag(in_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * it = lv_label_create(in_row);
    lv_label_set_text(it, "INPUT TOTAL TO CONFIRM:");
    lv_obj_set_style_text_color(it, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(it, FONT_SM, 0);
    s.review_manual_lbl = lv_label_create(in_row);
    lv_label_set_text(s.review_manual_lbl, "P0.00");
    lv_obj_set_style_text_color(s.review_manual_lbl, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(s.review_manual_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s.review_manual_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(s.review_manual_lbl, LV_PCT(100));

    s.review_submit_btn = lv_btn_create(s.review_actions_panel);
    lv_obj_set_width(s.review_submit_btn, LV_PCT(100));
    lv_obj_set_height(s.review_submit_btn, 30);
    lv_obj_set_style_bg_color(s.review_submit_btn, lv_color_hex(0x16A34A), 0);
    lv_obj_add_state(s.review_submit_btn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s.review_submit_btn, final_submit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * sl = lv_label_create(s.review_submit_btn);
    lv_label_set_text(sl, "CONFIRM & SUBMIT SALE");
    lv_obj_set_style_text_font(sl, FONT_SM, 0);
    lv_obj_center(sl);

    lv_obj_t * back_btn = lv_btn_create(s.review_actions_panel);
    lv_obj_set_width(back_btn, LV_PCT(100));
    lv_obj_set_height(back_btn, 20);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(COL_PANEL2), 0);
    lv_obj_add_event_cb(back_btn, cancel_review_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * bl = lv_label_create(back_btn);
    lv_label_set_text(bl, "Back / Edit Items");
    lv_obj_set_style_text_color(bl, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(bl, FONT_SM, 0);
    lv_obj_center(bl);

    s.review_success_panel = lv_obj_create(rfoot);
    lv_obj_set_width(s.review_success_panel, LV_PCT(100));
    lv_obj_set_height(s.review_success_panel, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s.review_success_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.review_success_panel, 0, 0);
    lv_obj_set_flex_flow(s.review_success_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s.review_success_panel, 6, 0);
    lv_obj_set_flex_align(s.review_success_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s.review_success_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s.review_success_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * succ_lbl = lv_label_create(s.review_success_panel);
    lv_label_set_text(succ_lbl, LV_SYMBOL_OK " SUCCESSFULLY SUBMITTED");
    lv_obj_set_style_text_color(succ_lbl, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(succ_lbl, FONT_SM, 0);

    s.review_done_btn = lv_btn_create(s.review_success_panel);
    lv_obj_set_width(s.review_done_btn, LV_PCT(100));
#if APP_PORTRAIT
    lv_obj_set_height(s.review_done_btn, 44);
#else
    lv_obj_set_height(s.review_done_btn, 32);
#endif
    lv_obj_set_style_bg_color(s.review_done_btn, lv_color_hex(0x155E75), 0);
    lv_obj_set_style_border_color(s.review_done_btn, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_border_width(s.review_done_btn, 1, 0);
    lv_obj_add_event_cb(s.review_done_btn, cancel_review_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * dl = lv_label_create(s.review_done_btn);
    lv_label_set_text(dl, "CLOSE");
    lv_obj_set_style_text_font(dl, &lv_font_montserrat_14, 0);
    lv_obj_center(dl);
    touch_expand_btn(s.review_done_btn, 10);

    /* Confirm dialog — centered modal, dark panel, equal YES / BACK buttons */
    s.confirm_overlay = lv_obj_create(s.left);
    lv_obj_set_style_bg_color(s.confirm_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s.confirm_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s.confirm_overlay, 0, 0);
    lv_obj_set_style_pad_all(s.confirm_overlay, 8, 0);
    lv_obj_add_flag(s.confirm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s.confirm_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s.confirm_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.confirm_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    overlay_prepare(s.confirm_overlay);

    lv_obj_t * cbox = lv_obj_create(s.confirm_overlay);
    lv_obj_set_width(cbox, LV_PCT(94));
    lv_obj_set_height(cbox, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(cbox, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_border_color(cbox, lv_color_hex(COL_CYAN_DIM), 0);
    lv_obj_set_style_border_width(cbox, 1, 0);
    lv_obj_set_style_radius(cbox, 6, 0);
    lv_obj_set_style_pad_all(cbox, 10, 0);
    lv_obj_set_style_pad_row(cbox, 8, 0);
    lv_obj_set_flex_flow(cbox, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(cbox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * ct = lv_label_create(cbox);
    lv_label_set_text(ct, "CONFIRM SUBMISSION");
    lv_obj_set_style_text_color(ct, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(ct, FONT_SM, 0);
    lv_obj_set_width(ct, LV_PCT(100));

    s.confirm_body_lbl = lv_label_create(cbox);
    lv_label_set_text(s.confirm_body_lbl, "Submit total sales?");
    lv_obj_set_style_text_color(s.confirm_body_lbl, lv_color_hex(0xE5E7EB), 0);
    lv_obj_set_style_text_font(s.confirm_body_lbl, FONT_SM, 0);
    lv_obj_set_style_text_align(s.confirm_body_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s.confirm_body_lbl, LV_PCT(100));
    lv_label_set_long_mode(s.confirm_body_lbl, LV_LABEL_LONG_WRAP);

    s.confirm_amt_lbl = lv_label_create(cbox);
    lv_obj_add_flag(s.confirm_amt_lbl, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * crow = lv_obj_create(cbox);
    lv_obj_set_width(crow, LV_PCT(100));
    lv_obj_set_height(crow, 30);
    lv_obj_set_style_bg_opa(crow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(crow, 0, 0);
    lv_obj_set_style_pad_all(crow, 0, 0);
    lv_obj_set_style_pad_column(crow, 8, 0);
    lv_obj_set_flex_flow(crow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(crow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(crow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * yes = lv_btn_create(crow);
    lv_obj_set_flex_grow(yes, 1);
    lv_obj_set_height(yes, 28);
    lv_obj_set_style_bg_color(yes, lv_color_hex(0x16A34A), 0);
    lv_obj_set_style_radius(yes, 4, 0);
    lv_obj_set_style_pad_all(yes, 0, 0);
    lv_obj_add_event_cb(yes, confirm_yes_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * yl = lv_label_create(yes);
    lv_label_set_text(yl, "YES");
    lv_obj_set_style_text_font(yl, FONT_SM, 0);
    lv_obj_center(yl);

    lv_obj_t * no = lv_btn_create(crow);
    lv_obj_set_flex_grow(no, 1);
    lv_obj_set_height(no, 28);
    lv_obj_set_style_bg_color(no, lv_color_hex(COL_PANEL2), 0);
    lv_obj_set_style_radius(no, 4, 0);
    lv_obj_set_style_pad_all(no, 0, 0);
    lv_obj_add_event_cb(no, confirm_no_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * nl = lv_label_create(no);
    lv_label_set_text(nl, "BACK");
    lv_obj_set_style_text_font(nl, FONT_SM, 0);
    lv_obj_center(nl);

    /* Mismatch overlay */
    s.mismatch_overlay = lv_obj_create(s.left);
    lv_obj_set_style_bg_color(s.mismatch_overlay, lv_color_hex(0x450A0A), 0);
    lv_obj_set_style_bg_opa(s.mismatch_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s.mismatch_overlay, 0, 0);
    lv_obj_add_flag(s.mismatch_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_flex_flow(s.mismatch_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.mismatch_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    overlay_prepare(s.mismatch_overlay);
    lv_obj_t * mt = lv_label_create(s.mismatch_overlay);
    lv_label_set_text(mt, "TOTAL MISMATCH!");
    lv_obj_set_style_text_color(mt, lv_color_hex(0xFFFFFF), 0);
    s.mismatch_info = lv_label_create(s.mismatch_overlay);
    lv_obj_set_style_text_align(s.mismatch_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t * mok = lv_btn_create(s.mismatch_overlay);
    lv_obj_add_event_cb(mok, mismatch_ok_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * ml = lv_label_create(mok);
    lv_label_set_text(ml, "Check Again");
    lv_obj_center(ml);

    /* Timer redraws entire screen constantly — causes stripe/corruption on JC3248.
     * Keypad still updates on field tap / add / review (refresh_keypad_target). */
#if defined(UI_SALES_KP_TIMER)
    s_kp_timer = lv_timer_create(sales_kp_timer, 500, NULL);
#endif
    load_from_storage();
#if APP_PORTRAIT
    portrait_kp_show();
    lv_obj_update_layout(s.root);
    if(s.right && lv_obj_is_valid(s.right)) {
        lv_obj_invalidate(s.right);
    }
#endif
    dbg_log_layout_dims("ui_sales.c:create");
    // #region agent log
    dbg_log("H1", "ui_sales.c:create", "exit_ok",
            (int32_t)lv_obj_get_width(s.root), (int32_t)lv_obj_get_height(s.root), PORTR_ACTION_H);
    dbg_log_heap("H1", "ui_sales.c:create", "exit_heap");
    // #endregion
}
