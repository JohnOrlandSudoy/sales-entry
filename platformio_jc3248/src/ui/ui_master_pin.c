#include "ui_master_pin.h"
#include "ui_colors.h"
#include "app_data.h"
#include "app_display.h"
#include <string.h>

#if LV_FONT_MONTSERRAT_10
  #define FONT_SM  (&lv_font_montserrat_10)
#else
  #define FONT_SM  (&lv_font_montserrat_12)
#endif
#if LV_FONT_MONTSERRAT_14
  #define FONT_MD  (&lv_font_montserrat_14)
#else
  #define FONT_MD  FONT_SM
#endif

#define PIN_LEN      4
#define KEY_GAP      2
#define PIN_ROW_H    38

#define COL_PANEL_D  COL_BG
#define COL_BORDER_D COL_PANEL2
#define COL_GRAY_D   COL_PANEL2

typedef struct {
    const char * key;
} mpin_key_evt_t;

typedef struct {
    lv_obj_t * overlay;
    lv_obj_t * dots[PIN_LEN];
    lv_obj_t * err_lbl;
    char pin[PIN_LEN + 1];
    bool error;
    ui_master_pin_cb_t on_success;
    ui_master_pin_cb_t on_cancel;
    void * user_data;
    ui_pin_kind_t kind;
} mpin_t;

static mpin_t s;
static mpin_key_evt_t s_key_evts[12];
static ui_master_pin_cb_t s_pending_cb;
static void * s_pending_ud;
static int s_evt_idx;

#define PIN_DEFER_MS  50

static void deferred_ok_timer(lv_timer_t * t);
static void deferred_cancel_timer(lv_timer_t * t);

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

static void trim_pin_field(char * pin, size_t pin_sz)
{
    if(!pin || pin_sz == 0) return;
    pin[pin_sz - 1] = '\0';
    size_t n = strlen(pin);
    while(n > 0 && (pin[n - 1] == ' ' || pin[n - 1] == '\r' || pin[n - 1] == '\n' || pin[n - 1] == '\t'))
        pin[--n] = '\0';
}

static bool pin_matches(const char * entered)
{
    app_settings_t * st = app_settings();
    char expect[sizeof(st->master_pin)];
    if(s.kind == UI_PIN_KIND_SYSTEM) {
        strncpy(expect, st->system_pin, sizeof(expect) - 1);
        if(expect[0] == '\0') strcpy(expect, "1234");
    }
    else {
        strncpy(expect, st->master_pin, sizeof(expect) - 1);
        if(expect[0] == '\0') strcpy(expect, "9999");
    }
    trim_pin_field(expect, sizeof(expect));
    return strcmp(entered, expect) == 0;
}

static void refresh_dots(void)
{
    int len = (int)strlen(s.pin);
    for(int i = 0; i < PIN_LEN; i++) {
        lv_obj_t * dot = s.dots[i];
        if(!dot || !lv_obj_is_valid(dot)) continue;
        bool filled = i < len;
        if(s.error) {
            lv_obj_set_style_border_color(dot, lv_color_hex(COL_RED), 0);
            lv_obj_set_style_bg_color(dot, lv_color_hex(0x7F1D1D), 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_50, 0);
        }
        else if(filled) {
            lv_obj_set_style_border_color(dot, lv_color_hex(COL_GRAY), 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
        }
        else {
            lv_obj_set_style_border_color(dot, lv_color_hex(COL_GRAY_D), 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
        }
        if(lv_obj_get_child_cnt(dot) > 0) {
            lv_obj_t * inner = lv_obj_get_child(dot, 0);
            if(filled && !s.error) {
                lv_obj_set_style_bg_color(inner, lv_color_hex(COL_GRAY), 0);
                lv_obj_set_style_bg_opa(inner, LV_OPA_COVER, 0);
            }
            else if(s.error) {
                lv_obj_set_style_bg_color(inner, lv_color_hex(COL_RED), 0);
                lv_obj_set_style_bg_opa(inner, LV_OPA_COVER, 0);
            }
            else {
                lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, 0);
            }
        }
    }
}

static void clear_pin(void)
{
    s.pin[0] = '\0';
    s.error = false;
    if(s.err_lbl) lv_obj_add_flag(s.err_lbl, LV_OBJ_FLAG_HIDDEN);
    refresh_dots();
}

static void pin_key(const char * key)
{
    if(strcmp(key, "C") == 0) {
        clear_pin();
        return;
    }
    if(strcmp(key, "BK") == 0) {
        size_t len = strlen(s.pin);
        if(len > 0) s.pin[len - 1] = '\0';
        s.error = false;
        if(s.err_lbl) lv_obj_add_flag(s.err_lbl, LV_OBJ_FLAG_HIDDEN);
        refresh_dots();
        return;
    }
    if(strlen(s.pin) >= PIN_LEN) return;

    size_t len = strlen(s.pin);
    s.pin[len] = key[0];
    s.pin[len + 1] = '\0';
    s.error = false;
    if(s.err_lbl) lv_obj_add_flag(s.err_lbl, LV_OBJ_FLAG_HIDDEN);
    refresh_dots();

    if(strlen(s.pin) < PIN_LEN) return;

    if(pin_matches(s.pin)) {
        s_pending_cb = s.on_success;
        s_pending_ud = s.user_data;
        lv_timer_t * tm = lv_timer_create(deferred_ok_timer, PIN_DEFER_MS, NULL);
        lv_timer_set_repeat_count(tm, 1);
    }
    else {
        s.error = true;
        refresh_dots();
        if(s.err_lbl) lv_obj_clear_flag(s.err_lbl, LV_OBJ_FLAG_HIDDEN);
        s.pin[0] = '\0';
    }
}

static void key_cb(lv_event_t * e)
{
    mpin_key_evt_t * evt = lv_event_get_user_data(e);
    if(!evt || !evt->key) return;
    pin_key(evt->key);
}

static void cancel_cb(lv_event_t * e)
{
    (void)e;
    s_pending_cb = s.on_cancel;
    s_pending_ud = s.user_data;
    lv_timer_t * tm = lv_timer_create(deferred_cancel_timer, PIN_DEFER_MS, NULL);
    lv_timer_set_repeat_count(tm, 1);
}

static void overlay_block_cb(lv_event_t * e)
{
    lv_event_stop_bubbling(e);
}

static void deferred_ok_timer(lv_timer_t * t)
{
    ui_master_pin_cb_t cb = s_pending_cb;
    void * cb_ud = s_pending_ud;
    s_pending_cb = NULL;
    s_pending_ud = NULL;
    lv_timer_del(t);
    ui_master_pin_hide();
    if(cb) cb(cb_ud);
}

static void deferred_cancel_timer(lv_timer_t * t)
{
    ui_master_pin_cb_t cb = s_pending_cb;
    void * cb_ud = s_pending_ud;
    s_pending_cb = NULL;
    s_pending_ud = NULL;
    lv_timer_del(t);
    ui_master_pin_hide();
    if(cb) cb(cb_ud);
}

static void style_pin_key(const char * key_id, uint32_t * bg, uint32_t * border, uint32_t * text)
{
    *bg = COL_PANEL;
    *border = COL_BORDER_D;
    *text = COL_GREEN;
    if(strcmp(key_id, "C") == 0) {
        *bg = 0x431407;
        *border = 0x7C2D12;
        *text = COL_ORANGE;
    }
    else if(strcmp(key_id, "BK") == 0) {
        *bg = 0x450A0A;
        *border = 0x991B1B;
        *text = 0xF87171;
    }
}

static lv_obj_t * make_key(lv_obj_t * parent, const char * label, const char * key_id,
                           uint32_t bg, uint32_t border, uint32_t text,
                           lv_coord_t key_w, lv_coord_t key_h)
{
    lv_obj_t * btn = lv_btn_create(parent);
    if(key_w > 0 && key_h > 0)
        lv_obj_set_size(btn, key_w, key_h);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    mpin_key_evt_t * evt = &s_key_evts[s_evt_idx % 12];
    s_evt_idx++;
    evt->key = key_id;
    lv_obj_add_event_cb(btn, key_cb, LV_EVENT_CLICKED, evt);
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(text), 0);
    if(strcmp(key_id, "BK") == 0) {
        lv_obj_set_style_text_font(lbl, FONT_MD, 0);
        lv_obj_set_style_text_letter_space(lbl, 1, 0);
    }
    else {
        lv_obj_set_style_text_font(lbl, FONT_MD, 0);
    }
    lv_obj_center(lbl);
    return btn;
}

static void build_pin_keypad_portrait(lv_obj_t * panel)
{
    static const char * key_ids[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "C", "0", "BK"
    };
    static const char * key_labels[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "C", "0", LV_SYMBOL_BACKSPACE
    };

    lv_obj_t * rows = lv_obj_create(panel);
    lv_obj_set_width(rows, LV_PCT(100));
    lv_obj_set_height(rows, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rows, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rows, 0, 0);
    lv_obj_set_style_pad_all(rows, 0, 0);
    lv_obj_set_style_pad_row(rows, KEY_GAP, 0);
    lv_obj_set_flex_flow(rows, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(rows, LV_OBJ_FLAG_SCROLLABLE);

    for(int r = 0; r < 4; r++) {
        lv_obj_t * row = lv_obj_create(rows);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, PIN_ROW_H);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_pad_column(row, KEY_GAP, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        for(int c = 0; c < 3; c++) {
            const int i = r * 3 + c;
            uint32_t bg, border, tcol;
            style_pin_key(key_ids[i], &bg, &border, &tcol);
            lv_obj_t * btn = make_key(row, key_labels[i], key_ids[i], bg, border, tcol, 0, 0);
            lv_obj_set_flex_grow(btn, 1);
            lv_obj_set_height(btn, PIN_ROW_H);
        }
    }
}

static void build_pin_keypad_landscape(lv_obj_t * panel, lv_coord_t key_w, lv_coord_t key_h)
{
    static const char * key_ids[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "C", "0", "BK"
    };
    static const char * key_labels[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "C", "0", LV_SYMBOL_BACKSPACE
    };

    lv_coord_t grid_w = key_w * 3 + KEY_GAP * 2;
    lv_coord_t grid_h = key_h * 4 + KEY_GAP * 3;

    lv_obj_t * grid = lv_obj_create(panel);
    lv_obj_set_size(grid, grid_w, grid_h);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    static lv_coord_t col_dsc[] = {56, 56, 56, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {36, 36, 36, 36, LV_GRID_TEMPLATE_LAST};
    col_dsc[0] = key_w;
    col_dsc[1] = key_w;
    col_dsc[2] = key_w;
    row_dsc[0] = key_h;
    row_dsc[1] = key_h;
    row_dsc[2] = key_h;
    row_dsc[3] = key_h;
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_row(grid, KEY_GAP, 0);
    lv_obj_set_style_pad_column(grid, KEY_GAP, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    for(int i = 0; i < 12; i++) {
        uint32_t bg, border, tcol;
        style_pin_key(key_ids[i], &bg, &border, &tcol);
        lv_obj_t * btn = make_key(grid, key_labels[i], key_ids[i], bg, border, tcol, key_w, key_h);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_CENTER, i % 3, 1, LV_GRID_ALIGN_CENTER, i / 3, 1);
    }
}

void ui_master_pin_hide(void)
{
    if(s.overlay) {
        lv_obj_del(s.overlay);
        s.overlay = NULL;
    }
    memset(&s, 0, sizeof(s));
}

void ui_master_pin_show_kind(lv_obj_t * parent, const char * label,
                             ui_master_pin_cb_t on_success, ui_master_pin_cb_t on_cancel,
                             void * user_data, ui_pin_kind_t kind)
{
    ui_master_pin_hide();
    s.on_success = on_success;
    s.on_cancel = on_cancel;
    s.user_data = user_data;
    s.kind = kind;
    s.pin[0] = '\0';
    s_evt_idx = 0;

    lv_coord_t sw, sh;
    screen_size(parent, &sw, &sh);
    bool portrait = sh > sw;
    lv_coord_t key_w = portrait ? 0 : 56;
    lv_coord_t key_h = portrait ? 0 : 36;
    lv_coord_t panel_w = portrait ? (sw - 24) : 260;

    s.overlay = lv_obj_create(parent);
    lv_obj_set_size(s.overlay, sw, sh);
    lv_obj_set_pos(s.overlay, 0, 0);
    lv_obj_set_style_bg_color(s.overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s.overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s.overlay, 0, 0);
    lv_obj_set_style_pad_all(s.overlay, 0, 0);
    lv_obj_set_flex_flow(s.overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s.overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s.overlay, overlay_block_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_move_foreground(s.overlay);

    lv_obj_t * panel = lv_obj_create(s.overlay);
    lv_obj_set_width(panel, panel_w);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COL_PANEL_D), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(COL_BORDER_D), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_set_style_pad_row(panel, 4, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = lv_label_create(panel);
    lv_label_set_text(title, label ? label : "MASTER PIN REQUIRED");
    lv_obj_set_style_text_color(title, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(title, portrait ? FONT_MD : FONT_SM, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, panel_w - 16);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);

    lv_obj_t * dots_row = lv_obj_create(panel);
    lv_obj_set_size(dots_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(dots_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots_row, 0, 0);
    lv_obj_set_style_pad_all(dots_row, 0, 0);
    lv_obj_set_flex_flow(dots_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dots_row, 10, 0);
    lv_obj_clear_flag(dots_row, LV_OBJ_FLAG_SCROLLABLE);

    for(int i = 0; i < PIN_LEN; i++) {
        s.dots[i] = lv_obj_create(dots_row);
        lv_obj_set_size(s.dots[i], 28, 28);
        lv_obj_set_style_radius(s.dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s.dots[i], 2, 0);
        lv_obj_clear_flag(s.dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t * inner = lv_obj_create(s.dots[i]);
        lv_obj_set_size(inner, 20, 20);
        lv_obj_center(inner);
        lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(inner, 1, 0);
        lv_obj_set_style_border_color(inner, lv_color_hex(COL_GRAY_D), 0);
        lv_obj_clear_flag(inner, LV_OBJ_FLAG_SCROLLABLE);
    }
    refresh_dots();

    s.err_lbl = lv_label_create(panel);
    lv_label_set_text(s.err_lbl, "WRONG PIN");
    lv_obj_set_style_text_color(s.err_lbl, lv_color_hex(COL_RED), 0);
    lv_obj_set_style_text_font(s.err_lbl, FONT_SM, 0);
    lv_obj_add_flag(s.err_lbl, LV_OBJ_FLAG_HIDDEN);

    if(portrait)
        build_pin_keypad_portrait(panel);
    else
        build_pin_keypad_landscape(panel, key_w, key_h);

    if(s.kind != UI_PIN_KIND_SYSTEM && on_cancel) {
        lv_obj_t * cancel_btn = lv_btn_create(panel);
        lv_obj_set_width(cancel_btn, LV_PCT(100));
        lv_obj_set_height(cancel_btn, portrait ? 32 : 24);
        lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(COL_PANEL), 0);
        lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(cancel_btn, lv_color_hex(COL_BORDER_D), 0);
        lv_obj_set_style_border_width(cancel_btn, 1, 0);
        lv_obj_add_event_cb(cancel_btn, cancel_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t * cl = lv_label_create(cancel_btn);
        lv_label_set_text(cl, "CANCEL");
        lv_obj_set_style_text_color(cl, lv_color_hex(COL_GRAY_TEXT), 0);
        lv_obj_set_style_text_font(cl, FONT_MD, 0);
        lv_obj_center(cl);
    }
}

void ui_master_pin_show(lv_obj_t * parent, const char * label,
                        ui_master_pin_cb_t on_success, ui_master_pin_cb_t on_cancel,
                        void * user_data)
{
    ui_master_pin_show_kind(parent, label, on_success, on_cancel, user_data, UI_PIN_KIND_MASTER);
}
