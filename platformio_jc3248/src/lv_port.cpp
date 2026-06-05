#include "lv_port.h"
#include "display.h"
#include "board_config.h"
#include "app_display.h"
#include <lvgl.h>
#include "esp_lcd_touch_axs15231b.h"
#include "app_power.h"
#include <Wire.h>
#include <esp_heap_caps.h>

#ifndef LVGL_BUF_LINES
#define LVGL_BUF_LINES 60
#endif

#ifndef LVGL_FULL_REFRESH
#define LVGL_FULL_REFRESH 0
#endif

static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_color_t *s_buf1 = nullptr;
static lv_color_t *s_buf2 = nullptr;

static void *lv_alloc(size_t bytes)
{
    void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    }
    return p;
}

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    Arduino_GFX *gfx = display_get_gfx();
    if (!gfx) {
        lv_disp_flush_ready(drv);
        return;
    }

    const int32_t sw = gfx->width();
    const int32_t sh = gfx->height();

    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= sw) x2 = sw - 1;
    if (y2 >= sh) y2 = sh - 1;
    if (x2 < x1 || y2 < y1) {
        lv_disp_flush_ready(drv);
        return;
    }

    const int32_t w = x2 - x1 + 1;
    const int32_t h = y2 - y1 + 1;

#if LV_COLOR_16_SWAP != 0
    gfx->draw16bitBeRGBBitmap(x1, y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(x1, y1, (uint16_t *)&color_p->full, w, h);
#endif

#if BOARD_USE_CANVAS
    if (lv_disp_flush_is_last(drv)) {
        display_flush();
    }
#endif

    lv_disp_flush_ready(drv);
}

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    touch_data_t touch;
    bsp_touch_read();
    if (bsp_touch_get_coordinates(&touch)) {
        app_idle_feed();
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.coords[0].x;
        data->point.y = touch.coords[0].y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

bool lv_port_init(void)
{
    Arduino_GFX *gfx = display_get_gfx();
    if (!gfx) {
        return false;
    }

    lv_init();

    const uint32_t w = display_width();
    const uint32_t h = display_height();
    if (w != APP_SCREEN_W || h != APP_SCREEN_H) {
        Serial.printf("[LVGL] ERROR size %ux%u != APP %ux%u\n", w, h, APP_SCREEN_W, APP_SCREEN_H);
        return false;
    }

#if LVGL_FULL_REFRESH
    const uint32_t buf_pixels = w * h;
    Serial.printf("[LVGL] full frame %ux%u (%.1f KB x2)\n",
                  w, h, (double)(buf_pixels * sizeof(lv_color_t)) / 1024.0);
#else
    const uint32_t buf_lines = LVGL_BUF_LINES;
    const uint32_t buf_pixels = w * buf_lines;
    Serial.printf("[LVGL] partial %ux%u lines=%lu (%.1f KB x2)\n",
                  w, h, (unsigned long)buf_lines,
                  (double)(buf_pixels * sizeof(lv_color_t)) / 1024.0);
#endif

    const size_t buf_bytes = buf_pixels * sizeof(lv_color_t);

    s_buf1 = (lv_color_t *)lv_alloc(buf_bytes);
    s_buf2 = (lv_color_t *)lv_alloc(buf_bytes);
    if (!s_buf1 || !s_buf2) {
        Serial.println("[LVGL] buffer alloc fail");
        return false;
    }

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, buf_pixels);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = w;
    s_disp_drv.ver_res = h;
    s_disp_drv.flush_cb = disp_flush;
    s_disp_drv.draw_buf = &s_draw_buf;
#if LVGL_FULL_REFRESH
    s_disp_drv.full_refresh = 1;
#else
    s_disp_drv.full_refresh = 0;
#endif
    s_disp_drv.sw_rotate = 0;
    lv_disp_drv_register(&s_disp_drv);

    display_touch_wire_begin();
#if defined(BOARD_GFX_ROTATION)
    bsp_touch_init(&Wire, -1, BOARD_GFX_ROTATION, w, h);
#else
    bsp_touch_init(&Wire, -1, 0, w, h);
#endif

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

#if LVGL_FULL_REFRESH
    Serial.println("[LVGL] ready (full_refresh)");
#else
    Serial.println("[LVGL] ready (partial refresh)");
#endif
    return true;
}
