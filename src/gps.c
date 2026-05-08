#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "gps.h"

#define TAG "GPS"

// UART2 is a good default on ESP32 for external peripherals.
#define GPS_UART_NUM UART_NUM_2
#define GPS_TX_PIN 17
#define GPS_RX_PIN 16
#define GPS_BAUD_RATE 9600
#define GPS_RX_BUF_SIZE 2048
#define GPS_LINE_MAX 128

static SemaphoreHandle_t s_gps_mutex;
static gps_data_t s_gps_data;
static bool s_uart_initialized;

static int hex_to_int(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    c = (char)toupper((unsigned char)c);
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return -1;
}

static bool nmea_checksum_ok(const char *line)
{
    if (line == NULL || line[0] != '$')
    {
        return false;
    }

    const char *star = strchr(line, '*');
    if (star == NULL || (star - line) < 2 || strlen(star) < 3)
    {
        return false;
    }

    unsigned char calc = 0;
    for (const char *p = line + 1; p < star; ++p)
    {
        calc ^= (unsigned char)(*p);
    }

    int hi = hex_to_int(star[1]);
    int lo = hex_to_int(star[2]);
    if (hi < 0 || lo < 0)
    {
        return false;
    }

    unsigned char expected = (unsigned char)((hi << 4) | lo);
    return calc == expected;
}

static bool parse_hhmmss(const char *s, int *hour, int *min, int *sec)
{
    if (s == NULL || strlen(s) < 6)
    {
        return false;
    }

    int h = (s[0] - '0') * 10 + (s[1] - '0');
    int m = (s[2] - '0') * 10 + (s[3] - '0');
    int se = (s[4] - '0') * 10 + (s[5] - '0');
    if (h < 0 || h > 23 || m < 0 || m > 59 || se < 0 || se > 59)
    {
        return false;
    }

    *hour = h;
    *min = m;
    *sec = se;
    return true;
}

static bool parse_ddmmyy(const char *s, int *day, int *month, int *year)
{
    if (s == NULL || strlen(s) < 6)
    {
        return false;
    }

    int d = (s[0] - '0') * 10 + (s[1] - '0');
    int mo = (s[2] - '0') * 10 + (s[3] - '0');
    int yy = (s[4] - '0') * 10 + (s[5] - '0');
    if (d < 1 || d > 31 || mo < 1 || mo > 12)
    {
        return false;
    }

    *day = d;
    *month = mo;
    *year = 2000 + yy;
    return true;
}

static bool nmea_to_decimal_degrees(const char *field, char hemi, double *out)
{
    if (field == NULL || out == NULL || field[0] == '\0')
    {
        return false;
    }

    double raw = atof(field);
    if (raw <= 0.0)
    {
        return false;
    }

    double deg = floor(raw / 100.0);
    double minutes = raw - (deg * 100.0);
    double dec = deg + (minutes / 60.0);

    if (hemi == 'S' || hemi == 'W')
    {
        dec = -dec;
    }

    *out = dec;
    return true;
}

static void update_shared_data(const gps_data_t *update)
{
    if (xSemaphoreTake(s_gps_mutex, pdMS_TO_TICKS(20)) == pdTRUE)
    {
        s_gps_data = *update;
        xSemaphoreGive(s_gps_mutex);
    }
}

static void parse_rmc_sentence(char *line)
{
    // Example: $GPRMC,hhmmss.sss,A,llll.ll,a,yyyyy.yy,a,...,ddmmyy,...*CS
    char *saveptr = NULL;
    char *token = strtok_r(line, ",", &saveptr);
    int field_index = 0;

    char time_str[16] = {0};
    char status = 'V';
    char lat_str[24] = {0};
    char ns = 'N';
    char lon_str[24] = {0};
    char ew = 'E';
    char date_str[16] = {0};

    while (token != NULL)
    {
        switch (field_index)
        {
        case 1:
            strncpy(time_str, token, sizeof(time_str) - 1);
            break;
        case 2:
            status = token[0];
            break;
        case 3:
            strncpy(lat_str, token, sizeof(lat_str) - 1);
            break;
        case 4:
            ns = token[0];
            break;
        case 5:
            strncpy(lon_str, token, sizeof(lon_str) - 1);
            break;
        case 6:
            ew = token[0];
            break;
        case 9:
            strncpy(date_str, token, sizeof(date_str) - 1);
            break;
        default:
            break;
        }

        token = strtok_r(NULL, ",", &saveptr);
        ++field_index;
    }

    gps_data_t next = s_gps_data;
    next.last_update_us = esp_timer_get_time();

    if (status == 'A')
    {
        double lat = 0.0;
        double lon = 0.0;
        if (nmea_to_decimal_degrees(lat_str, ns, &lat) && nmea_to_decimal_degrees(lon_str, ew, &lon))
        {
            next.latitude = lat;
            next.longitude = lon;
            next.valid_fix = true;
        }
    }
    else
    {
        next.valid_fix = false;
    }

    int hour = 0;
    int min = 0;
    int sec = 0;
    int day = 0;
    int month = 0;
    int year = 0;
    if (parse_hhmmss(time_str, &hour, &min, &sec) && parse_ddmmyy(date_str, &day, &month, &year))
    {
        memset(&next.utc_time, 0, sizeof(next.utc_time));
        next.utc_time.tm_year = year - 1900;
        next.utc_time.tm_mon = month - 1;
        next.utc_time.tm_mday = day;
        next.utc_time.tm_hour = hour;
        next.utc_time.tm_min = min;
        next.utc_time.tm_sec = sec;
        next.valid_time = true;
    }

    update_shared_data(&next);
}

static void parse_gga_sentence(char *line)
{
    // Example: $GPGGA,time,lat,NS,lon,EW,fix,sats,...
    char *saveptr = NULL;
    char *token = strtok_r(line, ",", &saveptr);
    int field_index = 0;

    int fix_quality = 0;
    int satellites = 0;

    while (token != NULL)
    {
        if (field_index == 6)
        {
            fix_quality = atoi(token);
        }
        else if (field_index == 7)
        {
            satellites = atoi(token);
        }

        token = strtok_r(NULL, ",", &saveptr);
        ++field_index;
    }

    gps_data_t next = s_gps_data;
    next.satellites = satellites;
    if (fix_quality <= 0)
    {
        next.valid_fix = false;
    }
    next.last_update_us = esp_timer_get_time();
    update_shared_data(&next);
}

static void handle_nmea_line(char *line)
{
    if (!nmea_checksum_ok(line))
    {
        return;
    }

    // Work on a mutable copy for tokenization.
    char tmp[GPS_LINE_MAX];
    strncpy(tmp, line, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    // Remove checksum suffix to simplify CSV token parsing.
    char *star = strchr(tmp, '*');
    if (star != NULL)
    {
        *star = '\0';
    }

    if (strncmp(tmp, "$GPRMC", 6) == 0 || strncmp(tmp, "$GNRMC", 6) == 0)
    {
        parse_rmc_sentence(tmp);
    }
    else if (strncmp(tmp, "$GPGGA", 6) == 0 || strncmp(tmp, "$GNGGA", 6) == 0)
    {
        parse_gga_sentence(tmp);
    }
}

esp_err_t gps_init(void)
{
    if (s_uart_initialized)
    {
        return ESP_OK;
    }

    if (s_gps_mutex == NULL)
    {
        s_gps_mutex = xSemaphoreCreateMutex();
        if (s_gps_mutex == NULL)
        {
            return ESP_ERR_NO_MEM;
        }
    }

    uart_config_t uart_config = {
        .baud_rate = GPS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(GPS_UART_NUM, GPS_RX_BUF_SIZE, 0, 0, NULL, 0);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = uart_param_config(GPS_UART_NUM, &uart_config);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = uart_set_pin(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK)
    {
        return ret;
    }

    memset(&s_gps_data, 0, sizeof(s_gps_data));
    s_uart_initialized = true;
    ESP_LOGI(TAG, "GPS UART initialized on UART2 RX=%d TX=%d @ %d baud", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    return ESP_OK;
}

bool gps_get_latest(gps_data_t *out)
{
    if (out == NULL || s_gps_mutex == NULL)
    {
        return false;
    }

    if (xSemaphoreTake(s_gps_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
    {
        return false;
    }

    *out = s_gps_data;
    xSemaphoreGive(s_gps_mutex);
    return true;
}

void gps_task(void *pvParameter)
{
    if (gps_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize GPS UART");
        vTaskDelete(NULL);
        return;
    }

    uint8_t rx_byte = 0;
    char line[GPS_LINE_MAX] = {0};
    int idx = 0;

    while (1)
    {
        int n = uart_read_bytes(GPS_UART_NUM, &rx_byte, 1, pdMS_TO_TICKS(100));
        if (n <= 0)
        {
            continue;
        }

        if (rx_byte == '\r')
        {
            continue;
        }

        if (rx_byte == '\n')
        {
            if (idx > 0)
            {
                line[idx] = '\0';
                handle_nmea_line(line);
                idx = 0;
            }
            continue;
        }

        if (idx < (GPS_LINE_MAX - 1))
        {
            line[idx++] = (char)rx_byte;
        }
        else
        {
            // Overflow protection: drop current line and wait for next newline.
            idx = 0;
        }
    }
}
