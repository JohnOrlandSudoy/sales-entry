/*
 * JC3248W535C — direct panel (no LVGL). Tactility pins + type1 init.
 */

#include <Arduino_GFX_Library.h>

#define LCD_CS   45
#define LCD_CLK  47
#define LCD_D0   21
#define LCD_D1   48
#define LCD_D2   40
#define LCD_D3   39
#define GFX_BL   1

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);

Arduino_GFX *gfx = new Arduino_AXS15231B(
    bus, GFX_NOT_DEFINED, 0, false, 320, 480,
    0, 0, 0, 0,
    axs15231b_320480_type1_init_operations,
    sizeof(axs15231b_320480_type1_init_operations));

void setup()
{
    Serial.begin(115200);
    delay(1500);
    Serial.println("guition_minimal type1 (Tactility pins)");
    Serial.printf("PSRAM: %s\n", psramFound() ? "yes" : "NO - enable OPI PSRAM");

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    if (!gfx->begin()) {
        Serial.println("begin FAILED");
        return;
    }

    gfx->fillScreen(RGB565_RED);
    delay(1000);
    gfx->fillScreen(RGB565_GREEN);
    delay(1000);
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(30, 220);
    gfx->println("JC3248W535C");
}

void loop() {}
