#ifndef RTC_H
#define RTC_H

#include <time.h>
#include "esp_err.h"

esp_err_t rtc_init_ds3231(void);
esp_err_t rtc_set_time(const struct tm *tm);
esp_err_t rtc_get_time(struct tm *tm);

// Optional helper for quick bring-up without Wi-Fi/GPS time source.
esp_err_t rtc_set_demo_time(void);

// Existing standalone task kept for direct RTC testing.
void rtc_task(void *pvParameter);

#endif
