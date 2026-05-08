#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

typedef struct
{
    bool valid_fix;
    bool valid_time;
    double latitude;
    double longitude;
    int satellites;
    struct tm utc_time;
    long long last_update_us;
} gps_data_t;

esp_err_t gps_init(void);
void gps_task(void *pvParameter);
bool gps_get_latest(gps_data_t *out);

#endif
