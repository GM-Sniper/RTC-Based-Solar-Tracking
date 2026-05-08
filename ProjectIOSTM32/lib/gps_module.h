#ifndef GPS_MODULE_H
#define GPS_MODULE_H

#include <stdint.h>
#include <stdbool.h>

/* GPS Data Structure */
typedef struct {
    // RMC sentence data
    bool valid_fix;           // Data valid flag
    uint8_t utc_hour;
    uint8_t utc_minute;
    uint8_t utc_second;
    uint8_t utc_day;
    uint8_t utc_month;
    uint16_t utc_year;        // Full year (e.g., 2026)
    float latitude;           // Decimal degrees (+ North, - South)
    float longitude;          // Decimal degrees (+ East, - West)
    float speed_knots;
    float course;
    
    // GGA sentence data
    uint8_t satellites;       // Number of satellites in use
    float hdop;               // Horizontal Dilution of Precision
    float altitude;           // Meters above sea level
    uint8_t fix_quality;      // 0=Invalid, 1=GPS, 2=DGPS, 3=PPS, 4=RTK, 5=RTK Float
    
    // Update flag
    bool new_data;            // True when new complete sentence parsed
} gps_data_t;

/* Public API Functions */
void GPS_Init(void);
void GPS_ProcessChar(uint8_t c);
void GPS_GetData(gps_data_t *data);
void GPS_PrintStatus(void);
void GPS_DebugPrint(const char *format, ...);

/* Task function for RTOS */
void GPS_Task(void);

#endif // GPS_MODULE_H
