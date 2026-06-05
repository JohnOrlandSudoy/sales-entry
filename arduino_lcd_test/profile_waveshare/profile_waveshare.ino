/*
 * Waveshare 3.5B demo — exact pin path (09_lvgl style, no LVGL).
 * If THIS looks clear but arduino_lcd_test does not → use BOARD_PROFILE 1.
 *
 * From: ESP32-S3-Touch-LCD-3.5B-Demo Arduino/examples/08_gfx style
 */

#include <Arduino_GFX_Library.h>
#include "TCA9554.h"

#define LCD_CS   12
#define LCD_CLK  5
#define LCD_D0   1
#define LCD_D1   2
#define LCD_D2   3
#define LCD_D3   4
#define GFX_BL   1

TCA9554 TCA(0x20);

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
Arduino_GFX *panel = new Arduino_AXS15231B(bus, -1, 0, false, 320, 480);
Arduino_Canvas *gfx = new Arduino_Canvas(320, 480, panel, 0, 0, 0);

void setup()
{
    Serial.begin(115200);
    delay(1500);
    Serial.println("profile_waveshare");

    Wire.begin(8, 7);
    TCA.begin();
    TCA.pinMode1(1, OUTPUT);
    TCA.write1(1, 1);
    delay(10);
    TCA.write1(1, 0);
    delay(10);
    TCA.write1(1, 1);
    delay(200);

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    if (!gfx->begin()) {
        Serial.println("begin fail");
        return;
    }

    gfx->fillScreen(RGB565_WHITE);
    gfx->flush();
    delay(1000);
    gfx->fillScreen(RGB565_RED);
    gfx->flush();
    delay(1000);

    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_GREEN);
    gfx->setTextSize(3);
    gfx->setCursor(20, 200);
    gfx->println("Waveshare OK");
    gfx->flush();
    Serial.println("done");
}

void loop() {}
