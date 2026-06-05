#include "ui_keypad.h"
#include "ui_colors.h"
#include "app_display.h"
#include <stdio.h>
#include <string.h>

/* Prototype NumericKeypad dense + fillHeight: 3 cols, 4 equal rows in remaining space */
#define KEY_GAP     2
#if APP_SCREEN_W > APP_SCREEN_H
  #define DISP_H        22
  #define KP_GRID_MIN   88
  #define KP_PORTRAIT   0
#else
  #define DISP_H        16
  #define KP_ROW_H      22
  #define KP_COL_W      100
  #define KP_GRID_MIN   (KP_ROW_H * 4 + KEY_GAP * 3)
  #define KP_PORTRAIT   1
#endif

typedef struct {
    ui_keypad_t * kp;
    const char * key;
} keypad_evt_t;

static ui_keypad_t s_kp_singleton;
static keypad_evt_t s_evts[12];
static char s_safe_buf[4] = "";
static ui_keypad_intercept_fn s_intercept;
static ui_keypad_change_fn s_change_cb;

static bool kp_display_valid(ui_keypad_t * kp)
{
    return kp && kp->display_lbl && lv_obj_is_valid(kp->display_lbl);
}

static void format_display_amount(const char * raw, char * out, size_t out_sz)
{
    if(!raw || !raw[0] || !out || out_sz == 0) {
        if(out && out_sz) out[0] = '\0';
        return;
    }

    const char * dot = strchr(raw, '.');
    char int_buf[12];
    size_t ilen = dot ? (size_t)(dot - raw) : strlen(raw);
    if(ilen >= sizeof(int_buf)) ilen = sizeof(int_buf) - 1;
    memcpy(int_buf, raw, ilen);
    int_buf[ilen] = '\0';

    char grouped[16];
    size_t gi = 0;
    int leading = (int)ilen;
    for(int i = 0; i < leading; i++) {
        if(i > 0 && (leading - i) % 3 == 0)
            grouped[gi++] = ',';
        grouped[gi++] = int_buf[i];
    }
    grouped[gi] = '\0';

    if(dot)
        snprintf(out, out_sz, "%s%s", grouped, dot);
    else
        snprintf(out, out_sz, "%s", grouped);
}

static void refresh_display(ui_keypad_t * kp)
{
    if(!kp_display_valid(kp)) return;
    if(kp->value_buf[0]) {
        char shown[20];
        format_display_amount(kp->value_buf, shown, sizeof(shown));
        lv_label_set_text(kp->display_lbl, shown);
        lv_obj_set_style_text_color(kp->display_lbl, lv_color_hex(COL_GREEN), 0);
    }
    else {
        lv_label_set_text(kp->display_lbl, kp->placeholder ? kp->placeholder : "");
        lv_obj_set_style_text_color(kp->display_lbl, lv_color_hex(COL_GRAY), 0);
    }
}

static void key_clicked(lv_event_t * e)
{
    keypad_evt_t * evt = lv_event_get_user_data(e);
    ui_keypad_t * kp = evt->kp;
    const char * key = evt->key;
    char * v = kp->value_buf;

    if(!v || v == s_safe_buf || kp->max_len <= 0) return;

    if(s_intercept && s_intercept(key)) {
        refresh_display(kp);
        if(s_change_cb) s_change_cb();
        return;
    }

    if(strcmp(key, "BK") == 0) {
        size_t len = strlen(v);
        if(len > 0) v[len - 1] = '\0';
    }
    else if(strcmp(key, ".") == 0) {
        if(!kp->show_decimal) return;
        if(strchr(v, '.')) return;
        if(strlen(v) >= (size_t)kp->max_len) return;
        strcat(v, ".");
    }
    else {
        if(strlen(v) >= (size_t)kp->max_len) return;
        size_t len = strlen(v);
        v[len] = key[0];
        v[len + 1] = '\0';
    }
    refresh_display(kp);
    if(s_change_cb) s_change_cb();
}

static lv_obj_t * make_key_btn(lv_obj_t * parent, const char * label, const char * key_id,
                               uint32_t bg, uint32_t border, uint32_t text)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, KP_PORTRAIT ? 4 : 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(text), 0);
    if(strcmp(key_id, "BK") == 0) {
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_letter_space(lbl, 1, 0);
    }
    else {
        lv_obj_set_style_text_font(lbl, KP_PORTRAIT ? &lv_font_montserrat_14 : &lv_font_montserrat_16, 0);
    }
    lv_obj_center(lbl);

    static int evt_idx;
    keypad_evt_t * evt = &s_evts[evt_idx % 12];
    evt_idx++;
    evt->kp = &s_kp_singleton;
    evt->key = key_id;
    lv_obj_add_event_cb(btn, key_clicked, LV_EVENT_CLICKED, evt);
    return btn;
}

static lv_obj_t * make_key(lv_obj_t * parent, const char * label, const char * key_id,
                           uint32_t bg, uint32_t border, uint32_t text,
                           int col, int row, bool stretch)
{
    lv_obj_t * btn = make_key_btn(parent, label, key_id, bg, border, text);
    if(stretch) {
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1,
                             LV_GRID_ALIGN_STRETCH, row, 1);
    }
    else {
        lv_obj_set_size(btn, 48, 34);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_CENTER, col, 1,
                             LV_GRID_ALIGN_CENTER, row, 1);
    }
    return btn;
}

static void style_key(uint32_t key_idx, uint32_t * bg, uint32_t * border, uint32_t * text)
{
    static const char * keys[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "BK"
    };
    *bg = 0x111827;
    *border = 0x1F2937;
    *text = 0x22C55E;
    if(strcmp(keys[key_idx], "BK") == 0) {
        *border = 0x991B1B;
        *text = 0xF87171;
    }
    if(strcmp(keys[key_idx], ".") == 0) {
        *bg = 0x1F2937;
        *border = 0x374151;
        *text = 0xEAB308;
    }
}

static void keypad_portrait_rows(lv_obj_t * parent, bool show_decimal)
{
    static const char * keys[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "BK"
    };
    static const char * labels[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "DEL"
    };

    lv_obj_t * rows = lv_obj_create(parent);
    lv_obj_set_width(rows, LV_PCT(100));
    lv_obj_set_height(rows, KP_GRID_MIN);
    lv_obj_set_style_bg_opa(rows, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rows, 0, 0);
    lv_obj_set_style_pad_all(rows, 0, 0);
    lv_obj_set_style_pad_row(rows, KEY_GAP, 0);
    lv_obj_set_flex_flow(rows, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(rows, LV_OBJ_FLAG_SCROLLABLE);

    for(int r = 0; r < 4; r++) {
        lv_obj_t * row = lv_obj_create(rows);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, KP_ROW_H);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_pad_column(row, KEY_GAP, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        for(int c = 0; c < 3; c++) {
            const int i = r * 3 + c;
            uint32_t bg, border, text;
            style_key((uint32_t)i, &bg, &border, &text);
            lv_obj_t * btn = make_key_btn(row, labels[i], keys[i], bg, border, text);
            lv_obj_set_flex_grow(btn, 1);
            lv_obj_set_height(btn, KP_ROW_H);
            if(i == 9 && !show_decimal) {
                lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

ui_keypad_t ui_keypad_create(lv_obj_t * parent, char * value_buf, size_t buf_sz,
                             bool show_decimal, int max_len, const char * placeholder,
                             bool fill_height, bool show_display)
{
    memset(&s_kp_singleton, 0, sizeof(s_kp_singleton));
    ui_keypad_t * kp = &s_kp_singleton;
    kp->value_buf = value_buf;
    kp->value_buf_sz = buf_sz;
    kp->max_len = max_len;
    kp->show_decimal = show_decimal;
    kp->placeholder = placeholder;

    kp->root = lv_obj_create(parent);
    lv_obj_set_width(kp->root, LV_PCT(100));
    lv_obj_set_height(kp->root, fill_height ? LV_PCT(100) : LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(kp->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kp->root, 0, 0);
    lv_obj_set_style_pad_all(kp->root, 0, 0);
    lv_obj_set_style_pad_row(kp->root, KEY_GAP, 0);
    lv_obj_set_flex_flow(kp->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(kp->root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(kp->root, LV_OBJ_FLAG_SCROLLABLE);

    if(show_display) {
        /* Display — prototype: bg-black border, h-8, green value */
        lv_obj_t * disp = lv_obj_create(kp->root);
        lv_obj_set_width(disp, LV_PCT(100));
        lv_obj_set_height(disp, DISP_H);
        lv_obj_set_style_bg_color(disp, lv_color_hex(COL_BG_DARK), 0);
        lv_obj_set_style_border_color(disp, lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_border_width(disp, 1, 0);
        lv_obj_set_style_radius(disp, 6, 0);
        lv_obj_set_style_pad_hor(disp, 8, 0);
        lv_obj_clear_flag(disp, LV_OBJ_FLAG_SCROLLABLE);

        kp->display_lbl = lv_label_create(disp);
        lv_obj_set_width(kp->display_lbl, LV_PCT(100));
        lv_label_set_long_mode(kp->display_lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(kp->display_lbl, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(kp->display_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(kp->display_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
        refresh_display(kp);
    }

    /* Key grid — portrait: flex rows (reliable on ESP32); landscape: LVGL grid */
    if(KP_PORTRAIT) {
        keypad_portrait_rows(kp->root, show_decimal);
        return *kp;
    }

    lv_obj_t * grid = lv_obj_create(kp->root);
    lv_obj_set_width(grid, LV_PCT(100));
    if(fill_height) {
        lv_obj_set_flex_grow(grid, 1);
        lv_obj_set_height(grid, LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(grid, KP_GRID_MIN, 0);
    }
    else {
        lv_obj_set_height(grid, LV_SIZE_CONTENT);
    }
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_row(grid, KEY_GAP, 0);
    lv_obj_set_style_pad_column(grid, KEY_GAP, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    static int32_t col_fr[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_fr[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t col_px[] = {48, 48, 48, LV_GRID_TEMPLATE_LAST};
    static int32_t row_px[] = {34, 34, 34, 34, LV_GRID_TEMPLATE_LAST};

    if(fill_height) {
        lv_obj_set_grid_dsc_array(grid, col_fr, row_fr);
    }
    else {
        lv_obj_set_grid_dsc_array(grid, col_px, row_px);
    }

    static const char * keys[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "BK"
    };
    static const char * labels[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "DEL"
    };

    for(int i = 0; i < 12; i++) {
        /* Prototype colors */
        uint32_t bg = 0x111827;
        uint32_t border = 0x1F2937;
        uint32_t text = 0x22C55E;
        if(strcmp(keys[i], "BK") == 0) {
            bg = 0x111827;
            border = 0x991B1B;
            text = 0xF87171;
        }
        if(strcmp(keys[i], ".") == 0) {
            bg = 0x1F2937;
            border = 0x374151;
            text = 0xEAB308;
        }

        int col = i % 3;
        int row = i / 3;
        lv_obj_t * btn = make_key(grid, labels[i], keys[i], bg, border, text, col, row, fill_height);

        if(i == 9 && !show_decimal) {
            lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
        }
    }

    return *kp;
}

void ui_keypad_bind(char * value_buf, int max_len, const char * placeholder)
{
    s_kp_singleton.value_buf = value_buf;
    s_kp_singleton.max_len = max_len;
    s_kp_singleton.placeholder = placeholder;
    refresh_display(&s_kp_singleton);
}

void ui_keypad_release(void)
{
    s_intercept = NULL;
    s_change_cb = NULL;
    s_kp_singleton.value_buf = s_safe_buf;
    s_kp_singleton.max_len = 0;
    s_safe_buf[0] = '\0';
    s_kp_singleton.placeholder = "";
    s_kp_singleton.display_lbl = NULL;
    s_kp_singleton.root = NULL;
}

void ui_keypad_set_intercept(ui_keypad_intercept_fn fn)
{
    s_intercept = fn;
}

void ui_keypad_set_change_cb(ui_keypad_change_fn fn)
{
    s_change_cb = fn;
}

void ui_keypad_refresh(ui_keypad_t * kp)
{
    (void)kp;
    refresh_display(&s_kp_singleton);
}
