#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "rtc.h"
#include "solpos00.h"

esp_err_t servo_tilt_init(gpio_num_t pin);
esp_err_t servo_tilt_set_angle(float angle_deg);

#define TAG "SOLAR"

// Adjust for your site
#define SITE_LATITUDE 30.020415f
#define SITE_LONGITUDE 31.498402f
#define SITE_TIMEZONE 2.0f

// Change this if your tilt servo is wired to a different GPIO.
#define TILT_SERVO_PIN GPIO_NUM_25

// Mechanical calibration offset. Increase/decrease if the panel is not level at 0 deg.
#define TILT_SERVO_OFFSET_DEG 0.0f

static float clampf(float v, float lo, float hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

void solar_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Initializing RTC for solar position calculations...");
    rtc_init_ds3231();

    ESP_LOGI(TAG, "Initializing tilt servo driver...");
    if (servo_tilt_init(TILT_SERVO_PIN) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize tilt servo");
        vTaskDelete(NULL);
        return;
    }

    // Temporary bring-up behavior: initialize RTC with a demo time once.
    // Remove this call when using real RTC/SNTP/GPS synchronization.
    if (rtc_set_demo_time() != ESP_OK)
    {
        ESP_LOGW(TAG, "RTC demo time initialization failed");
    }

    struct posdata pdat;
    S_init(&pdat);

    // Use month/day input mode (not daynum).
    pdat.function &= ~S_DOY;

    // Site/environment constants
    pdat.latitude = SITE_LATITUDE;
    pdat.longitude = SITE_LONGITUDE;
    pdat.timezone = SITE_TIMEZONE;
    pdat.press = 1013.0f;
    pdat.temp = 25.0f;

    while (1)
    {
        struct tm rtc_tm = {0};
        esp_err_t rtc_ret = rtc_get_time(&rtc_tm);
        if (rtc_ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to read RTC (err=%d)", (int)rtc_ret);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Update time fields from DS3231
        pdat.year = rtc_tm.tm_year + 1900;
        pdat.month = rtc_tm.tm_mon + 1;
        pdat.day = rtc_tm.tm_mday;
        pdat.hour = rtc_tm.tm_hour;
        pdat.minute = rtc_tm.tm_min;
        pdat.second = rtc_tm.tm_sec;
        pdat.interval = 0;

        long ret = S_solpos(&pdat);
        if (ret != 0)
        {
            ESP_LOGW(TAG, "S_solpos validation/calculation error code: %ld", ret);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // For now, directly map solar elevation to a positional servo angle.
        float elevation_deg = pdat.elevref;
        float servo_target_deg = clampf(elevation_deg + TILT_SERVO_OFFSET_DEG, 0.0f, 180.0f);

        if (servo_tilt_set_angle(servo_target_deg) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to update tilt servo angle");
        }

        ESP_LOGI(TAG,
                 "RTC %04d-%02d-%02d %02d:%02d:%02d | Elev=%.2f deg | Azim=%.2f deg | ServoTiltTarget=%.2f deg",
                 pdat.year, pdat.month, pdat.day,
                 pdat.hour, pdat.minute, pdat.second,
                 elevation_deg, pdat.azim, servo_target_deg);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
