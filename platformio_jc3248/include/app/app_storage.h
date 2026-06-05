#ifndef APP_STORAGE_H
#define APP_STORAGE_H

#include "app_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Settings file location:
 * - PC simulator:  data/settings.cfg  (next to project / cwd)
 * - ESP32 + SD:    /sdcard/DailySales/settings.cfg
 */
const char * app_storage_settings_path(void);

bool app_storage_load_settings(app_settings_t * out);
bool app_storage_save_settings(const app_settings_t * settings);

bool app_storage_save_sales(const app_sales_entry_t * active, int active_count,
                            const app_sales_entry_t * archived, int archived_count);
bool app_storage_load_sales(app_sales_entry_t * active, int * active_count,
                            app_sales_entry_t * archived, int * archived_count);

/** Save settings + sales to SD card */
bool app_storage_save_all(const app_settings_t * settings,
                          const app_sales_entry_t * active, int active_count,
                          const app_sales_entry_t * archived, int archived_count);

/** True if last settings save went to microSD (not NVS fallback). */
bool app_storage_last_settings_on_sd(void);

/** Short reason when save failed completely. */
const char * app_storage_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
