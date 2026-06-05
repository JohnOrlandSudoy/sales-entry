#ifndef APP_MAIL_H
#define APP_MAIL_H

#include "app_types.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Send pending invoice rows (ESP32: direct SMTP; PC: http://127.0.0.1:3001).
 * @return true if SMTP/API accepted the message.
 */
bool app_mail_send_invoice(const app_settings_t * settings, char * err, size_t err_sz);

#ifdef __cplusplus
}
#endif

#endif
