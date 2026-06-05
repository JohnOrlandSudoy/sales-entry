#include "lv_port.h"
#include "board_config.h"
#include "hardware.h"
#include "esp_lcd_touch_axs15231b.h"
#include <Wire.h>
#include <esp_heap_caps.h>

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
    Arduino_GFX *gfx = board_get_gfx();
    if (!gfx) {
        lv_disp_flush_ready(drv);
        return;
    }

    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;

#if LV_COLOR_16_SWAP != 0
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    board_display_flush();
    lv_disp_flush_ready(drv);
}

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    touch_data_t touch;

    bsp_touch_read();

    if (bsp_touch_get_coordinates(&touch)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.coords[0].x;
        data->point.y = touch.coords[0].y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

bool lv_port_init(void)
{
    Arduino_GFX *gfx = board_get_gfx();
    if (!gfx) {
        return false;
    }

    lv_init();

    const uint32_t w = gfx->width();
    const uint32_t h = gfx->height();
    const uint32_t buf_pixels = w * h;

    Serial.printf("[LVGL] Full buffer %ux%u in PSRAM\n", w, h);

    const size_t buf_bytes = buf_pixels * sizeof(lv_color_t);
    s_buf1 = (lv_color_t *)lv_alloc(buf_bytes);
    s_buf2 = (lv_color_t *)lv_alloc(buf_bytes);
    if (!s_buf1 || !s_buf2) {
        Serial.println("[LVGL] Buffer alloc failed");
        return false;
    }

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, buf_pixels);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = w;
    s_disp_drv.ver_res = h;
    s_disp_drv.flush_cb = disp_flush;
    s_disp_drv.draw_buf = &s_draw_buf;
    s_disp_drv.full_refresh = true;
    lv_disp_drv_register(&s_disp_drv);

    board_touch_wire_begin();
    bsp_touch_init(&Wire, -1, 0, w, h);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    Serial.println("[LVGL] Ready");
    return true;
}
