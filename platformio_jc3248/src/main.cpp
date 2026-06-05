#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

#include "display.h"
#include "lv_port.h"
#include "app_display.h"
#include "ui/ui_colors.h"

#if defined(APP_USE_SAMPLE_UI)
#include "ui/ui_sales_sample.h"
#else
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "app_data.h"
#include "app_mail_task.h"
#include "app_wifi.h"
#include "app_sd_mount.h"
#include "app_time.h"
#include "app_power.h"
#include "ui/app_shell.h"
#include "debug_agent.h"
#endif

static void log_heap(const char *tag)
{
    Serial.printf("[%s] heap=%u psram=%u\n", tag,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

#if !defined(APP_USE_SAMPLE_UI)

static bool s_ui_built;
static bool s_disp_ready;

static void log_reset_reason(void)
{
    esp_reset_reason_t r = esp_reset_reason();
    const char *name = "other";
    switch (r) {
    case ESP_RST_POWERON: name = "power-on"; break;
    case ESP_RST_PANIC: name = "panic"; break;
    case ESP_RST_TASK_WDT: name = "task_wdt"; break;
    case ESP_RST_BROWNOUT: name = "brownout"; break;
    case ESP_RST_DEEPSLEEP: name = "deep-sleep wake"; break;
    default: break;
    }
    Serial.printf("[boot] reset_reason=%s (%d)\n", name, (int)r);
}

static void build_app_ui(void)
{
    if (!s_disp_ready || s_ui_built) {
        return;
    }
    s_ui_built = true;

    Serial.println("[UI] app_shell_show...");
    log_heap("pre-ui");
    esp_task_wdt_reset();

    app_shell_show();
    lv_refr_now(NULL);
}

#endif /* !APP_USE_SAMPLE_UI */

void setup()
{
    Serial.begin(115200);
    delay(1500);

#if defined(APP_USE_SAMPLE_UI)
    Serial.printf("=== Sales Entry SAMPLE %dx%d ===\n", APP_SCREEN_W, APP_SCREEN_H);
#else
    Serial.printf("=== Sales Entry PORTRAIT %dx%d ===\n", APP_SCREEN_W, APP_SCREEN_H);
    log_reset_reason();
#endif
    log_heap("boot");

#if !defined(APP_USE_SAMPLE_UI)
    app_time_boot_init();
    app_idle_feed();
    if(app_sd_mount()) {
        Serial.println("[SD] early mount OK — sales can load from card");
    }
    else {
        Serial.printf("[WARN] early SD: %s\n", app_sd_user_message());
    }
    log_heap("sd-early");
    app_data_init();
    app_wifi_boot_init();
    log_heap("data");
#endif

    if (!display_begin()) {
        Serial.println("[ERR] display_begin — reboot (heap too low?)");
        return;
    }
    log_heap("disp");

    if (!lv_port_init()) {
        Serial.println("[ERR] lv_port_init");
        return;
    }
    s_disp_ready = true;
    log_heap("lvgl");

#if !defined(APP_USE_SAMPLE_UI)
    /* Reserve mail stack while heap is still ~75+ KB (APPROVE only has ~25 KB left). */
    app_mail_task_init();
    log_heap("mail");
    /* Retry SD if early mount failed; reload sales/history from card. */
    if(!app_sd_is_mounted()) {
        if(app_sd_mount()) {
            Serial.println("[SD] mount OK after display — reloading data");
            app_data_reload_from_storage();
        }
        else {
            Serial.printf("[WARN] %s\n", app_sd_user_message());
        }
    }
    log_heap("sd");
#endif

#if defined(APP_USE_SAMPLE_UI)

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    ui_sales_sample_create(lv_scr_act());

#else

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    Serial.println("[setup] display OK, loading UI...");

#endif

    lv_timer_handler();
}

void loop()
{
#if !defined(APP_USE_SAMPLE_UI)
    if (s_disp_ready && !s_ui_built) {
        build_app_ui();
    }
    if (s_disp_ready) {
        lv_timer_handler();
        app_time_poll();
        app_idle_poll();
    }
#else
    lv_timer_handler();
#endif
    delay(1);
}
