#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "protocol_examples_common.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/i2c.h"
#include "lwip/apps/sntp.h"

// --- Configuration: update these to match your Wi-Fi ---
#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "your-password"
#endif

#define TAG "RTC"

// I2C config for DS3231
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

#define DS3231_ADDR 0x68

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

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

static esp_err_t ds3231_set_time(struct tm *tm)
{
    uint8_t data[8];
    data[0] = 0x00; // start register
    data[1] = dec_to_bcd(tm->tm_sec);
    data[2] = dec_to_bcd(tm->tm_min);
    data[3] = dec_to_bcd(tm->tm_hour);
    data[4] = dec_to_bcd(tm->tm_wday + 1); // DS3231: 1 = Sunday
    data[5] = dec_to_bcd(tm->tm_mday);
    data[6] = dec_to_bcd(tm->tm_mon + 1);
    data[7] = dec_to_bcd((tm->tm_year + 1900) % 100);

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

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void initialise_wifi(void)
{
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID:%s", WIFI_SSID);
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Wi-Fi connected");
}

static void obtain_time(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    time_t now = 0;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "System time is: %s", asctime(&timeinfo));
}

void rtc_task(void *pvParameter)
{
    // initialize NVS (required by Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    initialise_wifi();
    obtain_time();

    initialize_i2c();

    // After SNTP sync, set DS3231
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (ds3231_set_time(&timeinfo) == ESP_OK)
    {
        ESP_LOGI(TAG, "DS3231 time set from SNTP: %s", asctime(&timeinfo));
    }
    else
    {
        ESP_LOGW(TAG, "Failed to set DS3231 time");
    }

    // Main loop: read DS3231 and log every second
    while (1)
    {
        struct tm rtc_tm = {0};
        if (ds3231_read_time(&rtc_tm) == ESP_OK)
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
