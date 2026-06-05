#include "app_time.h"
#include "app_data.h"
#include "app_wifi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#if defined(ESP_PLATFORM)
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#define APP_TIME_MIN_EPOCH      1700000000L
#define APP_NTP_POLL_MS_MAX     12000

static bool s_ntp_pending;
static bool s_ntp_synced_this_boot;
static uint32_t s_ntp_started_ms;

static uint32_t now_ms(void)
{
#if defined(ESP_PLATFORM)
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
    return 0;
#endif
}

static void app_time_set_tz(void)
{
    /* POSIX TZ — reliable on ESP32 libc (UTC+8, walang DST) */
    setenv("TZ", "PHT-8", 1);
    tzset();
}

static void seed_fallback_clock(void)
{
    struct tm t = {};
    t.tm_year = 126;
    t.tm_mon = 4;
    t.tm_mday = 20;
    t.tm_hour = 9;
    t.tm_min = 0;
    t.tm_sec = 0;
    time_t epoch = mktime(&t);
    struct timeval tv = {epoch, 0};
    settimeofday(&tv, nullptr);
    printf("[time] seeded fallback (no valid RTC yet)\n");
}

#if defined(ESP_PLATFORM)
static void sntp_sync_cb(struct timeval * tv)
{
    (void)tv;
    s_ntp_pending = false;
    s_ntp_synced_this_boot = true;
    app_time_set_tz();
    printf("[time] SNTP ok epoch=%lu\n", (unsigned long)time(nullptr));
    if(app_clamp_selected_date())
        printf("[time] selected date clamped after sync\n");
}
#endif

void app_time_boot_init(void)
{
    app_time_set_tz();

    if(time(nullptr) > APP_TIME_MIN_EPOCH) {
        printf("[time] RTC ok epoch=%lu\n", (unsigned long)time(nullptr));
        return;
    }
    seed_fallback_clock();
}

bool app_time_is_valid(void)
{
    return time(nullptr) > APP_TIME_MIN_EPOCH;
}

void app_time_on_wifi_connected(void)
{
#if !defined(ESP_PLATFORM)
    (void)0;
#else
    if(s_ntp_pending || s_ntp_synced_this_boot)
        return;

    if(!esp_sntp_enabled()) {
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.nist.gov");
        esp_sntp_set_time_sync_notification_cb(sntp_sync_cb);
        esp_sntp_init();
    }
    else {
        esp_sntp_restart();
    }

    s_ntp_pending = true;
    s_ntp_started_ms = now_ms();
    printf("[time] SNTP sync started\n");
#endif
}

void app_time_poll(void)
{
    static bool s_wifi_ntp_armed;
    if(!s_wifi_ntp_armed && app_wifi_is_connected()) {
        s_wifi_ntp_armed = true;
        app_time_on_wifi_connected();
    }

    if(!s_ntp_pending)
        return;

    if(s_ntp_synced_this_boot) {
        s_ntp_pending = false;
        return;
    }

    if(!app_wifi_is_connected()) {
        s_ntp_pending = false;
        return;
    }

    if(now_ms() - s_ntp_started_ms > APP_NTP_POLL_MS_MAX) {
        s_ntp_pending = false;
        printf("[time] SNTP timeout — using RTC/fallback\n");
    }
}
