#include "display.h"
#include "board_config.h"
#include "app_display.h"
#include <cstring>

#if BOARD_USE_CANVAS
#include "canvas/Arduino_Canvas.h"
#endif

static Arduino_DataBus *s_bus = nullptr;
static Arduino_GFX *s_panel = nullptr;
static Arduino_GFX *s_gfx = nullptr;

#if defined(BOARD_GFX_ROTATION)
#define GFX_ROT  BOARD_GFX_ROTATION
#else
#define GFX_ROT  0
#endif

static Arduino_GFX *create_panel(Arduino_DataBus *bus)
{
#if defined(axs15231b_320480_type1_init_operations)
#if BOARD_INIT_TYPE == 1
    return new Arduino_AXS15231B(
        bus, GFX_NOT_DEFINED, 0, false,
        BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT,
        0, 0, 0, 0,
        axs15231b_320480_type2_init_operations,
        sizeof(axs15231b_320480_type2_init_operations));
#else
    return new Arduino_AXS15231B(
        bus, GFX_NOT_DEFINED, 0, false,
        BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT,
        0, 0, 0, 0,
        axs15231b_320480_type1_init_operations,
        sizeof(axs15231b_320480_type1_init_operations));
#endif
#else
    return new Arduino_AXS15231B(
        bus, GFX_NOT_DEFINED, 0, false,
        BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
#endif
}

void display_backlight_on(void)
{
    pinMode(BOARD_BACKLIGHT, OUTPUT);
#if BOARD_BL_ACTIVE_HIGH
    digitalWrite(BOARD_BACKLIGHT, HIGH);
#else
    digitalWrite(BOARD_BACKLIGHT, LOW);
#endif
}

void display_backlight_off(void)
{
    pinMode(BOARD_BACKLIGHT, OUTPUT);
#if BOARD_BL_ACTIVE_HIGH
    digitalWrite(BOARD_BACKLIGHT, LOW);
#else
    digitalWrite(BOARD_BACKLIGHT, HIGH);
#endif
}

uint16_t display_width(void)
{
    return s_gfx ? s_gfx->width() : 0;
}

uint16_t display_height(void)
{
    return s_gfx ? s_gfx->height() : 0;
}

bool display_begin(void)
{
    Serial.printf("[DISP] AXS15231B %dx%d init_type=%d qspi=%lu",
                  BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, BOARD_INIT_TYPE,
                  (unsigned long)BOARD_QSPI_HZ);
#if BOARD_USE_CANVAS
    Serial.printf(" mode=canvas rot=%d\n", GFX_ROT);
#else
    Serial.printf(" mode=direct rot=%d\n", GFX_ROT);
#endif

    if (!psramFound()) {
        Serial.println("[DISP] WARN: no PSRAM");
    } else {
        Serial.printf("[DISP] PSRAM %u\n", ESP.getPsramSize());
    }

    s_bus = new Arduino_ESP32QSPI(
        BOARD_LCD_CS, BOARD_LCD_CLK,
        BOARD_LCD_D0, BOARD_LCD_D1, BOARD_LCD_D2, BOARD_LCD_D3);
    s_panel = create_panel(s_bus);

    if (!s_panel->begin(BOARD_QSPI_HZ)) {
        Serial.println("[DISP] panel begin FAILED");
        return false;
    }

#if BOARD_USE_CANVAS
    /* Portrait sample path — Canvas framebuffer + rotation in ctor */
    s_gfx = new Arduino_Canvas(BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, s_panel, 0, 0, GFX_ROT);
    if (!s_gfx->begin(GFX_SKIP_OUTPUT_BEGIN)) {
        Serial.println("[DISP] canvas begin FAILED");
        return false;
    }
    auto *canvas = (Arduino_Canvas *)s_gfx;
    uint16_t *fb = canvas->getFramebuffer();
    if (!fb) {
        Serial.println("[DISP] framebuffer NULL");
        return false;
    }
    memset(fb, 0, (size_t)BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * sizeof(uint16_t));
#else
    /* Direct panel (legacy A/B env only) */
    s_panel->setRotation(GFX_ROT);
    s_gfx = s_panel;
#endif

    const int lw = s_gfx->width();
    const int lh = s_gfx->height();
    Serial.printf("[DISP] logical %dx%d (expect %dx%d)\n", lw, lh, APP_SCREEN_W, APP_SCREEN_H);

    if (lw != APP_SCREEN_W || lh != APP_SCREEN_H) {
        Serial.println("[DISP] ERROR W/H mismatch");
        return false;
    }

    s_gfx->fillScreen(0x0000);
#if BOARD_USE_CANVAS
    s_gfx->flush();
#endif

    display_backlight_on();
    return true;
}

Arduino_GFX *display_get_gfx(void)
{
    return s_gfx;
}

void display_flush(void)
{
    if (!s_gfx) {
        return;
    }
#if BOARD_USE_CANVAS
    s_gfx->flush();
#else
    /* Direct panel: pixels already written by draw16bit* in flush_cb */
#endif
}

void display_touch_wire_begin(void)
{
    Wire.begin(BOARD_TOUCH_I2C_SDA, BOARD_TOUCH_I2C_SCL);
    Wire.setClock(400000);
}
