#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "rtc.h"

#define TAG "RTC"

// I2C config for DS3231
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

#define DS3231_ADDR 0x68

static void initialize_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

static uint8_t dec_to_bcd(int val)
{
    return (uint8_t)((val / 10) << 4 | (val % 10));
}

static int bcd_to_dec(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

static esp_err_t ds3231_set_time_internal(const struct tm *tm)
{
    struct tm local_tm = *tm;
    uint8_t data[8];
    data[0] = 0x00; // start register
    data[1] = dec_to_bcd(local_tm.tm_sec);
    data[2] = dec_to_bcd(local_tm.tm_min);
    data[3] = dec_to_bcd(local_tm.tm_hour);
    data[4] = dec_to_bcd(local_tm.tm_wday + 1); // DS3231: 1 = Sunday
    data[5] = dec_to_bcd(local_tm.tm_mday);
    data[6] = dec_to_bcd(local_tm.tm_mon + 1);
    data[7] = dec_to_bcd((local_tm.tm_year + 1900) % 100);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, sizeof(data), true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t ds3231_read_time(struct tm *tm)
{
    uint8_t reg = 0x00;
    uint8_t buf[7];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buf, 6, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, buf + 6, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_OK)
    {
        tm->tm_sec = bcd_to_dec(buf[0] & 0x7F);
        tm->tm_min = bcd_to_dec(buf[1]);
        tm->tm_hour = bcd_to_dec(buf[2] & 0x3F);
        tm->tm_mday = bcd_to_dec(buf[4]);
        tm->tm_mon = bcd_to_dec(buf[5]) - 1;
        tm->tm_year = bcd_to_dec(buf[6]) + 100; // years since 1900 (assume 20xx)
        tm->tm_wday = bcd_to_dec(buf[3]) - 1;
    }
    return ret;
}

esp_err_t rtc_init_ds3231(void)
{
    initialize_i2c();
    return ESP_OK;
}

esp_err_t rtc_set_time(const struct tm *tm)
{
    if (tm == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return ds3231_set_time_internal(tm);
}

esp_err_t rtc_get_time(struct tm *tm)
{
    if (tm == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return ds3231_read_time(tm);
}

esp_err_t rtc_set_demo_time(void)
{
    struct tm init_time = {
        .tm_sec = 45,
        .tm_min = 23,
        .tm_hour = 10,
        .tm_mday = 2,
        .tm_mon = 4,    // May (0-11, so 4 = May)
        .tm_year = 126, // 2026 (years since 1900)
        .tm_wday = 5,   // Friday (0-6, 0 = Sunday)
    };
    return ds3231_set_time_internal(&init_time);
}

void rtc_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Initializing I2C and DS3231...");
    rtc_init_ds3231();

    if (rtc_set_demo_time() == ESP_OK)
    {
        ESP_LOGI(TAG, "DS3231 time initialized to: 2026-05-02 14:30:45");
    }
    else
    {
        ESP_LOGW(TAG, "Failed to set DS3231 time");
    }

    // Main loop: read DS3231 and log every second
    while (1)
    {
        struct tm rtc_tm = {0};
        if (rtc_get_time(&rtc_tm) == ESP_OK)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                     rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
                     rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);
            ESP_LOGI(TAG, "DS3231: %s", buf);
        }
        else
        {
            ESP_LOGW(TAG, "Failed to read DS3231 time");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
