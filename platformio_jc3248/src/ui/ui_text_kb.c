#include "ui_text_kb.h"
#include "app_display.h"
#include <string.h>
#include <ctype.h>

#if LV_FONT_MONTSERRAT_10
  #define FONT_KEY  (&lv_font_montserrat_10)
#else
  #define FONT_KEY  (&lv_font_montserrat_12)
#endif

#define KEY_GAP   2
#define KEY_COLS  10
#define KEY_ROWS  5

#define COL_BG      0x111827
#define COL_KEY     0x1F2937
#define COL_BORDER  0x374151
#define COL_GREEN   0x22C55E
#define COL_YELLOW  0xEAB308
#define COL_CYAN    0x22D3EE
#define COL_RED     0xF87171

typedef struct {
    lv_obj_t * panel;
    lv_obj_t * grid;
    lv_obj_t * ta;
    bool upper;
    ui_text_kb_close_cb_t close_cb;
    void * close_ud;
} tk_state_t;

static tk_state_t s;

static int key_w(void)
{
    return (APP_SCREEN_W - 8 - KEY_GAP * (KEY_COLS - 1)) / KEY_COLS;
}

static int key_h(void)
{
    return (UI_TEXT_KB_HEIGHT - 8 - KEY_GAP * (KEY_ROWS - 1)) / KEY_ROWS;
}

static void insert_char(char c)
{
    if(!s.ta || !lv_obj_is_valid(s.ta)) return;
    if(s.upper) c = (char)toupper((unsigned char)c);
    lv_textarea_add_char(s.ta, c);
}

static void key_cb(lv_event_t * e)
{
    const char * key = lv_event_get_user_data(e);
    if(!key || !s.ta) return;

    if(strcmp(key, "BK") == 0) {
        lv_textarea_del_char(s.ta);
        return;
    }
    if(strcmp(key, "SH") == 0) {
        s.upper = !s.upper;
        return;
    }
    if(strcmp(key, "OK") == 0) {
        if(s.close_cb) s.close_cb(s.close_ud);
        ui_text_kb_set_visible(s.panel, false);
        return;
    }
    if(strcmp(key, "SP") == 0) {
        lv_textarea_add_char(s.ta, ' ');
        return;
    }
    if(key[0] && key[1] == '\0')
        insert_char(key[0]);
}

static lv_obj_t * place_key(lv_obj_t * parent, const char * label, const char * key_id,
                            int col, int row, int span, uint32_t text_col)
{
    int kw = key_w();
    int kh = key_h();
    int x = col * (kw + KEY_GAP);
    int y = row * (kh + KEY_GAP);
    int w = span * kw + (span - 1) * KEY_GAP;

    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, kh);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_KEY), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_event_cb(btn, key_cb, LV_EVENT_CLICKED, (void *)key_id);

    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(text_col), 0);
    lv_obj_set_style_text_font(lbl, FONT_KEY, 0);
    lv_obj_center(lbl);
    return btn;
}

static void build_keys(lv_obj_t * grid)
{
    lv_obj_clean(grid);
    lv_obj_set_size(grid, APP_SCREEN_W - 8, UI_TEXT_KB_HEIGHT - 8);

    static const char * r0[] = {"1","2","3","4","5","6","7","8","9","0"};
    static const char * r1[] = {"q","w","e","r","t","y","u","i","o","p"};
    static const char * r2[] = {"a","s","d","f","g","h","j","k","l"};
    static const char * r3[] = {"z","x","c","v","b","n","m"};

    for(int i = 0; i < 10; i++)
        place_key(grid, r0[i], r0[i], i, 0, 1, COL_GREEN);
    for(int i = 0; i < 10; i++)
        place_key(grid, r1[i], r1[i], i, 1, 1, COL_GREEN);
    for(int i = 0; i < 9; i++)
        place_key(grid, r2[i], r2[i], i, 2, 1, COL_GREEN);
    place_key(grid, LV_SYMBOL_BACKSPACE, "BK", 9, 2, 1, COL_RED);

    place_key(grid, "ABC", "SH", 0, 3, 1, COL_YELLOW);
    for(int i = 0; i < 7; i++)
        place_key(grid, r3[i], r3[i], i + 1, 3, 1, COL_GREEN);
    place_key(grid, "@", "@", 8, 3, 1, COL_CYAN);
    place_key(grid, ".", ".", 9, 3, 1, COL_CYAN);

    place_key(grid, "space", "SP", 0, 4, 7, COL_GREEN);
    place_key(grid, LV_SYMBOL_OK, "OK", 7, 4, 3, COL_GREEN);
}

lv_obj_t * ui_text_kb_create(lv_obj_t * parent)
{
    memset(&s, 0, sizeof(s));

    s.panel = lv_obj_create(parent);
    lv_obj_set_width(s.panel, APP_SCREEN_W);
    lv_obj_set_height(s.panel, UI_TEXT_KB_HEIGHT);
    lv_obj_align(s.panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s.panel, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(s.panel, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_bg_color(s.panel, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s.panel, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(s.panel, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(s.panel, 1, 0);
    lv_obj_set_style_pad_all(s.panel, 4, 0);
    lv_obj_set_style_radius(s.panel, 0, 0);
    lv_obj_clear_flag(s.panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s.panel, LV_OBJ_FLAG_HIDDEN);

    s.grid = lv_obj_create(s.panel);
    lv_obj_set_style_bg_opa(s.grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s.grid, 0, 0);
    lv_obj_set_style_pad_all(s.grid, 0, 0);
    lv_obj_clear_flag(s.grid, LV_OBJ_FLAG_SCROLLABLE);

    build_keys(s.grid);
    return s.panel;
}

void ui_text_kb_set_textarea(lv_obj_t * panel, lv_obj_t * ta)
{
    (void)panel;
    s.ta = ta;
}

void ui_text_kb_set_visible(lv_obj_t * panel, bool visible)
{
    if(!panel) return;
    if(visible) {
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(panel);
    }
    else {
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
        s.ta = NULL;
    }
}

void ui_text_kb_set_uppercase(lv_obj_t * panel, bool upper)
{
    (void)panel;
    s.upper = upper;
}

void ui_text_kb_set_close_cb(lv_obj_t * panel, ui_text_kb_close_cb_t cb, void * user_data)
{
    (void)panel;
    s.close_cb = cb;
    s.close_ud = user_data;
}
