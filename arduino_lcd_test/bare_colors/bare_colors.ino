/*
 * JC3248W535C — solid colors, Tactility pins, type1, 8MHz
 * Backlight AFTER begin (same as PDQgraphicstest)
 */

#include <Arduino_GFX_Library.h>

#define GFX_BL   1
#define QSPI_HZ  8000000

Arduino_DataBus *bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);

Arduino_GFX *panel = new Arduino_AXS15231B(
    bus, GFX_NOT_DEFINED, 0, false, 320, 480,
    0, 0, 0, 0,
    axs15231b_320480_type1_init_operations,
    sizeof(axs15231b_320480_type1_init_operations));

Arduino_Canvas *gfx = new Arduino_Canvas(320, 480, panel, 0, 0, 0);

void show(uint16_t color, const char *name)
{
    gfx->fillScreen(color);
    gfx->flush();
    Serial.println(name);
    delay(2000);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("bare_colors JC3248W535C");
    Serial.printf("PSRAM: %s (%u bytes free)\n",
                  psramFound() ? "OK" : "MISSING",
                  ESP.getFreePsram());

    if (!gfx->begin(QSPI_HZ)) {
        Serial.println("begin FAIL");
        return;
    }

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    show(RGB565_WHITE, "WHITE");
    show(RGB565_RED, "RED");
    show(RGB565_GREEN, "GREEN");
    show(RGB565_BLUE, "BLUE");

    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(30, 220);
    gfx->println("OK");
    gfx->flush();
}

void loop() {}
