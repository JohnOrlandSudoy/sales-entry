#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Mount SD card at /sdcard (FAT). Call before app_data_init() for sales/history persistence. */
bool app_sd_mount(void);

bool app_sd_is_mounted(void);

/** Force remount (use before save if card was inserted late). */
bool app_sd_remount(void);

/** User save / settings — bypass 60s "skip remount" cooldown. */
bool app_sd_mount_retry_user(void);

/** Last mount/write error for UI or serial logs. */
const char * app_sd_last_error(void);

/** Short message for on-screen toast (mounted / missing / bad format). */
const char * app_sd_user_message(void);

/** Prepare writable data folder; sets app_sd_data_dir(). */
bool app_sd_ensure_data_dir(void);

/** Active data directory: "/sdcard/DailySales" or "/sdcard" fallback. */
const char * app_sd_data_dir(void);

#ifdef __cplusplus
}
#endif
