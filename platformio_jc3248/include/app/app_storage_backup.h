#ifndef APP_STORAGE_BACKUP_H
#define APP_STORAGE_BACKUP_H

#include "app_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** NVS fallback when microSD is missing or write fails (settings only). */
bool app_storage_backup_save_settings(const app_settings_t * settings);
bool app_storage_backup_load_settings(app_settings_t * out);

#ifdef __cplusplus
}
#endif

#endif
