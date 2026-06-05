#include "ui_header.h"
#include "ui_colors.h"
#include "ui_touch.h"
#include "app_data.h"
#include "app_display.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#if LV_FONT_MONTSERRAT_10
  #define FONT_SM  (&lv_font_montserrat_10)
#else
  #define FONT_SM  (&lv_font_montserrat_12)
#endif

#define HDR_H  APP_HDR_H

typedef struct {
    ui_header_t * hdr;
    app_screen_t screen;
} hdr_btn_t;

static hdr_btn_t s_nav_btns[3];
static ui_header_t s_hdr_store;

void ui_header_set_date_changed_cb(void (*cb)(void))
{
    (void)cb;
}

static void style_tab(lv_obj_t * btn, bool active)
{
    if(active) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x155E75), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(COL_CYAN), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(COL_CYAN), 0);
    }
    else {
        lv_obj_set_style_bg_color(btn, lv_color_hex(COL_PANEL), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(COL_GRAY), 0);
    }
}

static void nav_clicked(lv_event_t * e)
{
    hdr_btn_t * b = lv_event_get_user_data(e);
    if(b && b->hdr && b->hdr->on_nav) b->hdr->on_nav(b->screen, b->hdr->user_data);
}

static void settings_clicked(lv_event_t * e)
{
    ui_header_t * hdr = lv_event_get_user_data(e);
    if(hdr && hdr->on_settings) hdr->on_settings(hdr->user_data);
}

static void email_clicked(lv_event_t * e)
{
    ui_header_t * hdr = lv_event_get_user_data(e);
    if(hdr && hdr->on_email) hdr->on_email(hdr->user_data);
}

ui_header_t ui_header_create(lv_obj_t * parent, app_screen_t current,
                             void (*on_nav)(app_screen_t, void *),
                             void (*on_settings)(void *),
                             void (*on_email)(void *),
                             void * user_data)
{
    memset(&s_hdr_store, 0, sizeof(s_hdr_store));
    s_hdr_store.current = current;
    s_hdr_store.on_nav = on_nav;
    s_hdr_store.on_settings = on_settings;
    s_hdr_store.on_email = on_email;
    s_hdr_store.user_data = user_data;

    s_hdr_store.root = lv_obj_create(parent);
    lv_obj_set_size(s_hdr_store.root, APP_SCREEN_W, HDR_H);
    lv_obj_align(s_hdr_store.root, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_hdr_store.root, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_border_side(s_hdr_store.root, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(s_hdr_store.root, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s_hdr_store.root, 1, 0);
    lv_obj_set_style_pad_all(s_hdr_store.root, 2, 0);
    lv_obj_set_style_radius(s_hdr_store.root, 0, 0);
    lv_obj_clear_flag(s_hdr_store.root, LV_OBJ_FLAG_SCROLLABLE);

#if APP_SCREEN_H > APP_SCREEN_W
    /* Portrait 320px: row1 = date + icons, row2 = Sales | Dash | Hist */
    lv_obj_set_flex_flow(s_hdr_store.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_hdr_store.root, 2, 0);

    lv_obj_t * row1 = lv_obj_create(s_hdr_store.root);
    lv_obj_set_width(row1, LV_PCT(100));
    lv_obj_set_height(row1, 18);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * date_row = lv_obj_create(row1);
    lv_obj_set_size(date_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(date_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_row, 0, 0);
    lv_obj_set_style_pad_all(date_row, 0, 0);
    lv_obj_set_flex_flow(date_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(date_row, 2, 0);
    lv_obj_clear_flag(date_row, LV_OBJ_FLAG_SCROLLABLE);
    s_hdr_store.date_lbl = lv_label_create(date_row);
    lv_obj_set_style_text_color(s_hdr_store.date_lbl, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_text_font(s_hdr_store.date_lbl, FONT_SM, 0);

    s_hdr_store.clock_lbl = lv_label_create(date_row);
    lv_obj_set_style_text_color(s_hdr_store.clock_lbl, lv_color_hex(COL_GRAY), 0);
    lv_obj_set_style_text_font(s_hdr_store.clock_lbl, FONT_SM, 0);
    lv_obj_set_style_pad_left(s_hdr_store.clock_lbl, 4, 0);
    lv_label_set_text(s_hdr_store.clock_lbl, "--:--");

    lv_obj_t * icon_row = lv_obj_create(row1);
    lv_obj_set_size(icon_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(icon_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_row, 0, 0);
    lv_obj_set_style_pad_all(icon_row, 0, 0);
    lv_obj_set_flex_flow(icon_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(icon_row, 2, 0);
    lv_obj_clear_flag(icon_row, LV_OBJ_FLAG_SCROLLABLE);

    s_hdr_store.btn_settings = lv_btn_create(icon_row);
    lv_obj_set_size(s_hdr_store.btn_settings, 32, 24);
    lv_obj_set_style_radius(s_hdr_store.btn_settings, 4, 0);
    lv_obj_set_style_shadow_width(s_hdr_store.btn_settings, 0, 0);
    lv_obj_t * sl = lv_label_create(s_hdr_store.btn_settings);
    lv_label_set_text(sl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(sl, FONT_SM, 0);
    lv_obj_center(sl);
    lv_obj_add_event_cb(s_hdr_store.btn_settings, settings_clicked, LV_EVENT_CLICKED, &s_hdr_store);
    ui_touch_expand(s_hdr_store.btn_settings, 8);

    s_hdr_store.btn_email = lv_btn_create(icon_row);
    lv_obj_set_size(s_hdr_store.btn_email, 32, 24);
    lv_obj_set_style_radius(s_hdr_store.btn_email, 4, 0);
    lv_obj_set_style_shadow_width(s_hdr_store.btn_email, 0, 0);
    lv_obj_t * ml = lv_label_create(s_hdr_store.btn_email);
    lv_label_set_text(ml, LV_SYMBOL_ENVELOPE);
    lv_obj_set_style_text_font(ml, FONT_SM, 0);
    lv_obj_center(ml);
    lv_obj_add_event_cb(s_hdr_store.btn_email, email_clicked, LV_EVENT_CLICKED, &s_hdr_store);
    ui_touch_expand(s_hdr_store.btn_email, 8);

    lv_obj_t * row2 = lv_obj_create(s_hdr_store.root);
    lv_obj_set_width(row2, LV_PCT(100));
    lv_obj_set_height(row2, 26);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row2, 3, 0);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);

    const char * labels[] = {"Sales", "Dash", "Hist"};
    app_screen_t screens[] = {APP_SCREEN_SALES, APP_SCREEN_DASH, APP_SCREEN_HISTORY};
    lv_obj_t ** tabs[] = {&s_hdr_store.tab_sales, &s_hdr_store.tab_dash, &s_hdr_store.tab_hist};
    for(int i = 0; i < 3; i++) {
        lv_obj_t * b = lv_btn_create(row2);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_height(b, 24);
        lv_obj_set_style_pad_hor(b, 4, 0);
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t * l = lv_label_create(b);
        lv_label_set_text(l, labels[i]);
        lv_obj_set_style_text_font(l, FONT_SM, 0);
        lv_obj_center(l);
        *tabs[i] = b;
        s_nav_btns[i].hdr = &s_hdr_store;
        s_nav_btns[i].screen = screens[i];
        lv_obj_add_event_cb(b, nav_clicked, LV_EVENT_CLICKED, &s_nav_btns[i]);
        ui_touch_expand(b, 10);
    }
#else
    lv_obj_set_flex_flow(s_hdr_store.root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_hdr_store.root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_hdr_store.root, 3, 0);

    lv_obj_t * date_row = lv_obj_create(s_hdr_store.root);
    lv_obj_set_size(date_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(date_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_row, 0, 0);
    lv_obj_set_style_pad_all(date_row, 0, 0);
    lv_obj_set_flex_flow(date_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(date_row, 2, 0);
    lv_obj_clear_flag(date_row, LV_OBJ_FLAG_SCROLLABLE);

    s_hdr_store.date_lbl = lv_label_create(date_row);
    lv_obj_set_style_text_color(s_hdr_store.date_lbl, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_text_font(s_hdr_store.date_lbl, FONT_SM, 0);

    s_hdr_store.clock_lbl = lv_label_create(date_row);
    lv_obj_set_style_text_color(s_hdr_store.clock_lbl, lv_color_hex(COL_GRAY), 0);
    lv_obj_set_style_text_font(s_hdr_store.clock_lbl, FONT_SM, 0);
    lv_obj_set_style_pad_left(s_hdr_store.clock_lbl, 4, 0);
    lv_label_set_text(s_hdr_store.clock_lbl, "--:--");

    lv_obj_t * spacer = lv_obj_create(s_hdr_store.root);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    const char * labels[] = {"Sales", "Dash", "Hist"};
    app_screen_t screens[] = {APP_SCREEN_SALES, APP_SCREEN_DASH, APP_SCREEN_HISTORY};
    lv_obj_t ** tabs[] = {&s_hdr_store.tab_sales, &s_hdr_store.tab_dash, &s_hdr_store.tab_hist};

    for(int i = 0; i < 3; i++) {
        lv_obj_t * b = lv_btn_create(s_hdr_store.root);
        lv_obj_set_height(b, 26);
        lv_obj_set_style_pad_hor(b, 8, 0);
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t * l = lv_label_create(b);
        lv_label_set_text(l, labels[i]);
        lv_obj_set_style_text_font(l, FONT_SM, 0);
        lv_obj_center(l);
        *tabs[i] = b;
        s_nav_btns[i].hdr = &s_hdr_store;
        s_nav_btns[i].screen = screens[i];
        lv_obj_add_event_cb(b, nav_clicked, LV_EVENT_CLICKED, &s_nav_btns[i]);
        ui_touch_expand(b, 8);
    }

    s_hdr_store.btn_settings = lv_btn_create(s_hdr_store.root);
    lv_obj_set_size(s_hdr_store.btn_settings, 28, 26);
    lv_obj_set_style_radius(s_hdr_store.btn_settings, 4, 0);
    lv_obj_set_style_shadow_width(s_hdr_store.btn_settings, 0, 0);
    lv_obj_t * sl = lv_label_create(s_hdr_store.btn_settings);
    lv_label_set_text(sl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(sl, FONT_SM, 0);
    lv_obj_center(sl);
    lv_obj_add_event_cb(s_hdr_store.btn_settings, settings_clicked, LV_EVENT_CLICKED, &s_hdr_store);
    ui_touch_expand(s_hdr_store.btn_settings, 8);

    s_hdr_store.btn_email = lv_btn_create(s_hdr_store.root);
    lv_obj_set_size(s_hdr_store.btn_email, 28, 26);
    lv_obj_set_style_radius(s_hdr_store.btn_email, 4, 0);
    lv_obj_set_style_shadow_width(s_hdr_store.btn_email, 0, 0);
    lv_obj_t * ml = lv_label_create(s_hdr_store.btn_email);
    lv_label_set_text(ml, LV_SYMBOL_ENVELOPE);
    lv_obj_set_style_text_font(ml, FONT_SM, 0);
    lv_obj_center(ml);
    lv_obj_add_event_cb(s_hdr_store.btn_email, email_clicked, LV_EVENT_CLICKED, &s_hdr_store);
    ui_touch_expand(s_hdr_store.btn_email, 8);
#endif

    ui_header_set_screen(&s_hdr_store, current);
    ui_header_refresh(&s_hdr_store);
    return s_hdr_store;
}

void ui_header_set_screen(ui_header_t * hdr, app_screen_t screen)
{
    if(!hdr) return;
    hdr->current = screen;
    style_tab(hdr->tab_sales, screen == APP_SCREEN_SALES);
    style_tab(hdr->tab_dash, screen == APP_SCREEN_DASH);
    style_tab(hdr->tab_hist, screen == APP_SCREEN_HISTORY);
    if(hdr->btn_settings)
        style_tab(hdr->btn_settings, screen == APP_SCREEN_SETTINGS);
}

void ui_header_refresh(ui_header_t * hdr)
{
    if(!hdr) return;
    if(hdr->clock_lbl) {
        time_t t = time(NULL);
        struct tm tm_buf;
        const struct tm * tm = localtime_r(&t, &tm_buf);
        char clk[32];
        if(tm) {
#if APP_PORTRAIT
            /* Portrait: oras lang — petsa nasa date picker (< 2026-05-20 >) */
            strftime(clk, sizeof(clk), "%H:%M", tm);
#else
            strftime(clk, sizeof(clk), "%b %d %H:%M:%S", tm);
#endif
        }
        else {
            strncpy(clk, "--:--", sizeof(clk) - 1);
            clk[sizeof(clk) - 1] = '\0';
        }
        lv_label_set_text(hdr->clock_lbl, clk);
    }
    if(hdr->date_lbl)
        lv_label_set_text(hdr->date_lbl, app_today_date());
}
