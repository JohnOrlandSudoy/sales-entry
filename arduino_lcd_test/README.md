# Guition JC3248W535C — Arduino test

Pins from [Tactility `guition-jc3248w535c`](https://github.com/TactilityProject/Tactility/tree/main/Devices/guition-jc3248w535c).

## Pin map (profile 0 — default)

| Function | GPIO |
|----------|------|
| Backlight | **1** |
| QSPI CS / CLK / D0–D3 | **45, 47, 21, 48, 40, 39** |
| Touch I2C SDA / SCL | **4, 8** |
| TCA9554 | **not used** |

## Arduino IDE settings

- Board: **ESP32S3 Dev Module**
- PSRAM: **OPI PSRAM** (required)
- Flash: **16MB**
- USB CDC On Boot: **Enabled**
- LVGL **8.4.0** — copy `lv_conf.h` → `Documents/Arduino/libraries/lvgl/`

## Upload order

1. **`bare_colors/bare_colors.ino`** — solid colors (no LVGL)
2. **`arduino_lcd_test.ino`** — full UI

## If still wrong

- Confirm `board_config.h` has `#define BOARD_PROFILE 0`
- Do **not** use Waveshare profile 1 on JC3248W535C
- Rename `GFX_Library_for_Arduino/src/databus/Arduino_ESP32LCD8.cpp` → `.bak` if compile fails

## Reference

- Tactility device tree: `guition,jc3248w535c.dts`
- Arduino_GFX: `#define JC3248W535` in PDQgraphicstest
