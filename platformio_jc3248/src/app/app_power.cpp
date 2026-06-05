#include "app_power.h"
#include "board_config.h"
#include "display.h"
#include "app_mail_task.h"

#include <stdio.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static uint32_t s_last_activity_ms;
static bool s_idle_inited;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static uint32_t idle_limit_ms(void)
{
    return (uint32_t)APP_IDLE_SLEEP_MINUTES * 60U * 1000U;
}

void app_idle_feed(void)
{
    s_last_activity_ms = now_ms();
    if(!s_idle_inited)
        s_idle_inited = true;
}

void app_idle_poll(void)
{
    if(!s_idle_inited) {
        app_idle_feed();
        return;
    }

    if(app_mail_task_busy()) {
        app_idle_feed();
        return;
    }

    const uint32_t limit = idle_limit_ms();
    const uint32_t idle = now_ms() - s_last_activity_ms;
    if(idle < limit)
        return;

    printf("[power] idle %lu min — deep sleep (touch GPIO%d to wake)\n",
           (unsigned long)(limit / 60000U), BOARD_TOUCH_INT);

    display_backlight_off();
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(50));

    gpio_reset_pin((gpio_num_t)BOARD_TOUCH_INT);
    gpio_set_direction((gpio_num_t)BOARD_TOUCH_INT, GPIO_MODE_INPUT);
    gpio_pullup_en((gpio_num_t)BOARD_TOUCH_INT);

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    const uint64_t mask = 1ULL << BOARD_TOUCH_INT;
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);

    esp_deep_sleep_start();
}
