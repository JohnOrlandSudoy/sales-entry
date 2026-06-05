# Paano i-upload ang arduino_lcd_test (Arduino IDE)

## Hakbang 1 — Install Arduino IDE

Download: https://www.arduino.cc/en/software (version 2.x)

## Hakbang 2 — ESP32 board support

1. **File → Preferences**
2. **Additional boards manager URLs** — idagdag:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. **Tools → Board → Boards Manager** → hanapin **esp32** → **Install**

## Hakbang 3 — Copy libraries (isang beses lang)

Copy lahat mula sa Waveshare zip:

```
C:\Users\ADMIN\Downloads\ESP32-S3-Touch-LCD-3.5B-Demo\Arduino\libraries\
```

Papunta sa:

```
C:\Users\ADMIN\Documents\Arduino\libraries\
```

Kailangan: `GFX_Library_for_Arduino`, `TCA9554`, `esp_lcd_touch_axs15231b`

### LVGL 8.4 (importante)

1. Sa `Documents\Arduino\libraries`, i-rename ang `lvgl` folder → `lvgl9_backup` (kung may LVGL 9)
2. **Sketch → Include Library → Manage Libraries** → install **lvgl** version **8.4.0**
3. Copy `lv_conf.h` mula sa project folder papunta sa:
   ```
   Documents\Arduino\libraries\lvgl\lv_conf.h
   ```
   (overwrite)

## Hakbang 4 — Kung SNOW / static ang screen

**Una i-upload** ang diagnostic (5 sec bawat mode, solid RED):

```
arduino_lcd_test\display_diag\display_diag.ino
```

Buksan **Serial Monitor 115200** — sabihin kung aling step (0–4) ang **solid red** (hindi snow).

O subukan ang official PDQ clone:

```
arduino_lcd_test\pdq_jc3248\pdq_jc3248.ino
```

Pag solid colors na:

```
arduino_lcd_test\bare_colors\bare_colors.ino
```

Tapos:

```
arduino_lcd_test\arduino_lcd_test.ino
```

Siguraduhing `board_config.h` → `BOARD_PROFILE 0`, **PSRAM = OPI PSRAM**.

## Hakbang 5 — Board settings (Tools menu)

| Setting | Value |
|---------|--------|
| Board | **ESP32S3 Dev Module** |
| USB CDC On Boot | **Enabled** |
| CPU Frequency | **240MHz** |
| Flash Size | **16MB (128Mb)** |
| Partition Scheme | **16M Flash (3MB APP / 9.9MB FATFS)** |
| PSRAM | **OPI PSRAM** |
| Upload Speed | **921600** |
| Port | **COM30** (o kung ano lumabas) |

## Hakbang 6 — Upload

1. Ikonekta ang board sa USB
2. **Isara** ang ESP-IDF monitor kung bukas (same COM port)
3. Click ang **→ Upload** button
4. Kung stuck sa "Connecting...":
   - Hold **BOOT** button
   - Click Upload
   - Bitawan **BOOT** pag "Writing..." na

## Hakbang 7 — Tingnan ang resulta

**Screen:** "DIYMORE LCD Test", button, bar, switch

**Serial Monitor** (Tools → Serial Monitor, **115200** baud):
```
=== arduino_lcd_test ===
[HW] Display 320x480 ready
[LVGL] Port ready
UI ready
```

## Kung blur / snow ang screen

- **PSRAM = OPI PSRAM** (kung wala, snow ang canvas)
- Backlight **pagkatapos** ng `gfx->begin()` (tulad ng PDQ example)
- Sa `board_config.h` subukan: `BOARD_INIT_TYPE 1` o `BOARD_QSPI_HZ 12000000`
- I-update ang GFX library mula sa GitHub kung luma pa ang version
- Huwag `BOARD_PROFILE 1` (Waveshare) sa JC3248W535C

## Kung compile error

| Error | Fix |
|-------|-----|
| TCA9554.h not found | Kulang libraries — Hakbang 3 |
| lv_disp_drv_t not found | Install LVGL **8.4**, hindi 9 |
| Multiple libraries lvgl | Tanggalin duplicate sa libraries folder |
