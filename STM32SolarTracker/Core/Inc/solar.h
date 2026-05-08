#ifndef SOLAR_H
#define SOLAR_H

#include "solpos00.h"

/* Site configuration */
#define SITE_LATITUDE   30.020415f   /* Your latitude (degrees north, negative for south) */
#define SITE_LONGITUDE  31.498402f   /* Your longitude (degrees east, negative for west) */
#define SITE_TIMEZONE   2.0f         /* Your timezone offset from UTC (e.g., UTC+2) */

#define TILT_SERVO_OFFSET_DEG  0.0f   /* Mechanical calibration offset for servo (degrees) */

/* Public API */
void Solar_Task(void *argument);
float Solar_GetElevation(void);    /* Get current solar elevation angle (degrees) */
float Solar_GetAzimuth(void);      /* Get current solar azimuth angle (degrees) */

#endif // SOLAR_H
