/**
 * Portrait sample UI — known-good path on JC3248 (no rotation, smaller than ui_sales.c).
 */
#include "ui/ui_sales_sample.h"
#include "ui/ui_colors.h"
#include "app_display.h"

#define SAMPLE_HDR_H  36

void ui_sales_sample_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* Header */
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_obj_set_height(hdr, SAMPLE_HDR_H);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "Sales Entry — Portrait Test");
    lv_obj_set_style_text_color(title, lv_color_hex(COL_CYAN), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_center(title);

    /* Employee row */
    lv_obj_t *emp_row = lv_obj_create(parent);
    lv_obj_set_width(emp_row, LV_PCT(100));
    lv_obj_set_height(emp_row, 40);
    lv_obj_set_style_bg_opa(emp_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(emp_row, 0, 0);
    lv_obj_set_style_pad_all(emp_row, 0, 0);
    lv_obj_clear_flag(emp_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *emp_lbl = lv_label_create(emp_row);
    lv_label_set_text(emp_lbl, "Employee:");
    lv_obj_set_style_text_color(emp_lbl, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_align(emp_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *emp_dd = lv_dropdown_create(emp_row);
    lv_dropdown_set_options(emp_dd, "John\nHoward lee");
    lv_obj_set_width(emp_dd, 160);
    lv_obj_align(emp_dd, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Barcode */
    lv_obj_t *bc_lbl = lv_label_create(parent);
    lv_label_set_text(bc_lbl, "Barcode");
    lv_obj_set_style_text_color(bc_lbl, lv_color_hex(COL_CYAN), 0);

    lv_obj_t *bc_btn = lv_btn_create(parent);
    lv_obj_set_width(bc_btn, LV_PCT(100));
    lv_obj_set_height(bc_btn, 36);
    lv_obj_set_style_bg_color(bc_btn, lv_color_hex(COL_PANEL2), 0);
    lv_obj_t *bc_txt = lv_label_create(bc_btn);
    lv_label_set_text(bc_txt, "0000000000000");
    lv_obj_set_style_text_color(bc_txt, lv_color_hex(COL_GREEN), 0);
    lv_obj_center(bc_txt);

    /* Grand total */
    lv_obj_t *gt = lv_label_create(parent);
    lv_label_set_text(gt, "Grand Total: P0.00");
    lv_obj_set_style_text_color(gt, lv_color_hex(COL_YELLOW), 0);
    lv_obj_set_style_text_font(gt, &lv_font_montserrat_14, 0);

    /* Buttons */
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 40);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *add = lv_btn_create(row);
    lv_obj_set_flex_grow(add, 1);
    lv_obj_set_style_bg_color(add, lv_color_hex(COL_CYAN_DIM), 0);
    lv_obj_t *add_l = lv_label_create(add);
    lv_label_set_text(add_l, "ADD ITEM");
    lv_obj_center(add_l);

    lv_obj_t *rev = lv_btn_create(row);
    lv_obj_set_flex_grow(rev, 1);
    lv_obj_set_style_bg_color(rev, lv_color_hex(COL_GREEN_DIM), 0);
    lv_obj_t *rev_l = lv_label_create(rev);
    lv_label_set_text(rev_l, "REVIEW");
    lv_obj_center(rev_l);

    /* Keypad hint */
    lv_obj_t *kp = lv_obj_create(parent);
    lv_obj_set_width(kp, LV_PCT(100));
    lv_obj_set_flex_grow(kp, 1);
    lv_obj_set_style_bg_color(kp, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_border_color(kp, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(kp, 1, 0);
    lv_obj_set_flex_flow(kp, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(kp, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(kp, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *kp_lbl = lv_label_create(kp);
    lv_label_set_text(kp_lbl, "Keypad area\n(portrait sample OK)");
    lv_obj_set_style_text_color(kp_lbl, lv_color_hex(COL_GRAY_TEXT), 0);
    lv_obj_set_style_text_align(kp_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *msg = lv_label_create(parent);
    lv_label_set_text(msg, "JC3248 portrait — upload jc3248_portrait");
    lv_obj_set_style_text_color(msg, lv_color_hex(COL_GREEN), 0);
}
