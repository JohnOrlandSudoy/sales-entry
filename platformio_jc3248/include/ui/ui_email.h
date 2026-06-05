#ifndef UI_EMAIL_H
#define UI_EMAIL_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_email_cb_t)(void * user_data);
/** @param sent_count rows emailed (0 = failed or nothing sent) */
typedef void (*ui_email_sent_cb_t)(int sent_count, const char * msg, void * user_data);

/** Full-screen promo-head invoice review (after master PIN). */
void ui_email_review_show(lv_obj_t * parent, ui_email_sent_cb_t on_sent, ui_email_cb_t on_close,
                          void * user_data);

void ui_email_review_hide(void);
/** Close invoice review before showing result modal (avoids dashboard flash). */
void ui_email_review_hide_before_result(void);

/** Blocking success/fail dialog after APPROVE & SEND (tap OK to continue). */
void ui_email_result_show(lv_obj_t * parent, bool success, const char * title, const char * message,
                          ui_email_cb_t on_ok, void * user_data);
void ui_email_result_hide(void);

#ifdef __cplusplus
}
#endif

#endif
