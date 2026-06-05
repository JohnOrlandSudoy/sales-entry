#pragma once

/*
 * Guition JC3248W535C — Tactility pins
 * https://github.com/TactilityProject/Tactility/tree/main/Devices/guition-jc3248w535c
 *
 * BOARD_PROFILE 0 = JC3248W535C (default)
 * BOARD_PROFILE 1 = Waveshare 3.5B (wrong for Guition board)
 */
#define BOARD_PROFILE  0

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

#define BOARD_USE_TCA9554     0
#define BOARD_USE_CANVAS      1

/* 0 = type1 init, 1 = type2 init (try if snow persists) */
#define BOARD_INIT_TYPE       0

/* Lower = more stable. Try 8000000, 12000000, 40000000 */
#define BOARD_QSPI_HZ         8000000

/* PDQ example: backlight ON only after gfx->begin() */
#define BOARD_BL_AFTER_BEGIN  1

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
#define BOARD_USE_TCA9554     1
#define BOARD_USE_CANVAS      0
#define BOARD_INIT_TYPE       0
#define BOARD_QSPI_HZ         8000000
#define BOARD_BL_AFTER_BEGIN  0

#endif

#define BOARD_LCD_WIDTH       320
#define BOARD_LCD_HEIGHT      480
#define BOARD_BOOT_COLOR_TEST 1
