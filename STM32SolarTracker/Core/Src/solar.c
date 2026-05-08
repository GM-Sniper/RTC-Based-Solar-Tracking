#include <stdio.h>
#include <time.h>
#include "cmsis_os.h"
#include "main.h"
#include "gps_module.h"
#include "servo.h"
#include "solpos00.h"
#include "solar.h"

/* Servo control functions (from servo.c) */
extern void Servo_Init(void);
extern void Servo_SetAngle(uint8_t channel, float angle_deg);

/* Debug/logging macro */
#define SOLAR_DEBUG_PRINT(fmt, ...) printf("[SOLAR] " fmt "\r\n", ##__VA_ARGS__)

/* Static storage for solar calculations */
static struct posdata g_pdat;
static float g_elevation = 0.0f;
static float g_azimuth = 0.0f;

/**
 * @brief Clamp a float value between lo and hi
 */
static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/**
 * @brief Get current solar elevation angle (degrees)
 */
float Solar_GetElevation(void)
{
    return g_elevation;
}

/**
 * @brief Get current solar azimuth angle (degrees)
 */
float Solar_GetAzimuth(void)
{
    return g_azimuth;
}

/**
 * @brief Solar tracking task (call from StartSolarTask in main.c)
 * 
 * This task:
 * 1. Reads GPS location and time
 * 2. Calculates solar position (elevation, azimuth)
 * 3. Updates servo angles to track the sun
 */
void Solar_Task(void *argument)
{
    SOLAR_DEBUG_PRINT("Initializing solar tracker...");
    
    /* Initialize servo control */
    Servo_Init();
    
    /* Initialize solar position data structure */
    S_init(&g_pdat);
    
    /* Use month/day input mode (not daynum) */
    g_pdat.function &= ~S_DOY;
    
    /* Set site location and atmospheric parameters */
    g_pdat.latitude = SITE_LATITUDE;
    g_pdat.longitude = SITE_LONGITUDE;
    g_pdat.timezone = SITE_TIMEZONE;
    g_pdat.press = 1013.0f;      /* Standard pressure */
    g_pdat.temp = 25.0f;         /* Ambient temperature */
    
    /* Move servos to center (90 degrees) initially */
    Servo_SetAngle(SERVO_CHANNEL_TILT, 90.0f);
    Servo_SetAngle(SERVO_CHANNEL_PAN, 90.0f);
    
    SOLAR_DEBUG_PRINT("Solar tracker ready. Waiting for GPS fix...");
    
    /* Main solar tracking loop */
    while (1)
    {
        /* Get GPS data (location and time) */
        gps_data_t gps_data;
        GPS_GetData(&gps_data);
        
        /* Check if GPS has a valid fix before using time/location */
        if (!gps_data.valid_fix)
        {
            SOLAR_DEBUG_PRINT("Waiting for GPS fix...");
            osDelay(5000);
            continue;
        }
        
        /* Update solar calculation with current GPS time and location */
        g_pdat.year = gps_data.utc_year;
        g_pdat.month = gps_data.utc_month;
        g_pdat.day = gps_data.utc_day;
        g_pdat.hour = gps_data.utc_hour;
        g_pdat.minute = gps_data.utc_minute;
        g_pdat.second = gps_data.utc_second;
        g_pdat.interval = 0;
        
        /* Use GPS location if available */
        if (gps_data.latitude != 0.0f && gps_data.longitude != 0.0f)
        {
            g_pdat.latitude = gps_data.latitude;
            g_pdat.longitude = gps_data.longitude;
        }
        
        /* Calculate solar position */
        long ret = S_solpos(&g_pdat);
        if (ret != 0)
        {
            SOLAR_DEBUG_PRINT("S_solpos error code: %ld", ret);
            osDelay(1000);
            continue;
        }
        
        /* Store calculated values */
        g_elevation = g_pdat.elevref;
        g_azimuth = g_pdat.azim;
        
        /* Clamp elevation to servo range (0-180 degrees) */
        float servo_tilt_target = clampf(g_elevation + TILT_SERVO_OFFSET_DEG, 0.0f, 180.0f);
        
        /* Update servo positions based on solar position */
        Servo_SetAngle(SERVO_CHANNEL_TILT, servo_tilt_target);
        
        /* Pan servo can track azimuth if needed (0-180 maps to 0-360 degrees) */
        float servo_pan_target = clampf(g_azimuth / 2.0f, 0.0f, 180.0f);
        Servo_SetAngle(SERVO_CHANNEL_PAN, servo_pan_target);
        
        SOLAR_DEBUG_PRINT("Time: %04d-%02d-%02d %02d:%02d:%02d | Lat=%.4f Lon=%.4f | "
                          "Elev=%.2f° | Azim=%.2f° | ServoTilt=%.2f° | ServoPan=%.2f°",
                          g_pdat.year, g_pdat.month, g_pdat.day,
                          g_pdat.hour, g_pdat.minute, g_pdat.second,
                          g_pdat.latitude, g_pdat.longitude,
                          g_elevation, g_azimuth,
                          servo_tilt_target, servo_pan_target);
        
        /* Update solar position once per second */
        osDelay(1000);
    }
}
