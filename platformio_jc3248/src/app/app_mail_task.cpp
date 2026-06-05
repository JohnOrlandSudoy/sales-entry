/**
 * SMTP on a dedicated task (internal-RAM stack) — not loopTask.
 */
#include "app_mail_task.h"
#include "app_data.h"

#if defined(ARDUINO) || defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <string.h>

#define MAIL_TASK_STACK_BYTES  (18 * 1024)
#define MAIL_TASK_PRIO         1
#define MAIL_TASK_CORE         1

static TaskHandle_t s_mail_task;
static volatile bool s_busy;
static app_mail_finish_cb s_done_cb;
static void * s_done_ud;
static int s_batch;
static char s_msg[128];
static char s_last_err[96];

static void mail_finish_on_lvgl(void * ud)
{
    (void)ud;
    app_mail_finish_cb cb = s_done_cb;
    void * cb_ud = s_done_ud;
    int batch = s_batch;
    char msg[128];
    memcpy(msg, s_msg, sizeof(msg));
    s_done_cb = NULL;
    s_done_ud = NULL;
    s_busy = false;
    if(cb)
        cb(batch, msg, cb_ud);
}

static void mail_worker(void * arg)
{
    (void)arg;
    for(;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        s_msg[0] = '\0';
        s_batch = app_send_pending_email(s_msg, sizeof(s_msg));
        lv_async_call(mail_finish_on_lvgl, NULL);
    }
}

static bool mail_task_create_with_stack(uint32_t stack_bytes)
{
    if(s_mail_task)
        return true;

    uint32_t heap = ESP.getFreeHeap();
    size_t block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if(block < stack_bytes + 1536) {
        snprintf(s_last_err, sizeof(s_last_err),
                 "heap=%u block=%u need %u",
                 (unsigned)heap, (unsigned)block, (unsigned)(stack_bytes + 1536));
        Serial.printf("[mail] %s\n", s_last_err);
        return false;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        mail_worker, "mail", stack_bytes, NULL,
        MAIL_TASK_PRIO, &s_mail_task, MAIL_TASK_CORE);
    if(ok != pdPASS) {
        snprintf(s_last_err, sizeof(s_last_err),
                 "xTaskCreate failed heap=%u block=%u", (unsigned)heap, (unsigned)block);
        Serial.printf("[mail] %s\n", s_last_err);
        s_mail_task = NULL;
        return false;
    }
    s_last_err[0] = '\0';
    Serial.printf("[mail] worker OK stack=%u KB heap=%u\n",
                  (unsigned)(stack_bytes / 1024),
                  (unsigned)ESP.getFreeHeap());
    return true;
}

static bool mail_task_create(void)
{
    return mail_task_create_with_stack(MAIL_TASK_STACK_BYTES);
}

void app_mail_task_init(void)
{
    if(!mail_task_create())
        Serial.println("[mail] WARN: worker not ready — email may fail on APPROVE");
}

const char * app_mail_task_last_error(void)
{
    return s_last_err[0] ? s_last_err : "email task not ready";
}

bool app_mail_task_busy(void)
{
    return s_busy;
}

bool app_mail_task_queue(app_mail_finish_cb on_done, void * user_data)
{
    if(s_busy) {
        snprintf(s_last_err, sizeof(s_last_err), "already sending");
        return false;
    }
    if(!s_mail_task && !mail_task_create()) {
        return false;
    }
    s_busy = true;
    s_done_cb = on_done;
    s_done_ud = user_data;
    s_msg[0] = '\0';
    s_batch = 0;
    xTaskNotifyGive(s_mail_task);
    return true;
}

#else

void app_mail_task_init(void) {}
const char * app_mail_task_last_error(void) { return ""; }
bool app_mail_task_busy(void) { return false; }
bool app_mail_task_queue(app_mail_finish_cb on_done, void * user_data)
{
    (void)on_done;
    (void)user_data;
    return false;
}

#endif
