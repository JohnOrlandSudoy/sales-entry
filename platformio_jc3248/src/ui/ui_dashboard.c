#include "ui_dashboard.h"
#include "ui_colors.h"
#include "ui_touch.h"
#include "ui_keypad.h"
#include <stdint.h>
#include "app_data.h"
#include "app_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void rebuild_table(void);
static void rebuild_recon_dropdown(void);

#define CONTENT_H     (APP_SCREEN_H - APP_HDR_H)
#define MAX_CASH      11

#if APP_SCREEN_H > APP_SCREEN_W
  #define APP_PORTRAIT        1
  #define PORTR_RECON_H       38
  #define PORTR_KP_ROW_H      22
  #define PORTR_KP_GRID_H     (PORTR_KP_ROW_H * 4 + 2 * 3)
  #define PORTR_KP_DISP_H     18
  #define PORTR_RECON_BTN_H   32
  #define PORTR_RECON_DOCK_H  (PORTR_KP_DISP_H + PORTR_KP_GRID_H + PORTR_RECON_BTN_H + 12)
  #define COL_NAME_W          68
  #define COL_SALES_W         40
  #define COL_ITM_W           22
  #define COL_STAT_W          38  
  #define COL_REC_W           30
#else
  #define APP_PORTRAIT        0
  #define KEYPAD_W            158
  #define COL_NAME_W          52
  #define COL_SALES_W         44
  #define COL_ITM_W           24
  #define COL_STAT_W          44
  #define COL_REC_W           36
#endif

#if LV_FONT_MONTSERRAT_10
  #define FONT_SM  (&lv_font_montserrat_10)
#else
  #define FONT_SM  (&lv_font_montserrat_12)
#endif

typedef struct {
    const char * id;
    const char * name;
    float total_sales;
    int item_count;
    char status[8];
    bool all_closed;
    app_reconciliation_t * rec;
} emp_summary_t;

typedef struct {
    lv_obj_t * root;
    lv_obj_t * left;
    lv_obj_t * right;
    lv_obj_t * kp_overlay;
    lv_obj_t * recon_bar;
    lv_obj_t * table_body;
    lv_obj_t * sent_badge;
    lv_obj_t * recon_dd;
    lv_obj_t * items_overlay;
    lv_obj_t * items_title;
    lv_obj_t * items_list;
    ui_keypad_t keypad;
    char cash_input[MAX_CASH];
    char recon_emp_id[APP_ID_LEN];
    bool show_keypad;
    void (*on_go_sales)(const char *);
} dash_ui_t;

static dash_ui_t s;

#if APP_PORTRAIT
static void dash_kp_show(void)
{
    if(!s.kp_overlay || !lv_obj_is_valid(s.kp_overlay)) return;
    lv_obj_clear_flag(s.kp_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s.kp_overlay);
}

static void dash_kp_hide(void)
{
    if(!s.kp_overlay || !lv_obj_is_valid(s.kp_overlay)) return;
    lv_obj_add_flag(s.kp_overlay, LV_OBJ_FLAG_HIDDEN);
}
#else
static void dash_kp_show(void) {}
static void dash_kp_hide(void) {}
#endif

static bool dash_is_sent(void)
{
    return app_is_date_sent(app_today_date());
}

static const char * dash_emp_id_from_idx(intptr_t idx)
{
    app_settings_t * st = app_settings();
    if(idx < 0 || idx >= st->employee_count) return NULL;
    return st->employees[idx].id;
}

static void list_items_cb(lv_event_t * e)
{
    const char * emp_id = dash_emp_id_from_idx((intptr_t)lv_event_get_user_data(e));
    if(!emp_id) return;

    lv_label_set_text_fmt(s.items_title, "ITEMS: %s", app_employee_name(emp_id));
    lv_obj_clean(s.items_list);

    for(int i = 0; i < app_entry_count(); i++) {
        app_sales_entry_t * entry = app_get_entry(i);
        if(!entry || strcmp(entry->date, app_today_date()) != 0) continue;
        if(strcmp(entry->employee_id, emp_id) != 0) continue;
        for(int j = 0; j < entry->item_count; j++) {
            app_sales_item_t * it = &entry->items[j];
            lv_obj_t * row = lv_obj_create(s.items_list);
            lv_obj_set_width(row, LV_PCT(100));
            lv_obj_set_height(row, 16);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_all(row, 2, 0);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t * a = lv_label_create(row);
            lv_label_set_text(a, it->barcode);
            lv_obj_set_style_text_color(a, lv_color_hex(COL_CYAN), 0);
            lv_obj_set_style_text_font(a, FONT_SM, 0);
            char buf[20];
            snprintf(buf, sizeof(buf), "P%.2f", it->line_total);
            lv_obj_t * b = lv_label_create(row);
            lv_label_set_text(b, buf);
            lv_obj_set_style_text_color(b, lv_color_hex(COL_YELLOW), 0);
            lv_obj_set_style_text_font(b, FONT_SM, 0);
        }
    }
    lv_obj_clear_flag(s.items_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s.items_overlay);
}

static void close_items_cb(lv_event_t * e)
{
    (void)e;
    lv_obj_add_flag(s.items_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void build_summary(emp_summary_t * sum, app_employee_t * emp)
{
    sum->id = emp->id;
    sum->name = emp->name;
    sum->total_sales = 0;
    sum->item_count = 0;
    strcpy(sum->status, "-");
    sum->all_closed = false;
    sum->rec = NULL;

    bool has_open = false;
    int entry_n = 0;
    for(int i = 0; i < app_entry_count(); i++) {
        app_sales_entry_t * e = app_get_entry(i);
        if(!e || strcmp(e->date, app_today_date()) != 0) continue;
        if(strcmp(e->employee_id, emp->id) != 0) continue;
        entry_n++;
        sum->total_sales += e->grand_total;
        sum->item_count += e->item_count;
        if(!e->closed) has_open = true;
    }

    if(app_is_entry_sent(emp->id, app_today_date())) {
        strcpy(sum->status, "SENT");
    }
    else if(entry_n == 0) {
        strcpy(sum->status, "-");
    }
    else if(has_open) {
        strcpy(sum->status, "OPEN");
    }
    else {
        strcpy(sum->status, "CLOSED");
    }
    sum->all_closed = entry_n > 0 && !has_open;
    sum->rec = app_find_reconciliation(emp->id, sum->total_sales);
}

static lv_obj_t * make_status_badge(lv_obj_t * parent, const char * status)
{
    lv_obj_t * b = lv_obj_create(parent);
    lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(b, 4, 0);
    lv_obj_set_style_pad_ver(b, 2, 0);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    uint32_t bg = 0x1F2937, fg = COL_GRAY;
    if(strcmp(status, "OPEN") == 0) { bg = 0x7C2D12; fg = 0xFB923C; }
    else if(strcmp(status, "CLOSED") == 0) { bg = 0x14532D; fg = COL_GREEN; }
    else if(strcmp(status, "SENT") == 0) { bg = 0x164E63; fg = COL_CYAN; }
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, status);
    lv_obj_set_style_text_color(l, lv_color_hex(fg), 0);
    lv_obj_set_style_text_font(l, FONT_SM, 0);
    lv_obj_center(l);
    return b;
}

static void go_sales_cb(lv_event_t * e)
{
    const char * emp_id = dash_emp_id_from_idx((intptr_t)lv_event_get_user_data(e));
    if(!emp_id || !s.on_go_sales) return;
    s.on_go_sales(emp_id);
}

static void go_sales_edit_cb(lv_event_t * e)
{
    app_set_sales_open_for_edit(true);
    go_sales_cb(e);
}

static void go_sales_add_cb(lv_event_t * e)
{
    app_set_sales_open_for_edit(false);
    go_sales_cb(e);
}

static void retry_recon_cb(lv_event_t * e)
{
    const char * emp_id = dash_emp_id_from_idx((intptr_t)lv_event_get_user_data(e));
    if(!emp_id) return;
    strncpy(s.recon_emp_id, emp_id, sizeof(s.recon_emp_id) - 1);
    s.show_keypad = true;
    s.cash_input[0] = '\0';
#if APP_PORTRAIT
    dash_kp_show();
#else
    if(s.right) {
        lv_obj_clear_flag(s.right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(s.left, APP_SCREEN_W - KEYPAD_W);
    }
#endif
    ui_keypad_refresh(&s.keypad);
    rebuild_recon_dropdown();
}

static void style_recon_dd(lv_obj_t * dd)
{
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x111827), LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_hex(COL_GREEN), LV_PART_MAIN);
    lv_obj_set_style_border_width(dd, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(dd, FONT_SM, LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_hex(COL_GRAY_TEXT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x030712), LV_PART_ITEMS);
    lv_obj_set_style_text_color(dd, lv_color_hex(COL_GREEN), LV_PART_ITEMS);
}

static void rebuild_recon_dropdown(void)
{
    char opts[256] = "Select...";
    app_settings_t * st = app_settings();
    for(int i = 0; i < st->employee_count; i++) {
        emp_summary_t sum;
        build_summary(&sum, &st->employees[i]);
        if(!sum.all_closed) continue;
        if(sum.rec && sum.rec->match) continue;
        char line[64];
        snprintf(line, sizeof(line), "\n%s (P%.2f)", sum.name, sum.total_sales);
        if(strlen(opts) + strlen(line) < sizeof(opts) - 1) strcat(opts, line);
    }
    lv_dropdown_set_options(s.recon_dd, opts);
    lv_dropdown_set_selected(s.recon_dd, 0);
}

static void recon_dd_changed(lv_event_t * e)
{
    (void)e;
    uint32_t sel = lv_dropdown_get_selected(s.recon_dd);
    if(sel == 0) {
        s.recon_emp_id[0] = '\0';
        return;
    }
    app_settings_t * st = app_settings();
    int idx = 0;
    for(int i = 0; i < st->employee_count; i++) {
        emp_summary_t sum;
        build_summary(&sum, &st->employees[i]);
        if(!sum.all_closed) continue;
        if(sum.rec && sum.rec->match) continue;
        idx++;
        if((uint32_t)idx == sel) {
            strncpy(s.recon_emp_id, st->employees[i].id, sizeof(s.recon_emp_id) - 1);
            return;
        }
    }
    s.recon_emp_id[0] = '\0';
}

static void cash_toggle_cb(lv_event_t * e)
{
    (void)e;
    if(dash_is_sent()) return;
    s.show_keypad = !s.show_keypad;
#if APP_PORTRAIT
    if(s.show_keypad) dash_kp_show();
    else dash_kp_hide();
#else
    if(s.show_keypad) {
        lv_obj_clear_flag(s.right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(s.left, APP_SCREEN_W - KEYPAD_W);
    }
    else {
        lv_obj_add_flag(s.right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(s.left, APP_SCREEN_W);
    }
#endif
}

static void reconcile_cb(lv_event_t * e)
{
    (void)e;
    if(dash_is_sent() || !s.recon_emp_id[0] || !s.cash_input[0]) return;

    emp_summary_t sum = {0};
    app_settings_t * st = app_settings();
    for(int i = 0; i < st->employee_count; i++) {
        if(strcmp(st->employees[i].id, s.recon_emp_id) == 0) {
            build_summary(&sum, &st->employees[i]);
            break;
        }
    }
    float actual = strtof(s.cash_input, NULL);
    app_reconciliation_t rec;
    strncpy(rec.employee_id, s.recon_emp_id, sizeof(rec.employee_id) - 1);
    rec.system_total = sum.total_sales;
    rec.actual_cash = actual;
    rec.match = fabsf(actual - sum.total_sales) < 0.01f;
    app_upsert_reconciliation(&rec);

    s.cash_input[0] = '\0';
    s.recon_emp_id[0] = '\0';
    s.show_keypad = false;
#if APP_PORTRAIT
    dash_kp_hide();
#else
    lv_obj_add_flag(s.right, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(s.left, APP_SCREEN_W);
#endif
    ui_dashboard_reload();
}

static void rebuild_table(void)
{
    if(!s.table_body || !lv_obj_is_valid(s.table_body)) return;
    lv_obj_clean(s.table_body);
    app_settings_t * st = app_settings();

    for(int i = 0; i < st->employee_count; i++) {
        emp_summary_t sum;
        build_summary(&sum, &st->employees[i]);

        lv_obj_t * row = lv_obj_create(s.table_body);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, APP_PORTRAIT ? 24 : 22);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(COL_BORDER), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_set_style_pad_column(row, 2, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * name = lv_label_create(row);
        lv_obj_set_width(name, COL_NAME_W);
        lv_label_set_text(name, sum.name);
        lv_obj_set_style_text_color(name, lv_color_hex(COL_GREEN), 0);
        lv_obj_set_style_text_font(name, FONT_SM, 0);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

        char sales[16];
        snprintf(sales, sizeof(sales), "P%.0f", sum.total_sales);
        lv_obj_t * sl = lv_label_create(row);
        lv_obj_set_width(sl, COL_SALES_W);
        lv_label_set_text(sl, sales);
        lv_obj_set_style_text_color(sl, lv_color_hex(COL_YELLOW), 0);
        lv_obj_set_style_text_font(sl, FONT_SM, 0);
        lv_obj_set_style_text_align(sl, LV_TEXT_ALIGN_RIGHT, 0);

        char ic[8];
        snprintf(ic, sizeof(ic), "%d", sum.item_count);
        lv_obj_t * il = lv_label_create(row);
        lv_obj_set_width(il, COL_ITM_W);
        lv_label_set_text(il, ic);
        lv_obj_set_style_text_color(il, lv_color_hex(COL_GRAY_TEXT), 0);
        lv_obj_set_style_text_font(il, FONT_SM, 0);
        lv_obj_set_style_text_align(il, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * st_cell = lv_obj_create(row);
        lv_obj_set_width(st_cell, COL_STAT_W);
        lv_obj_set_height(st_cell, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(st_cell, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(st_cell, 0, 0);
        lv_obj_set_style_pad_all(st_cell, 0, 0);
        lv_obj_clear_flag(st_cell, LV_OBJ_FLAG_SCROLLABLE);
        make_status_badge(st_cell, sum.status);

        lv_obj_t * rec_cell = lv_obj_create(row);
        lv_obj_set_width(rec_cell, COL_REC_W);
        lv_obj_set_height(rec_cell, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(rec_cell, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(rec_cell, 0, 0);
        lv_obj_clear_flag(rec_cell, LV_OBJ_FLAG_SCROLLABLE);
        if(sum.rec) {
            lv_obj_t * rl = lv_label_create(rec_cell);
            if(sum.rec->match) {
                lv_label_set_text(rl, LV_SYMBOL_OK);
                lv_obj_set_style_text_color(rl, lv_color_hex(COL_GREEN), 0);
            }
            else {
                lv_label_set_text(rl, "X");
                lv_obj_set_style_text_color(rl, lv_color_hex(COL_RED), 0);
                lv_obj_t * retry = lv_btn_create(rec_cell);
                lv_obj_set_size(retry, 36, 22);
                lv_obj_align(retry, LV_ALIGN_RIGHT_MID, 0, 0);
                lv_obj_t * tl = lv_label_create(retry);
                lv_label_set_text(tl, "RETRY");
                lv_obj_set_style_text_font(tl, FONT_SM, 0);
                lv_obj_center(tl);
                lv_obj_add_event_cb(retry, retry_recon_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
                ui_touch_expand(retry, 8);
            }
            lv_obj_set_style_text_font(rl, FONT_SM, 0);
        }
        else {
            lv_obj_t * rl = lv_label_create(rec_cell);
            lv_label_set_text(rl, "-");
            lv_obj_set_style_text_color(rl, lv_color_hex(COL_GRAY), 0);
            lv_obj_set_style_text_font(rl, FONT_SM, 0);
        }

        lv_obj_t * act = lv_obj_create(row);
        lv_obj_set_flex_grow(act, 1);
        lv_obj_set_height(act, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(act, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(act, 0, 0);
        lv_obj_set_style_pad_all(act, 0, 0);
        lv_obj_set_flex_flow(act, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(act, 2, 0);
        lv_obj_clear_flag(act, LV_OBJ_FLAG_SCROLLABLE);

        if(sum.item_count > 0) {
            lv_obj_t * lb = lv_btn_create(act);
            lv_obj_set_size(lb, 28, 24);
            lv_obj_t * ll = lv_label_create(lb);
            lv_label_set_text(ll, LV_SYMBOL_LIST);
            lv_obj_set_style_text_font(ll, FONT_SM, 0);
            lv_obj_center(ll);
            lv_obj_add_event_cb(lb, list_items_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            ui_touch_expand(lb, 8);
        }

        bool sent = app_is_entry_sent(sum.id, app_today_date());
        if(strcmp(sum.status, "SENT") == 0) {
            lv_obj_t * em = lv_label_create(act);
            lv_label_set_text(em, "Emailed");
            lv_obj_set_style_text_color(em, lv_color_hex(COL_CYAN), 0);
            lv_obj_set_style_text_font(em, FONT_SM, 0);
        }
        else if(strcmp(sum.status, "-") == 0 && !dash_is_sent()) {
            lv_obj_t * ab = lv_btn_create(act);
            lv_obj_set_size(ab, 44, 24);
            lv_obj_t * al = lv_label_create(ab);
            lv_label_set_text(al, "+ADD");
            lv_obj_set_style_text_color(al, lv_color_hex(COL_GREEN), 0);
            lv_obj_set_style_text_font(al, FONT_SM, 0);
            lv_obj_center(al);
            lv_obj_add_event_cb(ab, go_sales_add_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            ui_touch_expand(ab, 8);
        }
        else if(strcmp(sum.status, "OPEN") == 0 || strcmp(sum.status, "CLOSED") == 0) {
            lv_obj_t * eb = lv_btn_create(act);
            lv_obj_set_size(eb, 44, 24);
            lv_obj_t * el = lv_label_create(eb);
            lv_label_set_text(el, sent ? "View" : "Edit");
            lv_obj_set_style_text_color(el, lv_color_hex(COL_CYAN), 0);
            lv_obj_set_style_text_font(el, FONT_SM, 0);
            lv_obj_center(el);
            lv_obj_add_event_cb(eb, go_sales_edit_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            ui_touch_expand(eb, 8);
        }
    }
}

void ui_dashboard_destroy(void)
{
    ui_keypad_release();
    if(s.root && lv_obj_is_valid(s.root)) {
        lv_obj_del(s.root);
        s.root = NULL;
    }
    memset(&s, 0, sizeof(s));
}

void ui_dashboard_reload(void)
{
    if(!s.root || !lv_obj_is_valid(s.root)) return;
    if(dash_is_sent()) lv_obj_clear_flag(s.sent_badge, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s.sent_badge, LV_OBJ_FLAG_HIDDEN);
    rebuild_table();
    rebuild_recon_dropdown();
}

static void create_recon_section(lv_obj_t * parent)
{
    lv_obj_t * recon_sec = lv_obj_create(parent);
    lv_obj_set_width(recon_sec, LV_PCT(100));
    lv_obj_set_height(recon_sec, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(recon_sec, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(recon_sec, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(recon_sec, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(recon_sec, 1, 0);
    lv_obj_set_style_pad_top(recon_sec, 4, 0);
    lv_obj_set_flex_flow(recon_sec, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(recon_sec, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * rt = lv_label_create(recon_sec);
    lv_label_set_text(rt, "CASH RECONCILIATION");
    lv_obj_set_style_text_color(rt, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(rt, FONT_SM, 0);

    lv_obj_t * recon_row = lv_obj_create(recon_sec);
    lv_obj_set_width(recon_row, LV_PCT(100));
    lv_obj_set_height(recon_row, 26);
    lv_obj_set_style_bg_opa(recon_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(recon_row, 0, 0);
    lv_obj_set_style_pad_all(recon_row, 0, 0);
    lv_obj_set_flex_flow(recon_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(recon_row, 4, 0);
    lv_obj_clear_flag(recon_row, LV_OBJ_FLAG_SCROLLABLE);

    s.recon_dd = lv_dropdown_create(recon_row);
    lv_obj_set_flex_grow(s.recon_dd, 1);
    lv_obj_set_height(s.recon_dd, 24);
    lv_dropdown_set_options(s.recon_dd, "Select...");
    style_recon_dd(s.recon_dd);
    lv_obj_add_event_cb(s.recon_dd, recon_dd_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * cash_btn = lv_btn_create(recon_row);
    lv_obj_set_size(cash_btn, 64, 32);
    lv_obj_set_style_bg_color(cash_btn, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(cash_btn, lv_color_hex(0x374151), 0);
    lv_obj_set_style_border_width(cash_btn, 1, 0);
    lv_obj_add_event_cb(cash_btn, cash_toggle_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * cbl = lv_label_create(cash_btn);
    lv_label_set_text(cbl, "Cash");
    lv_obj_set_style_text_color(cbl, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_text_font(cbl, FONT_SM, 0);
    lv_obj_center(cbl);
    ui_touch_expand(cash_btn, 12);
    ui_touch_expand(s.recon_dd, 8);
}

static void create_items_overlay(void)
{
    s.items_overlay = lv_obj_create(s.root);
    lv_obj_set_size(s.items_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s.items_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s.items_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s.items_overlay, 0, 0);
    lv_obj_add_flag(s.items_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_flex_flow(s.items_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s.items_overlay, 8, 0);

    lv_obj_t * ihdr_row = lv_obj_create(s.items_overlay);
    lv_obj_set_width(ihdr_row, LV_PCT(100));
    lv_obj_set_height(ihdr_row, 20);
    lv_obj_set_style_bg_opa(ihdr_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ihdr_row, 0, 0);
    lv_obj_set_flex_flow(ihdr_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ihdr_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ihdr_row, LV_OBJ_FLAG_SCROLLABLE);

    s.items_title = lv_label_create(ihdr_row);
    lv_label_set_text(s.items_title, "ITEMS");
    lv_obj_set_style_text_color(s.items_title, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(s.items_title, FONT_SM, 0);

    lv_obj_t * iclose = lv_btn_create(ihdr_row);
    lv_obj_set_size(iclose, 24, 20);
    lv_obj_t * ixl = lv_label_create(iclose);
    lv_label_set_text(ixl, LV_SYMBOL_CLOSE);
    lv_obj_center(ixl);
    lv_obj_add_event_cb(iclose, close_items_cb, LV_EVENT_CLICKED, NULL);

    s.items_list = lv_obj_create(s.items_overlay);
    lv_obj_set_width(s.items_list, LV_PCT(100));
    lv_obj_set_flex_grow(s.items_list, 1);
    lv_obj_set_style_bg_color(s.items_list, lv_color_hex(0x111827), 0);
    lv_obj_set_style_border_color(s.items_list, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.items_list, 1, 0);
    lv_obj_set_style_radius(s.items_list, 6, 0);
    lv_obj_set_flex_flow(s.items_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.items_list, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_dashboard_create(lv_obj_t * parent, void (*on_go_sales)(const char * employee_id))
{
    ui_dashboard_destroy();
    s.on_go_sales = on_go_sales;
    s.show_keypad = false;

    s.root = lv_obj_create(parent);
    lv_obj_set_size(s.root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s.root, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(s.root, 0, 0);
    lv_obj_set_style_pad_all(s.root, 0, 0);
#if APP_PORTRAIT
    lv_obj_set_flex_flow(s.root, LV_FLEX_FLOW_COLUMN);
#else
    lv_obj_set_flex_flow(s.root, LV_FLEX_FLOW_ROW);
#endif
    lv_obj_clear_flag(s.root, LV_OBJ_FLAG_SCROLLABLE);

    s.left = lv_obj_create(s.root);
#if APP_PORTRAIT
    lv_obj_set_width(s.left, LV_PCT(100));
    lv_obj_set_flex_grow(s.left, 1);
    lv_obj_set_style_min_height(s.left, 0, 0);
#else
    lv_obj_set_height(s.left, LV_PCT(100));
    lv_obj_set_width(s.left, APP_SCREEN_W);
#endif
    lv_obj_set_style_bg_opa(s.left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.left, 0, 0);
    lv_obj_set_style_pad_all(s.left, 6, 0);
    lv_obj_set_flex_flow(s.left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s.left, 4, 0);
    lv_obj_clear_flag(s.left, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title_row = lv_obj_create(s.left);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 18);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * title = lv_label_create(title_row);
    lv_label_set_text_fmt(title, "DAILY SUMMARY - %s", app_today_date());
    lv_obj_set_style_text_color(title, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(title, FONT_SM, 0);
    s.sent_badge = lv_label_create(title_row);
    lv_label_set_text(s.sent_badge, "SENT");
    lv_obj_set_style_text_color(s.sent_badge, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(s.sent_badge, FONT_SM, 0);
    lv_obj_add_flag(s.sent_badge, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * hdr = lv_obj_create(s.left);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_obj_set_height(hdr, 16);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x111827), 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_hor(hdr, 4, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    const char * cols[] = {"Name", "Sales", "Itm", "Stat", "Rec", "Act"};
    const int cw[] = {COL_NAME_W, COL_SALES_W, COL_ITM_W, COL_STAT_W, COL_REC_W, 40};
    for(int i = 0; i < 6; i++) {
        lv_obj_t * c = lv_label_create(hdr);
        lv_obj_set_width(c, cw[i]);
        lv_label_set_text(c, cols[i]);
        lv_obj_set_style_text_color(c, lv_color_hex(COL_GRAY), 0);
        lv_obj_set_style_text_font(c, FONT_SM, 0);
    }

    s.table_body = lv_obj_create(s.left);
    lv_obj_set_width(s.table_body, LV_PCT(100));
    lv_obj_set_flex_grow(s.table_body, 1);
    lv_obj_set_style_min_height(s.table_body, 0, 0);
    lv_obj_set_style_bg_color(s.table_body, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s.table_body, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.table_body, 1, 0);
    lv_obj_set_style_radius(s.table_body, 6, 0);
    lv_obj_set_style_pad_all(s.table_body, 2, 0);
    lv_obj_set_flex_flow(s.table_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.table_body, LV_OBJ_FLAG_SCROLLABLE);

#if APP_PORTRAIT
    create_recon_section(s.left);
#else
    create_recon_section(s.left);
#endif

#if APP_PORTRAIT
    s.kp_overlay = lv_obj_create(s.root);
    lv_obj_set_width(s.kp_overlay, LV_PCT(100));
    lv_obj_set_height(s.kp_overlay, PORTR_RECON_DOCK_H);
    lv_obj_set_flex_grow(s.kp_overlay, 0);
    lv_obj_set_style_bg_color(s.kp_overlay, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(s.kp_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s.kp_overlay, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(s.kp_overlay, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.kp_overlay, 1, 0);
    lv_obj_set_style_pad_all(s.kp_overlay, 4, 0);
    lv_obj_set_style_pad_row(s.kp_overlay, 4, 0);
    lv_obj_set_flex_flow(s.kp_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.kp_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s.kp_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * kp_panel = lv_obj_create(s.kp_overlay);
    lv_obj_set_width(kp_panel, LV_PCT(100));
    lv_obj_set_flex_grow(kp_panel, 1);
    lv_obj_set_style_bg_opa(kp_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kp_panel, 0, 0);
    lv_obj_set_style_pad_all(kp_panel, 0, 0);
    lv_obj_clear_flag(kp_panel, LV_OBJ_FLAG_SCROLLABLE);

    s.keypad = ui_keypad_create(kp_panel, s.cash_input, sizeof(s.cash_input),
                                true, 10, "Actual Cash", true, true);

    lv_obj_t * rec_btn = lv_btn_create(s.kp_overlay);
    lv_obj_set_width(rec_btn, LV_PCT(100));
    lv_obj_set_height(rec_btn, PORTR_RECON_BTN_H);
    lv_obj_set_flex_grow(rec_btn, 0);
    lv_obj_set_style_bg_color(rec_btn, lv_color_hex(0x0E7490), 0);
    lv_obj_add_event_cb(rec_btn, reconcile_cb, LV_EVENT_CLICKED, NULL);
    ui_touch_expand(rec_btn, 8);
    lv_obj_t * rl = lv_label_create(rec_btn);
    lv_label_set_text(rl, "RECONCILE");
    lv_obj_set_style_text_font(rl, &lv_font_montserrat_12, 0);
    lv_obj_center(rl);
#else
    s.right = lv_obj_create(s.root);
    lv_obj_set_size(s.right, KEYPAD_W, LV_PCT(100));
    lv_obj_set_style_bg_color(s.right, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_side(s.right, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(s.right, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.right, 1, 0);
    lv_obj_set_style_pad_all(s.right, 6, 0);
    lv_obj_set_flex_flow(s.right, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.right, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s.right, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * kp_wrap = lv_obj_create(s.right);
    lv_obj_set_width(kp_wrap, LV_PCT(100));
    lv_obj_set_flex_grow(kp_wrap, 1);
    lv_obj_set_style_bg_opa(kp_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kp_wrap, 0, 0);
    lv_obj_clear_flag(kp_wrap, LV_OBJ_FLAG_SCROLLABLE);

    s.keypad = ui_keypad_create(kp_wrap, s.cash_input, sizeof(s.cash_input), true, 10, "Actual Cash", true, true);

    lv_obj_t * rec_btn = lv_btn_create(s.right);
    lv_obj_set_width(rec_btn, LV_PCT(100));
    lv_obj_set_height(rec_btn, 28);
    lv_obj_set_style_bg_color(rec_btn, lv_color_hex(0x0E7490), 0);
    lv_obj_add_event_cb(rec_btn, reconcile_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * rl = lv_label_create(rec_btn);
    lv_label_set_text(rl, "RECONCILE");
    lv_obj_set_style_text_font(rl, &lv_font_montserrat_12, 0);
    lv_obj_center(rl);
#endif

    create_items_overlay();
    ui_dashboard_reload();
}
