#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "TCA9554PWR.h"
#include "PCF85063.h"
#include "QMI8658.h"
#include "ST7701S.h"
#include "CST820.h"
#include "SD_MMC.h"
#include "LVGL_Driver.h"
#include "LVGL_Example.h"
#include "Wireless.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

void Driver_Loop(void *parameter)
{
    while(1)
    {
        QMI8658_Loop();
        RTC_Loop();
        BAT_Get_Volts();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}
void Driver_Init(void)
{
    Flash_Searching();
    BAT_Init();
    I2C_Init();
    PCF85063_Init();
    QMI8658_Init();
    EXIO_Init();                    // Example Initialize EXIO
    xTaskCreatePinnedToCore(
        Driver_Loop, 
        "Other Driver task",
        4096, 
        NULL, 
        3, 
        NULL, 
        0);
}
static const char *APP_TAG = "APP";

static esp_timer_handle_t s_lv_tick_timer = NULL;
static void lv_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

static void gui_task(void *arg)
{
    esp_task_wdt_add(NULL);
    for (;;) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
        esp_task_wdt_reset();
    }
}

void app_main(void)
{   
    ESP_LOGI(APP_TAG, "Booting...");

    // Task Watchdog is auto-initialized by Kconfig (CONFIG_ESP_TASK_WDT_INIT=y)
    // Just register tasks that need monitoring (e.g., in gui_task)

    Wireless_Init();
    Driver_Init();

    LCD_Init();
    Touch_Init();
    SD_Init();
    LVGL_Init();
    Lvgl_Example1();

    // 1ms LVGL tick
    const esp_timer_create_args_t tick_args = {
        .callback = &lv_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &s_lv_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_lv_tick_timer, 1000));

    // Dedicated GUI task on core 1
    xTaskCreatePinnedToCore(gui_task, "GUI", 6144, NULL, 5, NULL, 1);
}
