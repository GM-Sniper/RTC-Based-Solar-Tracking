#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Servo task implemented in servo.c
extern void Servo_Task(void *pvParameter);
// RTC task implemented in rtc.c
extern void rtc_task(void *pvParameter);

#define TAG "MAIN"

void app_main(void)
{
    ESP_LOGI(TAG, "Starting application and creating Servo task...");
    // Create the servo control task implemented in servo.c
    xTaskCreate(Servo_Task, "Servo_Task", 4096, NULL, 5, NULL);
    // Create RTC task that syncs SNTP and logs DS3231 every second
    xTaskCreate(rtc_task, "RTC_Task", 8192, NULL, 5, NULL);
    // app_main returns; FreeRTOS keeps running tasks.
}
