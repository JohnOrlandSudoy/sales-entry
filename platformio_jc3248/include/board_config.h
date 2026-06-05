#pragma once

#ifndef BOARD_PROFILE
#define BOARD_PROFILE 0
#endif

#ifndef BOARD_INIT_TYPE
#define BOARD_INIT_TYPE 0
#endif

#ifndef BOARD_QSPI_HZ
#define BOARD_QSPI_HZ 8000000
#endif

/* 1 = Arduino_Canvas framebuffer (portrait sample). 0 = direct AXS15231B + setRotation (landscape). */
#ifndef BOARD_USE_CANVAS
#define BOARD_USE_CANVAS 1
#endif

#ifndef BOARD_BOOT_COLOR_TEST
#define BOARD_BOOT_COLOR_TEST 1
#endif

#if BOARD_PROFILE == 0

#define BOARD_LCD_CS          45
#define BOARD_LCD_CLK         47
#define BOARD_LCD_D0          21
#define BOARD_LCD_D1          48
#define BOARD_LCD_D2          40
#define BOARD_LCD_D3          39
#define BOARD_BACKLIGHT       1
#define BOARD_TOUCH_I2C_SDA   4
#define BOARD_TOUCH_I2C_SCL   8
#define BOARD_TOUCH_INT       3
#define BOARD_USE_TCA9554     0

#else

#define BOARD_LCD_CS          12
#define BOARD_LCD_CLK         5
#define BOARD_LCD_D0          1
#define BOARD_LCD_D1          2
#define BOARD_LCD_D2          3
#define BOARD_LCD_D3          4
#define BOARD_BACKLIGHT       6
#define BOARD_TOUCH_I2C_SDA   8
#define BOARD_TOUCH_I2C_SCL   7
#define BOARD_USE_TCA9554     0

#endif

#define BOARD_LCD_WIDTH       320
#define BOARD_LCD_HEIGHT      480

/* TF/microSD — Guition JC3248W535 / JC3248W535C (SPI, slot sa likod ng board)
 * CS=10, MOSI=11, MISO=13, SCK=12 — FAT32 card lang, hanggang 32GB recommended */
#define BOARD_SD_CS           10
#define BOARD_SD_MOSI         11
#define BOARD_SD_MISO         13
#define BOARD_SD_SCK          12

/* 1 = HIGH/on, 0 = try active-low if screen stays black */
#define BOARD_BL_ACTIVE_HIGH  1
