#ifndef APP_SETTINGS_DEFAULTS_H
#define APP_SETTINGS_DEFAULTS_H

#include "app_types.h"
#include <stdbool.h>

/** Fill empty sender/recipient/SMTP fields from app_defaults.h. Returns true if any field was set. */
bool app_settings_apply_defaults(app_settings_t * settings);

#endif
