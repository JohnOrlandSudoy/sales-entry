#include "hardware.h"
#include "board_config.h"
#include <Wire.h>
#include <esp_heap_caps.h>

#if BOARD_USE_TCA9554
#include "TCA9554.h"
static TCA9554 s_tca(0x20);
#endif

static Arduino_DataBus *s_bus = nullptr;
static Arduino_GFX *s_panel = nullptr;
static Arduino_GFX *s_gfx = nullptr;

static Arduino_GFX *create_panel(Arduino_DataBus *bus)
{
#if BOARD_PROFILE == 0
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

static void backlight_on(void)
{
    pinMode(BOARD_BACKLIGHT, OUTPUT);
    digitalWrite(BOARD_BACKLIGHT, HIGH);
}

#if BOARD_USE_TCA9554
static void tca_lcd_reset(void)
{
    Wire.begin(BOARD_TOUCH_I2C_SDA, BOARD_TOUCH_I2C_SCL);
    if (s_tca.begin()) {
        s_tca.pinMode1(1, OUTPUT);
        s_tca.write1(1, 1);
        delay(10);
        s_tca.write1(1, 0);
        delay(10);
        s_tca.write1(1, 1);
        delay(200);
        Serial.println("[HW] TCA9554 reset done");
    } else {
        Serial.println("[HW] TCA9554 not found");
    }
}
#endif

static void print_heap(void)
{
    Serial.printf("[HW] heap=%u largest=%u psram=%u free_psram=%u\n",
                  ESP.getFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  ESP.getPsramSize(),
                  ESP.getFreePsram());
}

static void solid_test(Arduino_GFX *gfx)
{
    gfx->fillScreen(RGB565_RED);
#if BOARD_USE_CANVAS
    gfx->flush();
#endif
    delay(600);
    gfx->fillScreen(RGB565_GREEN);
#if BOARD_USE_CANVAS
    gfx->flush();
#endif
    delay(600);
    gfx->fillScreen(RGB565_BLACK);
#if BOARD_USE_CANVAS
    gfx->flush();
#endif
}

bool board_hardware_init(void)
{
    Serial.println("[HW] JC3248W535C init");
    Serial.printf("[HW] profile=%d init_type=%d qspi=%dHz canvas=%d\n",
                  BOARD_PROFILE, BOARD_INIT_TYPE, BOARD_QSPI_HZ, BOARD_USE_CANVAS);
    Serial.printf("[HW] PSRAM %s\n", psramFound() ? "OK" : "MISSING — set OPI PSRAM");
    print_heap();

#if !BOARD_BL_AFTER_BEGIN
    backlight_on();
#endif

#if BOARD_USE_TCA9554
    tca_lcd_reset();
#endif

    s_bus = new Arduino_ESP32QSPI(
        BOARD_LCD_CS, BOARD_LCD_CLK,
        BOARD_LCD_D0, BOARD_LCD_D1, BOARD_LCD_D2, BOARD_LCD_D3);

    s_panel = create_panel(s_bus);

#if BOARD_USE_CANVAS
    s_gfx = new Arduino_Canvas(
        BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, s_panel, 0, 0, 0);
    if (!s_gfx->begin(BOARD_QSPI_HZ)) {
        Serial.println("[HW] Canvas begin FAILED");
        return false;
    }
#else
    if (!s_panel->begin(BOARD_QSPI_HZ)) {
        Serial.println("[HW] Panel begin FAILED");
        return false;
    }
    s_gfx = s_panel;
#endif

#if BOARD_BL_AFTER_BEGIN
    backlight_on();
    Serial.printf("[HW] Backlight GPIO %d (after begin)\n", BOARD_BACKLIGHT);
#endif

    Serial.printf("[HW] QSPI CS=%d CLK=%d D0..3=%d,%d,%d,%d\n",
                  BOARD_LCD_CS, BOARD_LCD_CLK,
                  BOARD_LCD_D0, BOARD_LCD_D1, BOARD_LCD_D2, BOARD_LCD_D3);

    print_heap();

#if BOARD_BOOT_COLOR_TEST
    solid_test(s_gfx);
    Serial.println("[HW] RED/GREEN test — dapat solid, hindi snow");
#endif

    return true;
}

Arduino_GFX *board_get_gfx(void)
{
    return s_gfx;
}

void board_display_flush(void)
{
#if BOARD_USE_CANVAS
    if (s_gfx) {
        s_gfx->flush();
    }
#endif
}

void board_touch_wire_begin(void)
{
    Wire.begin(BOARD_TOUCH_I2C_SDA, BOARD_TOUCH_I2C_SCL);
    Wire.setClock(400000);
}
