#include "ui_email.h"
#include "ui_colors.h"
#include "app_data.h"
#include "app_mail_task.h"
#include "app_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef APP_MAIL_ON_DEVICE
#if defined(ARDUINO) || defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
#define APP_MAIL_ON_DEVICE  1
#else
#define APP_MAIL_ON_DEVICE  0
#endif
#endif

#if APP_MAIL_ON_DEVICE
#include <esp_task_wdt.h>
#endif

#define EMAIL_LOG(fmt, ...) printf("[email-ui] " fmt "\n", ##__VA_ARGS__)

#if LV_FONT_MONTSERRAT_10
  #define FONT_SM  (&lv_font_montserrat_10)
#else
  #define FONT_SM  (&lv_font_montserrat_12)
#endif

#define MAX_ROWS  96

typedef struct {
    const app_sales_entry_t * entry;
    const app_sales_item_t * item;
    bool is_last_in_sale;
    bool is_last_for_salesman;
    int salesman_total_qty;
} inv_row_t;

typedef struct {
    lv_obj_t * overlay;
    lv_obj_t * table_body;
    lv_obj_t * empty_panel;
    lv_obj_t * table_wrap;
    lv_obj_t * send_btn;
    lv_obj_t * send_btn_lbl;
    lv_obj_t * title_lbl;
    ui_email_sent_cb_t on_sent;
    ui_email_cb_t on_close;
    void * user_data;
    inv_row_t rows[MAX_ROWS];
    int row_count;
} email_ui_t;

static email_ui_t s;

#if APP_MAIL_ON_DEVICE
static ui_email_sent_cb_t s_mail_sent_cb;
static void * s_mail_ud;
static char s_mail_msg[128];
static int s_mail_batch;
static volatile bool s_mail_busy;
static void mail_send_done_async(void * ud);
static void mail_task_finished(int batch, const char * msg, void * ud);
static void mail_kick_async(void * ud);
static void mail_fail_on_ui(const char * reason);
#endif

/** Full-screen overlays on LVGL top layer so they stay above Sales/Dashboard. */
static lv_obj_t * email_overlay_parent(void)
{
    lv_obj_t * top = lv_layer_top();
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    return top;
}

static void screen_size(lv_obj_t * parent, lv_coord_t * w, lv_coord_t * h)
{
    lv_disp_t * disp = parent ? lv_obj_get_disp(parent) : NULL;
    if(!disp) disp = lv_disp_get_default();
    if(disp) {
        *w = lv_disp_get_hor_res(disp);
        *h = lv_disp_get_ver_res(disp);
    }
    else {
        *w = lv_obj_get_width(parent);
        *h = lv_obj_get_height(parent);
    }
    if(*w <= 0) *w = APP_SCREEN_W;
    if(*h <= 0) *h = APP_SCREEN_H;
}

static int cmp_rows(const void * a, const void * b)
{
    const inv_row_t * ra = a;
    const inv_row_t * rb = b;
    int n = strcmp(ra->entry->employee_name, rb->entry->employee_name);
    if(n != 0) return n;
    return strcmp(ra->entry->date, rb->entry->date);
}

static void build_rows(void)
{
    s.row_count = 0;
    int pending = app_pending_email_count();
    for(int p = 0; p < pending && s.row_count < MAX_ROWS; p++) {
        const app_sales_entry_t * entry = app_pending_email_entry(p);
        if(!entry) continue;
        for(int i = 0; i < entry->item_count && s.row_count < MAX_ROWS; i++) {
            inv_row_t * r = &s.rows[s.row_count++];
            r->entry = entry;
            r->item = &entry->items[i];
            r->is_last_in_sale = (i == entry->item_count - 1);
            r->is_last_for_salesman = false;
            r->salesman_total_qty = 0;
        }
    }
    if(s.row_count == 0) return;

    qsort(s.rows, (size_t)s.row_count, sizeof(inv_row_t), cmp_rows);

    int qty_by_name[APP_MAX_EMPLOYEES];
    const char * names[APP_MAX_EMPLOYEES];
    int name_n = 0;
    memset(qty_by_name, 0, sizeof(qty_by_name));

    for(int i = 0; i < s.row_count; i++) {
        const char * name = s.rows[i].entry->employee_name;
        int idx = -1;
        for(int j = 0; j < name_n; j++) {
            if(strcmp(names[j], name) == 0) { idx = j; break; }
        }
        if(idx < 0 && name_n < APP_MAX_EMPLOYEES) {
            idx = name_n;
            names[name_n++] = name;
        }
        if(idx >= 0) qty_by_name[idx] += s.rows[i].item->quantity;
    }

    for(int i = 0; i < s.row_count; i++) {
        const char * name = s.rows[i].entry->employee_name;
        int idx = -1;
        for(int j = 0; j < name_n; j++) {
            if(strcmp(names[j], name) == 0) { idx = j; break; }
        }
        if(idx >= 0) s.rows[i].salesman_total_qty = qty_by_name[idx];
        bool last = true;
        for(int j = i + 1; j < s.row_count; j++) {
            if(strcmp(s.rows[j].entry->employee_name, name) == 0) {
                last = false;
                break;
            }
        }
        s.rows[i].is_last_for_salesman = last;
    }
}

static void add_cell(lv_obj_t * row, const char * txt, lv_color_t color, bool right)
{
    lv_obj_t * c = lv_label_create(row);
    lv_label_set_text(c, txt);
    lv_obj_set_style_text_color(c, color, 0);
    lv_obj_set_style_text_font(c, FONT_SM, 0);
    if(right) lv_obj_set_style_text_align(c, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_flex_grow(c, 1);
}

static void refresh_table(void)
{
    if(!s.table_body) return;
    lv_obj_clean(s.table_body);
    build_rows();

    bool has_pending = s.row_count > 0;
    if(s.empty_panel) {
        if(has_pending) lv_obj_add_flag(s.empty_panel, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(s.empty_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if(s.table_wrap) {
        if(has_pending) lv_obj_clear_flag(s.table_wrap, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s.table_wrap, LV_OBJ_FLAG_HIDDEN);
    }
    if(s.send_btn) {
        if(has_pending) lv_obj_clear_state(s.send_btn, LV_STATE_DISABLED);
        else lv_obj_add_state(s.send_btn, LV_STATE_DISABLED);
    }

    if(!has_pending) {
        if(s.empty_panel) {
            lv_obj_t * main_lbl = lv_obj_get_child(s.empty_panel, 0);
            lv_obj_t * sub_lbl = lv_obj_get_child(s.empty_panel, 1);
            if(main_lbl) {
                lv_label_set_text(main_lbl,
                    app_has_unsent_sales_work() ? "Nothing ready to send" : "All invoices sent");
            }
            if(sub_lbl) {
                lv_label_set_text(sub_lbl,
                    app_has_unsent_sales_work()
                        ? "Close OPEN sales on Dash,\nthen complete cash reconciliation"
                        : "View sent status in History tab");
            }
        }
        return;
    }

    for(int i = 0; i < s.row_count; i++) {
        const inv_row_t * r = &s.rows[i];
        char buf[32];

        lv_obj_t * tr = lv_obj_create(s.table_body);
        lv_obj_set_width(tr, LV_PCT(100));
        lv_obj_set_height(tr, 14);
        lv_obj_set_style_bg_opa(tr, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_side(tr, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(tr, lv_color_hex(0x111827), 0);
        lv_obj_set_style_border_width(tr, 1, 0);
        lv_obj_set_style_pad_all(tr, 1, 0);
        lv_obj_set_style_pad_column(tr, 2, 0);
        lv_obj_set_flex_flow(tr, LV_FLEX_FLOW_ROW);
        lv_obj_clear_flag(tr, LV_OBJ_FLAG_SCROLLABLE);

        if(strlen(r->entry->date) >= 10)
            snprintf(buf, sizeof(buf), "%s", r->entry->date + 5);
        else
            snprintf(buf, sizeof(buf), "%s", r->entry->date);
        add_cell(tr, buf, lv_color_hex(COL_GRAY_TEXT), false);

        add_cell(tr, r->entry->employee_name, lv_color_hex(COL_CYAN), false);

        add_cell(tr, r->item->barcode, lv_color_hex(COL_CYAN), false);

        snprintf(buf, sizeof(buf), "%.2f", r->item->price);
        add_cell(tr, buf, lv_color_hex(COL_CYAN), true);

        snprintf(buf, sizeof(buf), "%d", r->item->quantity);
        add_cell(tr, buf, lv_color_hex(COL_CYAN), true);

        if(r->is_last_for_salesman)
            snprintf(buf, sizeof(buf), "%d", r->salesman_total_qty);
        else
            buf[0] = '\0';
        add_cell(tr, buf, lv_color_hex(COL_CYAN), true);

        if(r->is_last_in_sale)
            snprintf(buf, sizeof(buf), "%.2f", r->entry->grand_total);
        else
            buf[0] = '\0';
        add_cell(tr, buf, lv_color_hex(COL_YELLOW), true);

        if(r->is_last_in_sale) {
            float manual = r->entry->has_manual_total ? r->entry->manual_total : 0.0f;
            snprintf(buf, sizeof(buf), "%.2f", manual);
        }
        else {
            buf[0] = '\0';
        }
        add_cell(tr, buf, lv_color_hex(COL_GREEN), true);
    }
}

static void review_hide_async(void * ud)
{
    (void)ud;
    ui_email_review_hide();
}

static void close_cb(lv_event_t * e)
{
    (void)e;
#if APP_MAIL_ON_DEVICE
    if(s_mail_busy) return;
#endif
    ui_email_cb_t cb = s.on_close;
    void * ud = s.user_data;
    ui_email_review_hide();
    if(cb) cb(ud);
}

#if APP_MAIL_ON_DEVICE
static void mail_fail_on_ui(const char * reason)
{
    s_mail_busy = false;
    s_mail_batch = 0;
    snprintf(s_mail_msg, sizeof(s_mail_msg), "%s",
             reason ? reason : "Cannot start email task");
    EMAIL_LOG("fail: %s", s_mail_msg);
    if(s_mail_sent_cb) {
        s_mail_sent_cb(0, s_mail_msg, s_mail_ud);
        s_mail_sent_cb = NULL;
    }
    s_mail_ud = NULL;
}

static void mail_task_finished(int batch, const char * msg, void * ud)
{
    (void)ud;
    s_mail_batch = batch;
    if(msg && msg[0])
        snprintf(s_mail_msg, sizeof(s_mail_msg), "%s", msg);
    else
        s_mail_msg[0] = '\0';
    mail_send_done_async(NULL);
}

/** Runs on next LVGL tick — starts dedicated mail task (not loopTask). */
static void mail_kick_async(void * ud)
{
    (void)ud;
    if(!s_mail_busy) {
        EMAIL_LOG("kick ignored (not busy)");
        return;
    }
    if(!app_mail_task_queue(mail_task_finished, NULL)) {
        EMAIL_LOG("mail task queue failed: %s", app_mail_task_last_error());
        char err[96];
        snprintf(err, sizeof(err), "Email: %s", app_mail_task_last_error());
        mail_fail_on_ui(err);
        return;
    }
    EMAIL_LOG("mail task queued");
}

static void mail_send_done_async(void * ud)
{
    (void)ud;
    s_mail_busy = false;
    EMAIL_LOG("send done batch=%d msg=%s", s_mail_batch,
              s_mail_msg[0] ? s_mail_msg : "(empty)");
    if(s_mail_sent_cb)
        s_mail_sent_cb(s_mail_batch, s_mail_msg, s_mail_ud);
    else
        EMAIL_LOG("ERROR: sent callback was NULL");
    s_mail_sent_cb = NULL;
    s_mail_ud = NULL;
    lv_refr_now(NULL);
}

#endif /* APP_MAIL_ON_DEVICE */

static void send_cb(lv_event_t * e)
{
    (void)e;
    int pending = app_pending_email_count();
    EMAIL_LOG("APPROVE clicked pending=%d", pending);
    if(pending == 0) {
        EMAIL_LOG("nothing pending — closing");
        lv_async_call(review_hide_async, NULL);
        return;
    }
#if APP_MAIL_ON_DEVICE
    if(s_mail_busy) {
        EMAIL_LOG("already sending — ignored");
        return;
    }
    s_mail_busy = true;
    s_mail_sent_cb = s.on_sent;
    s_mail_ud = s.user_data;
    s_mail_msg[0] = '\0';
    s_mail_batch = 0;
    if(!s_mail_sent_cb)
        EMAIL_LOG("WARN: on_sent callback not set");
    if(s.send_btn && lv_obj_is_valid(s.send_btn)) {
        lv_obj_add_state(s.send_btn, LV_STATE_DISABLED);
        lv_obj_clear_flag(s.send_btn, LV_OBJ_FLAG_CLICKABLE);
    }
    if(s.send_btn_lbl && lv_obj_is_valid(s.send_btn_lbl))
        lv_label_set_text(s.send_btn_lbl, "Sending...");
    /* Defer task create + any screen teardown — never delete this overlay from send_cb. */
    lv_async_call(mail_kick_async, NULL);
    return;
#else
    char msg[128];
    int batch = app_send_pending_email(msg, sizeof(msg));
    ui_email_sent_cb_t sent = s.on_sent;
    void * ud = s.user_data;
    if(sent) sent(batch, msg, ud);
    else
        ui_email_review_hide();
    lv_refr_now(NULL);
#endif
}

void ui_email_review_hide(void)
{
    if(s.overlay) {
        lv_obj_del(s.overlay);
        s.overlay = NULL;
    }
    memset(&s, 0, sizeof(s));
}

typedef struct {
    lv_obj_t * overlay;
    ui_email_cb_t on_ok;
    void * user_data;
} email_result_t;

static email_result_t s_result;

static void result_ok_cb(lv_event_t * e)
{
    (void)e;
    ui_email_cb_t cb = s_result.on_ok;
    void * ud = s_result.user_data;
    ui_email_result_hide();
    if(cb) cb(ud);
}

void ui_email_result_hide(void)
{
    if(s_result.overlay) {
        lv_obj_del(s_result.overlay);
        s_result.overlay = NULL;
    }
    s_result.on_ok = NULL;
    s_result.user_data = NULL;
}

void ui_email_result_show(lv_obj_t * parent, bool success, const char * title, const char * message,
                          ui_email_cb_t on_ok, void * user_data)
{
    (void)parent;
    ui_email_result_hide();
    s_result.on_ok = on_ok;
    s_result.user_data = user_data;

    lv_obj_t * layer = email_overlay_parent();
    lv_coord_t sw, sh;
    screen_size(layer, &sw, &sh);

    s_result.overlay = lv_obj_create(layer);
    lv_obj_set_size(s_result.overlay, sw, sh);
    lv_obj_set_pos(s_result.overlay, 0, 0);
    lv_obj_add_flag(s_result.overlay, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(s_result.overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_result.overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_result.overlay, 0, 0);
    lv_obj_clear_flag(s_result.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s_result.overlay);

    lv_obj_t * box = lv_obj_create(s_result.overlay);
    lv_coord_t box_w = sw - 24;
    if(box_w > 280) box_w = 280;
    lv_obj_set_width(box, box_w);
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(success ? 0x16A34A : COL_RED), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_set_style_pad_all(box, 12, 0);
    lv_obj_set_style_pad_row(box, 8, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * icon = lv_label_create(box);
    lv_label_set_text(icon, success ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(icon, lv_color_hex(success ? 0x4ADE80 : COL_RED), 0);
    lv_obj_set_style_text_font(icon, FONT_SM, 0);

    lv_obj_t * ttl = lv_label_create(box);
    lv_label_set_text(ttl, title && title[0] ? title : (success ? "SUCCESS" : "FAILED"));
    lv_obj_set_style_text_color(ttl, lv_color_hex(success ? 0x4ADE80 : COL_RED), 0);
    lv_obj_set_style_text_font(ttl, FONT_SM, 0);
    lv_obj_set_style_text_align(ttl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ttl, LV_PCT(100));

    lv_obj_t * body = lv_label_create(box);
    lv_label_set_text(body, message && message[0] ? message
                                                 : (success ? "Invoice emailed successfully."
                                                            : "Could not send email."));
    lv_obj_set_style_text_color(body, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(body, FONT_SM, 0);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(body, LV_PCT(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);

    lv_obj_t * ok_btn = lv_btn_create(box);
    lv_obj_set_width(ok_btn, LV_PCT(100));
    lv_obj_set_height(ok_btn, 32);
    lv_obj_set_style_bg_color(ok_btn, lv_color_hex(success ? 0x15803D : 0x7F1D1D), 0);
    lv_obj_add_event_cb(ok_btn, result_ok_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "OK");
    lv_obj_set_style_text_font(ok_lbl, FONT_SM, 0);
    lv_obj_center(ok_lbl);
    lv_obj_move_foreground(s_result.overlay);
    EMAIL_LOG("result modal shown success=%d title=%s", success ? 1 : 0,
              title ? title : "?");
    lv_refr_now(NULL);
}

void ui_email_review_hide_before_result(void)
{
    ui_email_review_hide();
}

void ui_email_review_show(lv_obj_t * parent, ui_email_sent_cb_t on_sent, ui_email_cb_t on_close,
                          void * user_data)
{
    (void)parent;
    ui_email_result_hide();
    ui_email_review_hide();
#if APP_MAIL_ON_DEVICE
    s_mail_busy = false;
    s_mail_sent_cb = NULL;
#endif
    s.on_sent = on_sent;
    s.on_close = on_close;
    s.user_data = user_data;

    app_settings_t * cfg = app_settings();

    lv_obj_t * layer = email_overlay_parent();
    lv_coord_t sw, sh;
    screen_size(layer, &sw, &sh);
    bool portrait = sh > sw;

    s.overlay = lv_obj_create(layer);
    lv_obj_set_size(s.overlay, sw, sh);
    lv_obj_set_pos(s.overlay, 0, 0);
    lv_obj_add_flag(s.overlay, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(s.overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s.overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s.overlay, 0, 0);
    lv_obj_set_style_pad_all(s.overlay, portrait ? 4 : 6, 0);
    lv_obj_set_flex_flow(s.overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s.overlay);

    lv_obj_t * modal = lv_obj_create(s.overlay);
    lv_coord_t modal_w = sw - (portrait ? 8 : 12);
    lv_coord_t modal_h = sh - (portrait ? 12 : 24);
    lv_obj_set_size(modal, modal_w, modal_h);
    lv_obj_set_style_bg_color(modal, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_border_color(modal, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(modal, 1, 0);
    lv_obj_set_style_radius(modal, 6, 0);
    lv_obj_set_style_pad_all(modal, 0, 0);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * head = lv_obj_create(modal);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, 22);
    lv_obj_set_style_bg_color(head, lv_color_hex(0x030712), 0);
    lv_obj_set_style_border_side(head, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(head, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(head, 1, 0);
    lv_obj_set_style_pad_hor(head, 6, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

    char title[64];
    snprintf(title, sizeof(title), "PENDING EMAIL - %s", cfg->promo_head_name);
    s.title_lbl = lv_label_create(head);
    lv_label_set_text(s.title_lbl, title);
    lv_obj_set_style_text_color(s.title_lbl, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(s.title_lbl, FONT_SM, 0);
    lv_obj_set_flex_grow(s.title_lbl, 1);
    lv_label_set_long_mode(s.title_lbl, LV_LABEL_LONG_DOT);

    lv_obj_t * xbtn = lv_btn_create(head);
    lv_obj_set_size(xbtn, 20, 18);
    lv_obj_set_style_bg_opa(xbtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(xbtn, 0, 0);
    lv_obj_add_event_cb(xbtn, close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * xl = lv_label_create(xbtn);
    lv_label_set_text(xl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xl, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_center(xl);

    lv_obj_t * body = lv_obj_create(modal);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_color(body, lv_color_hex(0x030712), 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    s.empty_panel = lv_obj_create(body);
    lv_obj_set_width(s.empty_panel, LV_PCT(100));
    lv_obj_set_height(s.empty_panel, LV_PCT(100));
    lv_obj_set_style_bg_opa(s.empty_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.empty_panel, 0, 0);
    lv_obj_set_flex_flow(s.empty_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.empty_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s.empty_panel, 6, 0);
    lv_obj_clear_flag(s.empty_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * el_main = lv_label_create(s.empty_panel);
    lv_label_set_text(el_main, "All invoices sent");
    lv_obj_set_style_text_color(el_main, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(el_main, FONT_SM, 0);
    lv_obj_set_style_text_align(el_main, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(el_main, LV_PCT(100));

    lv_obj_t * el_sub = lv_label_create(s.empty_panel);
    lv_label_set_text(el_sub, "View sent status in History tab");
    lv_obj_set_style_text_color(el_sub, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_font(el_sub, FONT_SM, 0);
    lv_obj_set_style_text_align(el_sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(el_sub, LV_PCT(100));
    lv_label_set_long_mode(el_sub, LV_LABEL_LONG_WRAP);

    s.table_wrap = lv_obj_create(body);
    lv_obj_set_width(s.table_wrap, LV_PCT(100));
    lv_obj_set_flex_grow(s.table_wrap, 1);
    lv_obj_set_style_bg_opa(s.table_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.table_wrap, 0, 0);
    lv_obj_set_style_pad_all(s.table_wrap, 0, 0);
    lv_obj_set_flex_flow(s.table_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.table_wrap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * thead = lv_obj_create(s.table_wrap);
    lv_obj_set_width(thead, LV_PCT(100));
    lv_obj_set_height(thead, 14);
    lv_obj_set_style_bg_color(thead, lv_color_hex(COL_PANEL2), 0);
    lv_obj_set_style_border_side(thead, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(thead, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(thead, 1, 0);
    lv_obj_set_style_pad_all(thead, 1, 0);
    lv_obj_set_style_pad_column(thead, 2, 0);
    lv_obj_set_flex_flow(thead, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(thead, LV_OBJ_FLAG_SCROLLABLE);

    static const char * pcols[] = {"DT", "SM", "BCOD", "PRC", "O", "TO", "SYS", "MAN"};
    static const char * lcols[] = {"DT", "SM", "BARCODE", "PRC", "Q", "TQ", "SYS", "MAN"};
    for(int i = 0; i < 8; i++) {
        lv_obj_t * th = lv_label_create(thead);
        lv_label_set_text(th, portrait ? pcols[i] : lcols[i]);
        lv_obj_set_style_text_color(th, lv_color_hex(COL_YELLOW), 0);
        lv_obj_set_style_text_font(th, FONT_SM, 0);
        if(i >= 3) lv_obj_set_style_text_align(th, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_flex_grow(th, 1);
    }

    lv_obj_t * scroll = lv_obj_create(s.table_wrap);
    lv_obj_set_width(scroll, LV_PCT(100));
    lv_obj_set_flex_grow(scroll, 1);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_pad_all(scroll, 0, 0);
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);

    s.table_body = lv_obj_create(scroll);
    lv_obj_set_width(s.table_body, LV_PCT(100));
    lv_obj_set_height(s.table_body, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s.table_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.table_body, 0, 0);
    lv_obj_set_style_pad_all(s.table_body, 0, 0);
    lv_obj_set_flex_flow(s.table_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s.table_body, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * foot = lv_obj_create(modal);
    lv_obj_set_width(foot, LV_PCT(100));
    lv_obj_set_height(foot, 32);
    lv_obj_set_style_bg_color(foot, lv_color_hex(0x030712), 0);
    lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(foot, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(foot, 1, 0);
    lv_obj_set_style_pad_all(foot, 4, 0);
    lv_obj_set_style_pad_column(foot, 6, 0);
    lv_obj_set_flex_flow(foot, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * cancel_btn = lv_btn_create(foot);
    lv_obj_set_flex_grow(cancel_btn, 1);
    lv_obj_set_height(cancel_btn, 24);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(COL_PANEL2), 0);
    lv_obj_add_event_cb(cancel_btn, close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * cl = lv_label_create(cancel_btn);
    lv_label_set_text(cl, "CANCEL");
    lv_obj_set_style_text_font(cl, FONT_SM, 0);
    lv_obj_center(cl);

    s.send_btn = lv_btn_create(foot);
    lv_obj_set_flex_grow(s.send_btn, 1);
    lv_obj_set_height(s.send_btn, 24);
    lv_obj_set_style_bg_color(s.send_btn, lv_color_hex(0x15803D), 0);
    lv_obj_add_event_cb(s.send_btn, send_cb, LV_EVENT_CLICKED, NULL);
    s.send_btn_lbl = lv_label_create(s.send_btn);
    lv_label_set_text(s.send_btn_lbl, "APPROVE & SEND");
    lv_obj_set_style_text_font(s.send_btn_lbl, FONT_SM, 0);
    lv_obj_center(s.send_btn_lbl);

    refresh_table();
}
