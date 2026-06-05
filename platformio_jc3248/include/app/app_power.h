#ifndef APP_POWER_H
#define APP_POWER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Minutes of no touch/UI activity before deep sleep (default 30). */
#ifndef APP_IDLE_SLEEP_MINUTES
#define APP_IDLE_SLEEP_MINUTES  30
#endif

void app_idle_feed(void);
void app_idle_poll(void);

#ifdef __cplusplus
}
#endif

#endif
