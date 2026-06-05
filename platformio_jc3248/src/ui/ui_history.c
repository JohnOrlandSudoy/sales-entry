#include "ui_history.h"
#include "ui_colors.h"
#include "ui_master_pin.h"
#include "app_data.h"
#include "app_display.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if LV_FONT_MONTSERRAT_10
  #define FONT_SM  (&lv_font_montserrat_10)
#else
  #define FONT_SM  (&lv_font_montserrat_12)
#endif

#define CONTENT_H  (APP_SCREEN_H - APP_HDR_H)

typedef struct {
    lv_obj_t * root;
    lv_obj_t * monday_bar;
    lv_obj_t * invoice_panel;
    lv_obj_t * week_list;
    void (*on_back)(void *);
    void * user_data;
    char expanded_week[APP_DATE_LEN];
    char clear_week_key[APP_DATE_LEN];
} hist_ui_t;

static hist_ui_t s;

static void format_week_label(const char * week_key, char * out, size_t sz)
{
    if(week_key && strlen(week_key) >= 10)
        snprintf(out, sz, "Week %s", week_key + 5);
    else if(week_key)
        snprintf(out, sz, "Week %s", week_key);
    else
        snprintf(out, sz, "Week");
}

static void style_row_container(lv_obj_t * obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static const char * badge_text(app_inv_badge_t b)
{
    switch(b) {
    case APP_INV_BADGE_SENT: return "SENT";
    case APP_INV_BADGE_OPEN: return "OPEN";
    case APP_INV_BADGE_AWAIT_RECON: return "AWAIT";
    case APP_INV_BADGE_PENDING: return "PEND";
    default: return NULL;
    }
}

static uint32_t badge_color(app_inv_badge_t b)
{
    switch(b) {
    case APP_INV_BADGE_SENT: return 0x16A34A;
    case APP_INV_BADGE_OPEN: return 0xEA580C;
    case APP_INV_BADGE_AWAIT_RECON: return 0xDC2626;
    case APP_INV_BADGE_PENDING: return 0xD97706;
    default: return COL_GRAY;
    }
}

static void back_cb(lv_event_t * e)
{
    (void)e;
    if(s.on_back) s.on_back(s.user_data);
}

static void clear_pin_ok(void * ud)
{
    (void)ud;
    if(s.clear_week_key[0]) app_clear_week(s.clear_week_key);
    s.clear_week_key[0] = '\0';
    ui_history_reload();
}

static void clear_week_cb(lv_event_t * e)
{
    lv_event_stop_bubbling(e);
    int w = (int)(intptr_t)lv_event_get_user_data(e);
    app_week_summary_t sum;
    app_week_summary_get(w, &sum);
    if(!sum.week_key[0]) return;
    strncpy(s.clear_week_key, sum.week_key, sizeof(s.clear_week_key) - 1);
    s.clear_week_key[sizeof(s.clear_week_key) - 1] = '\0';
    lv_obj_t * scr = lv_obj_get_screen(s.root);
    ui_master_pin_show(scr, "MASTER PIN TO CLEAR WEEK", clear_pin_ok, NULL, NULL);
}

static void clear_last_week_cb(lv_event_t * e)
{
    (void)e;
    if(app_week_summary_count() < 2) return;
    app_week_summary_t wk;
    app_week_summary_get(1, &wk);
    strncpy(s.clear_week_key, wk.week_key, sizeof(s.clear_week_key) - 1);
    s.clear_week_key[sizeof(s.clear_week_key) - 1] = '\0';
    lv_obj_t * scr = lv_obj_get_screen(s.root);
    ui_master_pin_show(scr, "MASTER PIN TO CLEAR WEEK", clear_pin_ok, NULL, NULL);
}

static void week_header_cb(lv_event_t * e)
{
    int w = (int)(intptr_t)lv_event_get_user_data(e);
    app_week_summary_t sum;
    app_week_summary_get(w, &sum);
    if(!sum.week_key[0]) return;
    if(s.expanded_week[0] && strcmp(s.expanded_week, sum.week_key) == 0)
        s.expanded_week[0] = '\0';
    else {
        strncpy(s.expanded_week, sum.week_key, sizeof(s.expanded_week) - 1);
        s.expanded_week[sizeof(s.expanded_week) - 1] = '\0';
    }
    ui_history_reload();
}

static lv_obj_t * make_badge(lv_obj_t * parent, const char * txt, uint32_t color)
{
    if(!txt) return NULL;
    lv_obj_t * b = lv_obj_create(parent);
    lv_obj_set_size(b, LV_SIZE_CONTENT, 14);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_60, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 3, 0);
    lv_obj_set_style_pad_hor(b, 3, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, FONT_SM, 0);
    lv_obj_center(l);
    return b;
}

static void refresh_invoice_panel(void)
{
    if(!s.invoice_panel) return;
    lv_obj_clean(s.invoice_panel);
    int dn = app_invoice_date_count();
    if(dn == 0) {
        lv_obj_add_flag(s.invoice_panel, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s.invoice_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * head = lv_obj_create(s.invoice_panel);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, 14);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(head, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(head, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(head, 1, 0);
    lv_obj_set_style_pad_hor(head, 4, 0);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * ht = lv_label_create(head);
    lv_label_set_text(ht, "INVOICE STATUS");
    lv_obj_set_style_text_color(ht, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(ht, FONT_SM, 0);

    lv_obj_t * chips = lv_obj_create(s.invoice_panel);
    lv_obj_set_width(chips, LV_PCT(100));
    lv_obj_set_height(chips, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(chips, 56, 0);
    style_row_container(chips);
    lv_obj_set_style_pad_all(chips, 4, 0);
    lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(chips, 2, 0);
    lv_obj_add_flag(chips, LV_OBJ_FLAG_SCROLLABLE);

    for(int i = 0; i < dn; i++) {
        const char * dt = app_invoice_date_at(i);
        if(!dt) continue;
        char st[16];
        app_invoice_date_status_label(dt, st, sizeof(st));
        bool sent = (strcmp(st, "SENT") == 0);
        char chip[40];
        snprintf(chip, sizeof(chip), "%s  %s", dt, st);
        lv_obj_t * c = lv_label_create(chips);
        lv_label_set_text(c, chip);
        lv_obj_set_width(c, LV_PCT(100));
        lv_obj_set_style_text_font(c, FONT_SM, 0);
        lv_obj_set_style_pad_all(c, 2, 0);
        lv_obj_set_style_radius(c, 3, 0);
        if(sent) {
            lv_obj_set_style_text_color(c, lv_color_hex(COL_GREEN), 0);
            lv_obj_set_style_bg_color(c, lv_color_hex(0x14532D), 0);
        }
        else if(strcmp(st, "PARTIAL") == 0) {
            lv_obj_set_style_text_color(c, lv_color_hex(COL_ORANGE), 0);
            lv_obj_set_style_bg_color(c, lv_color_hex(0x78350F), 0);
        }
        else {
            lv_obj_set_style_text_color(c, lv_color_hex(COL_GRAY_TEXT), 0);
            lv_obj_set_style_bg_color(c, lv_color_hex(COL_PANEL2), 0);
        }
        lv_obj_set_style_bg_opa(c, LV_OPA_50, 0);
    }
}

static void refresh_week_list(void)
{
    if(!s.week_list) return;
    lv_obj_clean(s.week_list);

    int wn = app_week_summary_count();
    if(wn == 0) {
        lv_obj_t * empty = lv_label_create(s.week_list);
        lv_label_set_text(empty, "No sales history");
        lv_obj_set_style_text_color(empty, lv_color_hex(COL_GRAY), 0);
        lv_obj_set_style_text_font(empty, FONT_SM, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty, LV_PCT(100));
        return;
    }

    for(int w = 0; w < wn; w++) {
        app_week_summary_t sum;
        app_week_summary_get(w, &sum);
        bool expanded = (s.expanded_week[0] && strcmp(s.expanded_week, sum.week_key) == 0);

        lv_obj_t * card = lv_obj_create(s.week_list);
        lv_obj_set_width(card, LV_PCT(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x111827), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_50, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(COL_BORDER), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 4, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        char wk_lbl[24];
        format_week_label(sum.week_key, wk_lbl, sizeof(wk_lbl));
        char buf[32];

        lv_obj_t * hdr = lv_btn_create(card);
        lv_obj_set_width(hdr, LV_PCT(100));
        lv_obj_set_height(hdr, 26);
        lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(hdr, 0, 0);
        lv_obj_set_style_radius(hdr, 0, 0);
        lv_obj_set_style_pad_all(hdr, 4, 0);
        lv_obj_set_style_pad_column(hdr, 4, 0);
        lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(hdr, week_header_cb, LV_EVENT_CLICKED, (void *)(intptr_t)w);

        lv_obj_t * left = lv_obj_create(hdr);
        lv_obj_set_flex_grow(left, 1);
        lv_obj_set_height(left, LV_SIZE_CONTENT);
        style_row_container(left);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(left, 4, 0);
        lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t * chev = lv_label_create(left);
        lv_label_set_text(chev, expanded ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(chev, lv_color_hex(COL_GRAY), 0);
        lv_obj_set_style_text_font(chev, FONT_SM, 0);

        lv_obj_t * wl = lv_label_create(left);
        lv_label_set_text(wl, wk_lbl);
        lv_obj_set_style_text_color(wl, lv_color_hex(COL_CYAN), 0);
        lv_obj_set_style_text_font(wl, FONT_SM, 0);
        lv_label_set_long_mode(wl, LV_LABEL_LONG_CLIP);
        lv_obj_set_flex_grow(wl, 1);

        lv_obj_t * right = lv_obj_create(hdr);
        lv_obj_set_width(right, APP_SCREEN_W < 400 ? 92 : 108);
        lv_obj_set_height(right, LV_SIZE_CONTENT);
        style_row_container(right);
        lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(right, 4, 0);
        lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        snprintf(buf, sizeof(buf), "P%.0f", sum.total);
        lv_obj_t * tot = lv_label_create(right);
        lv_label_set_text(tot, buf);
        lv_obj_set_style_text_color(tot, lv_color_hex(COL_YELLOW), 0);
        lv_obj_set_style_text_font(tot, FONT_SM, 0);

        snprintf(buf, sizeof(buf), "%d", sum.entry_count);
        lv_obj_t * ec = lv_label_create(right);
        lv_label_set_text(ec, buf);
        lv_obj_set_style_text_color(ec, lv_color_hex(COL_GRAY_TEXT), 0);
        lv_obj_set_style_text_font(ec, FONT_SM, 0);

        lv_obj_t * trash = lv_btn_create(right);
        lv_obj_set_size(trash, 22, 20);
        lv_obj_set_style_bg_opa(trash, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(trash, 0, 0);
        lv_obj_set_style_pad_all(trash, 0, 0);
        lv_obj_add_event_cb(trash, clear_week_cb, LV_EVENT_CLICKED, (void *)(intptr_t)w);
        lv_obj_t * tl = lv_label_create(trash);
        lv_label_set_text(tl, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(tl, lv_color_hex(COL_RED), 0);
        lv_obj_set_style_text_font(tl, FONT_SM, 0);
        lv_obj_center(tl);

        if(expanded) {
            lv_obj_t * body = lv_obj_create(card);
            lv_obj_set_width(body, LV_PCT(100));
            lv_obj_set_height(body, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_side(body, LV_BORDER_SIDE_TOP, 0);
            lv_obj_set_style_border_color(body, lv_color_hex(0x1F2937), 0);
            lv_obj_set_style_border_width(body, 1, 0);
            lv_obj_set_style_pad_all(body, 4, 0);
            lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
            lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

            int en = app_week_entry_count(sum.week_key);
            for(int ei = 0; ei < en; ei++) {
                const app_sales_entry_t * entry = app_week_entry_at(sum.week_key, ei);
                if(!entry) continue;

                lv_obj_t * row = lv_obj_create(body);
                lv_obj_set_width(row, LV_PCT(100));
                lv_obj_set_height(row, LV_SIZE_CONTENT);
                lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
                lv_obj_set_style_border_color(row, lv_color_hex(0x1F2937), 0);
                lv_obj_set_style_border_width(row, 1, 0);
                lv_obj_set_style_pad_all(row, 4, 0);
                lv_obj_set_style_pad_row(row, 3, 0);
                style_row_container(row);
                lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);

                lv_obj_t * top = lv_obj_create(row);
                lv_obj_set_width(top, LV_PCT(100));
                lv_obj_set_height(top, LV_SIZE_CONTENT);
                style_row_container(top);
                lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

                char line[48];
                snprintf(line, sizeof(line), "%s  (%d itm)  %s",
                         entry->employee_name, entry->item_count, entry->date);
                lv_obj_t * nm = lv_label_create(top);
                lv_label_set_text(nm, line);
                lv_obj_set_width(nm, LV_PCT(68));
                lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
                lv_obj_set_style_text_color(nm, lv_color_hex(COL_GREEN), 0);
                lv_obj_set_style_text_font(nm, FONT_SM, 0);

                snprintf(buf, sizeof(buf), "P%.2f", entry->grand_total);
                lv_obj_t * gt = lv_label_create(top);
                lv_label_set_text(gt, buf);
                lv_obj_set_style_text_color(gt, lv_color_hex(COL_YELLOW), 0);
                lv_obj_set_style_text_font(gt, FONT_SM, 0);
                lv_obj_set_style_text_align(gt, LV_TEXT_ALIGN_RIGHT, 0);

                if(entry->item_count > 0) {
                    lv_obj_t * bc_lbl = lv_label_create(row);
                    lv_label_set_text(bc_lbl, entry->items[0].barcode);
                    lv_obj_set_width(bc_lbl, LV_PCT(100));
                    lv_label_set_long_mode(bc_lbl, LV_LABEL_LONG_DOT);
                    lv_obj_set_style_text_color(bc_lbl, lv_color_hex(COL_CYAN), 0);
                    lv_obj_set_style_text_font(bc_lbl, FONT_SM, 0);
                }

                lv_obj_t * badges = lv_obj_create(row);
                lv_obj_set_width(badges, LV_PCT(100));
                lv_obj_set_height(badges, LV_SIZE_CONTENT);
                style_row_container(badges);
                lv_obj_set_flex_flow(badges, LV_FLEX_FLOW_ROW);
                lv_obj_set_style_pad_column(badges, 4, 0);
                lv_obj_set_flex_align(badges, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

                app_inv_badge_t inv = app_entry_invoice_badge(entry);
                const char * bt = badge_text(inv);
                if(bt) make_badge(badges, bt, badge_color(inv));

                make_badge(badges, entry->closed ? "CLOSED" : "OPEN",
                           entry->closed ? 0x16A34A : 0xEA580C);
            }
        }
    }
}

void ui_history_destroy(void)
{
    if(s.root) {
        lv_obj_del(s.root);
        s.root = NULL;
    }
    memset(&s, 0, sizeof(s));
}

void ui_history_reload(void)
{
    if(!s.root) return;
    if(s.monday_bar) {
        if(app_is_monday_today() && app_week_summary_count() > 1)
            lv_obj_clear_flag(s.monday_bar, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s.monday_bar, LV_OBJ_FLAG_HIDDEN);
    }
    refresh_invoice_panel();
    refresh_week_list();
}

void ui_history_create(lv_obj_t * parent, void (*on_back)(void *), void * user_data)
{
    ui_history_destroy();
    s.on_back = on_back;
    s.user_data = user_data;
    s.expanded_week[0] = '\0';
    s.clear_week_key[0] = '\0';

    s.root = lv_obj_create(parent);
    lv_obj_set_size(s.root, APP_SCREEN_W, CONTENT_H);
    lv_obj_set_style_bg_color(s.root, lv_color_hex(0x030712), 0);
    lv_obj_set_style_border_width(s.root, 0, 0);
    lv_obj_set_style_pad_all(s.root, 0, 0);
    lv_obj_set_flex_flow(s.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.root, LV_OBJ_FLAG_SCROLLABLE);

    s.monday_bar = lv_obj_create(s.root);
    lv_obj_set_width(s.monday_bar, LV_PCT(100));
    lv_obj_set_height(s.monday_bar, 22);
    lv_obj_set_style_bg_color(s.monday_bar, lv_color_hex(0x7C2D12), 0);
    lv_obj_set_style_bg_opa(s.monday_bar, LV_OPA_40, 0);
    lv_obj_set_style_border_side(s.monday_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(s.monday_bar, lv_color_hex(0x9A3412), 0);
    lv_obj_set_style_border_width(s.monday_bar, 1, 0);
    lv_obj_set_style_pad_hor(s.monday_bar, 6, 0);
    lv_obj_set_flex_flow(s.monday_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s.monday_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s.monday_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s.monday_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * mt = lv_label_create(s.monday_bar);
    lv_label_set_text(mt, "Monday - clear last week?");
    lv_obj_set_style_text_color(mt, lv_color_hex(COL_ORANGE), 0);
    lv_obj_set_style_text_font(mt, FONT_SM, 0);
    lv_obj_set_width(mt, LV_PCT(65));

    lv_obj_t * mbtn = lv_btn_create(s.monday_bar);
    lv_obj_set_size(mbtn, LV_SIZE_CONTENT, 16);
    lv_obj_set_style_bg_color(mbtn, lv_color_hex(0x9A3412), 0);
    lv_obj_add_event_cb(mbtn, clear_last_week_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * mbl = lv_label_create(mbtn);
    lv_label_set_text(mbl, "Clear Last");
    lv_obj_set_style_text_font(mbl, FONT_SM, 0);
    lv_obj_center(mbl);

    lv_obj_t * head = lv_obj_create(s.root);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, 18);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_pad_hor(head, 6, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * ht = lv_label_create(head);
    lv_label_set_text(ht, "WEEKLY HISTORY");
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

    s.invoice_panel = lv_obj_create(s.root);
    lv_obj_set_width(s.invoice_panel, LV_PCT(100));
    lv_obj_set_height(s.invoice_panel, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s.invoice_panel, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(s.invoice_panel, LV_OPA_60, 0);
    lv_obj_set_style_border_color(s.invoice_panel, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.invoice_panel, 1, 0);
    lv_obj_set_style_radius(s.invoice_panel, 4, 0);
    lv_obj_set_style_pad_all(s.invoice_panel, 0, 0);
    lv_obj_set_style_pad_hor(s.invoice_panel, 4, 0);
    lv_obj_set_flex_flow(s.invoice_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.invoice_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s.invoice_panel, LV_OBJ_FLAG_SCROLLABLE);

    s.week_list = lv_obj_create(s.root);
    lv_obj_set_width(s.week_list, LV_PCT(100));
    lv_obj_set_flex_grow(s.week_list, 1);
    lv_obj_set_style_bg_opa(s.week_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.week_list, 0, 0);
    lv_obj_set_style_pad_all(s.week_list, 4, 0);
    lv_obj_set_style_pad_row(s.week_list, 4, 0);
    lv_obj_set_flex_flow(s.week_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s.week_list, LV_OBJ_FLAG_SCROLLABLE);

    ui_history_reload();
}
