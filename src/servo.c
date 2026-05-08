#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"

void Servo_Task(void *pvParameter);
esp_err_t servo_tilt_init(gpio_num_t pin);
esp_err_t servo_tilt_set_angle(float angle_deg);

#define TAG "SERVO"
#define SERVO_TILT_PIN GPIO_NUM_25 // Tilt / Elevation (change if you wired another GPIO)

static gpio_num_t s_tilt_pin = SERVO_TILT_PIN;
static bool s_tilt_initialized = false;

static uint32_t angle_to_duty_cycle(float angle)
{
    if (angle < 0.0f)
    {
        angle = 0.0f;
    }
    if (angle > 180.0f)
    {
        angle = 180.0f;
    }

    // Map 0-180 degrees to duty cycle range (e.g., 0.5ms to 2.5ms pulse width)
    float pulse_width = 0.5f + (angle / 180.0f) * 2.0f; // Pulse width in ms
    return (uint32_t)((pulse_width / 20.0f) * 4096.0f);
}

static esp_err_t servo_tilt_apply_angle(float angle_deg)
{
    if (!s_tilt_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t duty_cycle = angle_to_duty_cycle(angle_deg);
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_cycle);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

esp_err_t servo_tilt_init(gpio_num_t pin)
{
    s_tilt_pin = pin;

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK};
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ledc_channel_config_t tilt_channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = s_tilt_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_0,
    };
    ret = ledc_channel_config(&tilt_channel);
    if (ret == ESP_OK)
    {
        s_tilt_initialized = true;
    }
    return ret;
}

esp_err_t servo_tilt_set_angle(float angle_deg)
{
    return servo_tilt_apply_angle(angle_deg);
}

// `app_main()` moved to src/main.c -- this file provides `Servo_Task()`.
// Call `xTaskCreate(Servo_Task, "Servo_Task", stack_size, NULL, priority, NULL)` from your
// application's `app_main()` to start the servo task.

void Servo_Task(void *pvParameter)
{
    ESP_LOGI(TAG, "Initializing tilt servo on GPIO %d", (int)s_tilt_pin);
    if (servo_tilt_init(s_tilt_pin) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize tilt servo");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Moving tilt servo to center (90 deg)");
    servo_tilt_set_angle(90.0f);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
