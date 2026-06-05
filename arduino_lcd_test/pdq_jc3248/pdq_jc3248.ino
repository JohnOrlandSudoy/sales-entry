/*
 * Official Arduino_GFX PDQ test for JC3248W535 — upload THIS if snow persists.
 * Same as PDQgraphicstest with #define JC3248W535
 *
 * Tools: ESP32S3, OPI PSRAM, 16MB flash, USB CDC ON
 * Library: GFX_Library_for_Arduino 1.6.5+ in Documents/Arduino/libraries
 */

#include <Arduino_GFX_Library.h>

#define GFX_BL 1

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    45, 47, 21, 48, 40, 39);

Arduino_GFX *g = new Arduino_AXS15231B(
    bus, GFX_NOT_DEFINED, 0, false, 320, 480,
    0, 0, 0, 0,
    axs15231b_320480_type1_init_operations,
    sizeof(axs15231b_320480_type1_init_operations));

Arduino_Canvas *gfx = new Arduino_Canvas(320, 480, g, 0, 0, 0);

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("PDQ JC3248W535 — official GFX example");
    Serial.printf("PSRAM: %s (%u)\n", psramFound() ? "yes" : "NO",
                  ESP.getPsramSize());

    if (!gfx->begin(8000000)) {
        Serial.println("gfx->begin FAILED");
        return;
    }

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    gfx->fillScreen(RGB565_RED);
    gfx->flush();
    delay(1500);
    gfx->fillScreen(RGB565_GREEN);
    gfx->flush();
    delay(1500);
    gfx->fillScreen(RGB565_BLUE);
    gfx->flush();
    delay(1500);

    gfx->fillScreen(RGB565_BLACK);
    gfx->setCursor(20, 220);
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_WHITE);
    gfx->println("PDQ OK = solid colors");
    gfx->flush();

    Serial.println("If snow: check PSRAM + update GFX from GitHub");
}

void loop() {}
