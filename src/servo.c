#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"

void Servo_Task(void *pvParameter);
uint32_t angle_to_duty_cycle(uint8_t angle);

#define TAG "SERVO"
#define SERVO_PAN_PIN GPIO_NUM_25 // Base / Azimuth

// `app_main()` moved to src/main.c -- this file provides `Servo_Task()`.
// Call `xTaskCreate(Servo_Task, "Servo_Task", stack_size, NULL, priority, NULL)` from your
// application's `app_main()` to start the servo task.

void Servo_Task(void *pvParameter)
{
    // 1. Configure the LEDC Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&ledc_timer);

    // 2. Configure the LEDC Channel for Pan
    ledc_channel_config_t pan_channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = SERVO_PAN_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_0,
    };
    ledc_channel_config(&pan_channel);

    while (1)
    {
        // Forward sweep: 0 to 180 degrees
        for (uint8_t angle = 0; angle <= 180; angle += 10)
        {
            ESP_LOGI(TAG, "Moving servo to %d degrees (forward)\n", angle);
            uint32_t duty_cycle = angle_to_duty_cycle(angle);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_cycle);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for servo to move
        }

        // Reverse sweep: 170 down to 10 degrees
        ESP_LOGI(TAG, "Starting reverse sweep...\n");
        for (int16_t angle = 170; angle >= 10; angle -= 10)
        {
            ESP_LOGI(TAG, "Moving servo to %d degrees (reverse)\n", angle);
            uint32_t duty_cycle = angle_to_duty_cycle((uint8_t)angle);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_cycle);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for servo to move
        }
        ESP_LOGI(TAG, "Reverse sweep complete, restarting...\n");
    }
}

uint32_t angle_to_duty_cycle(uint8_t angle)
{
    if (angle > 180)
        angle = 180; // Limit to 180 degrees
    // Map 0-180 degrees to duty cycle range (e.g., 0.5ms to 2.5ms pulse width)
    float pulse_width = 0.5 + (angle / 180.0) * 2.0; // Pulse width in ms
    uint32_t duty = (pulse_width / 20.0) * 4096;     // Convert to duty cycle for 12-bit resolution
    return duty;
}
