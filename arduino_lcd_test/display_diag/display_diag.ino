/*
 * Auto-cycle display init modes every 5 seconds.
 * Watch the screen + Serial Monitor (115200).
 * Note which step number shows SOLID red (not snow).
 */

#include <Arduino_GFX_Library.h>

#define GFX_BL  1
#define W       320
#define H       480

struct Mode {
    const char *name;
    int cs, clk, d0, d1, d2, d3;
    int initType; /* 0=type1, 1=type2, 2=default 180640 (wrong) */
    int hz;
    bool canvas;
};

static const Mode kModes[] = {
    {"0 Tactility type1 canvas 8MHz",  45, 47, 21, 48, 40, 39, 0,  8000000, true},
    {"1 Tactility type2 canvas 8MHz",  45, 47, 21, 48, 40, 39, 1,  8000000, true},
    {"2 Tactility type1 canvas 40MHz",45,47, 21, 48, 40, 39, 0, 40000000, true},
    {"3 Tactility type1 DIRECT 8MHz",  45, 47, 21, 48, 40, 39, 0,  8000000, false},
    {"4 Waveshare type1 canvas 8MHz",  12,  5,  1,  2,  3,  4, 0,  8000000, true},
};

static Arduino_DataBus *s_bus = nullptr;
static Arduino_GFX *s_panel = nullptr;
static Arduino_GFX *s_gfx = nullptr;

static Arduino_GFX *make_panel(Arduino_DataBus *bus, int initType)
{
    if (initType == 1) {
        return new Arduino_AXS15231B(
            bus, GFX_NOT_DEFINED, 0, false, W, H, 0, 0, 0, 0,
            axs15231b_320480_type2_init_operations,
            sizeof(axs15231b_320480_type2_init_operations));
    }
    if (initType == 0) {
        return new Arduino_AXS15231B(
            bus, GFX_NOT_DEFINED, 0, false, W, H, 0, 0, 0, 0,
            axs15231b_320480_type1_init_operations,
            sizeof(axs15231b_320480_type1_init_operations));
    }
    return new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, W, H);
}

static void cleanup_gfx(void)
{
    delete s_gfx;
    delete s_panel;
    delete s_bus;
    s_gfx = nullptr;
    s_panel = nullptr;
    s_bus = nullptr;
}

static bool run_mode(const Mode &m)
{
    cleanup_gfx();

    Serial.println();
    Serial.println(m.name);
    Serial.printf("PSRAM free=%u heap=%u\n", ESP.getFreePsram(), ESP.getFreeHeap());

    s_bus = new Arduino_ESP32QSPI(m.cs, m.clk, m.d0, m.d1, m.d2, m.d3);
    s_panel = make_panel(s_bus, m.initType);

    if (m.canvas) {
        s_gfx = new Arduino_Canvas(W, H, s_panel, 0, 0, 0);
        if (!s_gfx->begin(m.hz)) {
            Serial.println("begin FAIL");
            return false;
        }
    } else {
        if (!s_panel->begin(m.hz)) {
            Serial.println("begin FAIL");
            return false;
        }
        s_gfx = s_panel;
    }

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    s_gfx->fillScreen(RGB565_RED);
    if (m.canvas) {
        s_gfx->flush();
    }

    Serial.println(">>> Dapat SOLID RED ng 5 sec <<<");
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("display_diag — JC3248W535C");
    Serial.printf("PSRAM found=%d size=%u\n", psramFound(), ESP.getPsramSize());

    if (!psramFound()) {
        Serial.println("ERROR: Enable OPI PSRAM in Tools menu!");
    }
}

void loop()
{
    static size_t idx = 0;
    const Mode &m = kModes[idx % (sizeof(kModes) / sizeof(kModes[0]))];
    run_mode(m);
    delay(5000);
    idx++;
}
