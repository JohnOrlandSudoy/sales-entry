#ifndef APP_TIME_H
#define APP_TIME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** TZ + seed RTC if invalid (call before app_data_init). */
void app_time_boot_init(void);

/** Start NTP when STA has IP (non-blocking; finish in app_time_poll). */
void app_time_on_wifi_connected(void);

/** Call from main loop — completes NTP and applies date clamp when sync succeeds. */
void app_time_poll(void);

bool app_time_is_valid(void);

#ifdef __cplusplus
}
#endif

#endif
