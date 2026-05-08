#include "gps_module.h"
#include "cmsis_os.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* External UART Handles (defined in main.c) */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* ============= Configuration ============= */
#define GPS_RX_BUFFER_SIZE    256
#define GPS_LINE_BUFFER_SIZE  128
#define GPS_UART              huart1
#define DEBUG_UART            huart2

/* ============= Private Variables ============= */
static gps_data_t gps_data = {0};
static uint8_t gps_rx_buffer[GPS_RX_BUFFER_SIZE] = {0};
static uint8_t gps_line_buffer[GPS_LINE_BUFFER_SIZE] = {0};
static uint16_t rx_index = 0;
static uint16_t line_index = 0;

/* External variable for HAL callback (accessed by HAL_UART_Receive_IT) */
uint8_t gps_rx_char = 0;

/* ============= Forward Declarations ============= */
static bool parse_nmea_rmc(const char *sentence);
static bool parse_nmea_gga(const char *sentence);
static uint8_t calculate_checksum(const char *sentence);
static bool validate_checksum(const char *sentence);
static float parse_coordinate(const char *coord_str, char direction);
static bool parse_utc_time(const char *time_str);
static bool parse_utc_date(const char *date_str);

/* ============= Public Functions ============= */

/**
 * @brief Initialize GPS module (start UART1 receive interrupt)
 */
void GPS_Init(void)
{
    memset(&gps_data, 0, sizeof(gps_data_t));
    memset(gps_rx_buffer, 0, GPS_RX_BUFFER_SIZE);
    memset(gps_line_buffer, 0, GPS_LINE_BUFFER_SIZE);
    rx_index = 0;
    line_index = 0;
    
    // Enable UART1 interrupt receiver
    HAL_UART_Receive_IT(&GPS_UART, &gps_rx_char, 1);
    
    GPS_DebugPrint("\r\n[GPS] Initialized on UART1 (9600 baud)\r\n");
}

/**
 * @brief Process incoming GPS character (call from UART1 IRQ)
 * Note: This is called automatically via HAL_UART_RxCpltCallback
 */
void GPS_ProcessChar(uint8_t c)
{
    // Store character in line buffer
    if (line_index < GPS_LINE_BUFFER_SIZE - 1) {
        gps_line_buffer[line_index++] = c;
    }
    
    // Check for end of line (LF after CR)
    if (c == '\n' && line_index > 1 && gps_line_buffer[line_index - 2] == '\r') {
        // Null-terminate
        gps_line_buffer[line_index] = '\0';
        
        // Process complete NMEA sentence
        const char *sentence = (const char *)gps_line_buffer;
        
        // Parse based on sentence type
        if (strncmp(sentence, "$GPRMC", 6) == 0) {
            if (parse_nmea_rmc(sentence)) {
                gps_data.new_data = true;
                GPS_DebugPrint("[RMC] Parsed: %02d/%02d/%04d %02d:%02d:%02d Lat=%.6f Lon=%.6f Valid=%d\r\n", 
                    gps_data.utc_day, gps_data.utc_month, gps_data.utc_year,
                    gps_data.utc_hour, gps_data.utc_minute, gps_data.utc_second,
                    gps_data.latitude, gps_data.longitude, gps_data.valid_fix);
            }
        } 
        else if (strncmp(sentence, "$GPGGA", 6) == 0) {
            if (parse_nmea_gga(sentence)) {
                GPS_DebugPrint("[GGA] Parsed: Sats=%d Fix=%d Alt=%.1f HDOP=%.1f\r\n", 
                    gps_data.satellites, gps_data.fix_quality, 
                    gps_data.altitude, gps_data.hdop);
            }
        }
        
        // Reset line buffer
        line_index = 0;
        memset(gps_line_buffer, 0, GPS_LINE_BUFFER_SIZE);
    }
}

/**
 * @brief Get latest GPS data (thread-safe snapshot)
 */
void GPS_GetData(gps_data_t *data)
{
    if (data != NULL) {
        memcpy(data, &gps_data, sizeof(gps_data_t));
        gps_data.new_data = false;  // Clear flag after read
    }
}

/**
 * @brief Print GPS status and all data to UART2
 */
void GPS_PrintStatus(void)
{
    GPS_DebugPrint("\r\n========== GPS STATUS ==========\r\n");
    GPS_DebugPrint("Valid Fix: %s\r\n", gps_data.valid_fix ? "YES" : "NO");
    GPS_DebugPrint("UTC Date: %02d/%02d/%04d\r\n", 
        gps_data.utc_day, gps_data.utc_month, gps_data.utc_year);
    GPS_DebugPrint("UTC Time: %02d:%02d:%02d\r\n", 
        gps_data.utc_hour, gps_data.utc_minute, gps_data.utc_second);
    GPS_DebugPrint("Latitude:  %.6f\r\n", gps_data.latitude);
    GPS_DebugPrint("Longitude: %.6f\r\n", gps_data.longitude);
    GPS_DebugPrint("Satellites: %d\r\n", gps_data.satellites);
    GPS_DebugPrint("Fix Quality: %d (0=Invalid, 1=GPS, 2=DGPS, 3=PPS, 4=RTK, 5=RTK Float)\r\n", 
        gps_data.fix_quality);
    GPS_DebugPrint("HDOP: %.1f\r\n", gps_data.hdop);
    GPS_DebugPrint("Altitude: %.1f m\r\n", gps_data.altitude);
    GPS_DebugPrint("Speed: %.1f knots\r\n", gps_data.speed_knots);
    GPS_DebugPrint("Course: %.1f degrees\r\n", gps_data.course);
    GPS_DebugPrint("================================\r\n\r\n");
}

/**
 * @brief Debug print to UART2 (like printf to TeraTerminal)
 */
void GPS_DebugPrint(const char *format, ...)
{
    static char debug_buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(debug_buffer, sizeof(debug_buffer), format, args);
    va_end(args);
    
    HAL_UART_Transmit(&DEBUG_UART, (uint8_t *)debug_buffer, strlen(debug_buffer), HAL_MAX_DELAY);
}

/**
 * @brief GPS Task (call from RTOS GPS task)
 * Periodically logs status for monitoring
 */
void GPS_Task(void)
{
    static uint32_t last_print = 0;
    uint32_t current_tick = HAL_GetTick();
    
    // Print status every 5 seconds
    if ((current_tick - last_print) > 5000) {
        GPS_PrintStatus();
        last_print = current_tick;
    }
    
    osDelay(100);  // Yield to other tasks
}

/* ============= Private Functions ============= */

/**
 * @brief Parse GPRMC sentence (Recommended Minimum Navigation Information)
 * Format: $GPRMC,hhmmss.ss,A,ddmm.mmmm,N/S,dddmm.mmmm,E/W,speed,course,ddmmyy,magvar,mode*hh
 */
static bool parse_nmea_rmc(const char *sentence)
{
    if (!validate_checksum(sentence)) {
        GPS_DebugPrint("[RMC] Checksum failed\r\n");
        return false;
    }
    
    char line[GPS_LINE_BUFFER_SIZE];
    strncpy(line, sentence, GPS_LINE_BUFFER_SIZE - 1);
    
    // Remove checksum
    char *checksum_pos = strchr(line, '*');
    if (checksum_pos) *checksum_pos = '\0';
    
    // Parse fields
    char *token = strtok(line, ",");
    if (!token) return false;  // $GPRMC
    
    token = strtok(NULL, ",");
    if (!token) return false;
    parse_utc_time(token);  // hhmmss.ss
    
    token = strtok(NULL, ",");
    if (!token) return false;
    gps_data.valid_fix = (token[0] == 'A');  // A=valid, V=invalid
    
    token = strtok(NULL, ",");
    if (!token) return false;
    char lat_str[16];
    strncpy(lat_str, token, 15);
    
    token = strtok(NULL, ",");
    if (!token) return false;
    char lat_dir = token[0];
    
    token = strtok(NULL, ",");
    if (!token) return false;
    char lon_str[16];
    strncpy(lon_str, token, 15);
    
    token = strtok(NULL, ",");
    if (!token) return false;
    char lon_dir = token[0];
    
    token = strtok(NULL, ",");
    if (!token) return false;
    gps_data.speed_knots = atof(token);  // Speed in knots
    
    token = strtok(NULL, ",");
    if (!token) return false;
    gps_data.course = atof(token);  // Course over ground
    
    token = strtok(NULL, ",");
    if (!token) return false;
    parse_utc_date(token);  // Date in ddmmyy format
    
    // Convert coordinates
    gps_data.latitude = parse_coordinate(lat_str, lat_dir);
    gps_data.longitude = parse_coordinate(lon_str, lon_dir);
    
    return true;
}

/**
 * @brief Parse GPGGA sentence (Global Positioning System Fix Data)
 * Format: $GPGGA,hhmmss.ss,ddmm.mmmm,N/S,dddmm.mmmm,E/W,quality,satellites,hdop,altitude,M,...*hh
 */
static bool parse_nmea_gga(const char *sentence)
{
    if (!validate_checksum(sentence)) {
        GPS_DebugPrint("[GGA] Checksum failed\r\n");
        return false;
    }
    
    char line[GPS_LINE_BUFFER_SIZE];
    strncpy(line, sentence, GPS_LINE_BUFFER_SIZE - 1);
    
    // Remove checksum
    char *checksum_pos = strchr(line, '*');
    if (checksum_pos) *checksum_pos = '\0';
    
    // Parse fields
    char *token = strtok(line, ",");
    if (!token) return false;  // $GPGGA
    
    token = strtok(NULL, ",");
    if (!token) return false;  // Time (skip, already from RMC)
    
    token = strtok(NULL, ",");
    if (!token) return false;  // Latitude (skip)
    
    token = strtok(NULL, ",");
    if (!token) return false;  // Lat direction (skip)
    
    token = strtok(NULL, ",");
    if (!token) return false;  // Longitude (skip)
    
    token = strtok(NULL, ",");
    if (!token) return false;  // Lon direction (skip)
    
    token = strtok(NULL, ",");
    if (!token) return false;
    gps_data.fix_quality = atoi(token);  // Fix quality
    
    token = strtok(NULL, ",");
    if (!token) return false;
    gps_data.satellites = atoi(token);  // Number of satellites
    
    token = strtok(NULL, ",");
    if (!token) return false;
    gps_data.hdop = atof(token);  // Horizontal DOP
    
    token = strtok(NULL, ",");
    if (!token) return false;
    gps_data.altitude = atof(token);  // Altitude above MSL
    
    return true;
}

/**
 * @brief Validate NMEA sentence checksum
 */
static bool validate_checksum(const char *sentence)
{
    const char *start = strchr(sentence, '$');
    const char *end = strchr(sentence, '*');
    
    if (!start || !end) return false;
    
    uint8_t expected = 0;
    for (const char *p = start + 1; p < end; p++) {
        expected ^= (uint8_t)*p;
    }
    
    char checksum_str[3];
    strncpy(checksum_str, end + 1, 2);
    checksum_str[2] = '\0';
    
    uint8_t received = (uint8_t)strtol(checksum_str, NULL, 16);
    
    return (expected == received);
}

/**
 * @brief Parse UTC time from hhmmss.ss format
 */
static bool parse_utc_time(const char *time_str)
{
    if (strlen(time_str) < 6) return false;
    
    char hour_str[3] = {time_str[0], time_str[1], '\0'};
    char min_str[3] = {time_str[2], time_str[3], '\0'};
    char sec_str[3] = {time_str[4], time_str[5], '\0'};
    
    gps_data.utc_hour = atoi(hour_str);
    gps_data.utc_minute = atoi(min_str);
    gps_data.utc_second = atoi(sec_str);
    
    return true;
}

/**
 * @brief Parse UTC date from ddmmyy format
 */
static bool parse_utc_date(const char *date_str)
{
    if (strlen(date_str) < 6) return false;
    
    char day_str[3] = {date_str[0], date_str[1], '\0'};
    char month_str[3] = {date_str[2], date_str[3], '\0'};
    char year_str[3] = {date_str[4], date_str[5], '\0'};
    
    gps_data.utc_day = atoi(day_str);
    gps_data.utc_month = atoi(month_str);
    uint8_t year_2digit = atoi(year_str);
    
    // Convert 2-digit year to 4-digit (00-99 becomes 2000-2099)
    gps_data.utc_year = 2000 + year_2digit;
    
    return true;
}

/**
 * @brief Parse coordinate from ddmm.mmmm or dddmm.mmmm format
 * Returns decimal degrees (negative if S or W)
 */
static float parse_coordinate(const char *coord_str, char direction)
{
    if (!coord_str || strlen(coord_str) < 5) return 0.0f;
    
    // Find decimal point
    char *decimal_pos = strchr(coord_str, '.');
    if (!decimal_pos) return 0.0f;
    
    // Calculate degree position (2 digits for latitude, 3 for longitude)
    int deg_len = decimal_pos - coord_str - 2;
    if (deg_len != 1 && deg_len != 2) return 0.0f;
    
    // Parse degrees
    char deg_str[4] = {0};
    strncpy(deg_str, coord_str, deg_len);
    float degrees = atof(deg_str);
    
    // Parse minutes
    char min_str[8] = {0};
    strncpy(min_str, coord_str + deg_len, decimal_pos - (coord_str + deg_len) + 5);
    float minutes = atof(min_str);
    
    // Convert to decimal degrees
    float decimal = degrees + (minutes / 60.0f);
    
    // Apply direction
    if (direction == 'S' || direction == 'W') {
        decimal = -decimal;
    }
    
    return decimal;
}

/* ============= HAL Callback (Interrupt Handler) ============= */
/**
 * @brief UART RX Complete Callback
 * Add this to main.c in the HAL_UART_RxCpltCallback function:
 * 
 * extern uint8_t gps_rx_char;
 * 
 * void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
 * {
 *     if (huart->Instance == USART1) {
 *         GPS_ProcessChar(gps_rx_char);
 *         HAL_UART_Receive_IT(&GPS_UART, &gps_rx_char, 1);
 *     }
 * }
 */
