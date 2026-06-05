#pragma once

/**
 * Logical UI size — MUST match gfx->width()/height() after any rotation.
 * JC3248 panel hardware is always 320x480 native (see board_config.h).
 * Device default: portrait 320x480, no rotation.
 */
#ifndef APP_SCREEN_W
#define APP_SCREEN_W  320
#endif
#ifndef APP_SCREEN_H
#define APP_SCREEN_H  480
#endif

#ifndef APP_EMAIL_SIMULATE
#define APP_EMAIL_SIMULATE  0
#endif

/* LVGL 8 (ESP32) vs LVGL 9 (PC sim) — label long-mode enum names */
#ifndef LV_LABEL_LONG_WRAP
  #ifdef LV_LABEL_LONG_MODE_WRAP
    #define LV_LABEL_LONG_WRAP LV_LABEL_LONG_MODE_WRAP
  #endif
#endif
#ifndef LV_LABEL_LONG_DOT
  #ifdef LV_LABEL_LONG_MODE_DOTS
    #define LV_LABEL_LONG_DOT LV_LABEL_LONG_MODE_DOTS
  #elif defined(LV_LABEL_LONG_MODE_DOT)
    #define LV_LABEL_LONG_DOT LV_LABEL_LONG_MODE_DOT
  #endif
#endif

#ifndef APP_HDR_H
#if defined(APP_SCREEN_H) && defined(APP_SCREEN_W) && (APP_SCREEN_H > APP_SCREEN_W)
#define APP_HDR_H  48
#else
#define APP_HDR_H  32
#endif
#endif

#if defined(APP_SCREEN_H) && defined(APP_SCREEN_W) && (APP_SCREEN_H > APP_SCREEN_W)
#define APP_PORTRAIT  1
#else
#define APP_PORTRAIT  0
#endif
