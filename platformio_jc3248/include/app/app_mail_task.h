#ifndef APP_MAIL_TASK_H
#define APP_MAIL_TASK_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_mail_finish_cb)(int batch, const char * msg, void * user_data);

/** Create mail worker task (call once from setup). */
void app_mail_task_init(void);

/** True while SMTP/archive is running on the mail task. */
bool app_mail_task_busy(void);

/** Reason when queue/create failed (for UI). */
const char * app_mail_task_last_error(void);

/**
 * Queue one send on the mail task (not loopTask).
 * @return false if busy or task was not created.
 */
bool app_mail_task_queue(app_mail_finish_cb on_done, void * user_data);

#ifdef __cplusplus
}
#endif

#endif
