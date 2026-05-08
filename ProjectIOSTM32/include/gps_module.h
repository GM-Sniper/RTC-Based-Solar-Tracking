#ifndef GPS_MODULE_H
#define GPS_MODULE_H

#include <stdint.h>
#include <stdbool.h>

/* GPS Data Structure */
typedef struct {
    bool valid_fix;
    uint8_t utc_hour;
    uint8_t utc_minute;
    uint8_t utc_second;
    uint8_t utc_day;
    uint8_t utc_month;
    uint16_t utc_year;
    float latitude;
    float longitude;
    float speed_knots;
    float course;

    uint8_t satellites;
    float hdop;
    float altitude;
    uint8_t fix_quality;

    bool new_data;
} gps_data_t;

void GPS_Init(void);
void GPS_ProcessChar(uint8_t c);
void GPS_GetData(gps_data_t *data);
void GPS_PrintStatus(void);
void GPS_DebugPrint(const char *format, ...);
void GPS_Task(void);

#endif /* GPS_MODULE_H */