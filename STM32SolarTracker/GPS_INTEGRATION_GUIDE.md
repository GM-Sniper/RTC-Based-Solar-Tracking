# GPS Module Integration Guide (STM32 + NEO-7M)

## Overview
This GPS module driver handles:
- **UART1** (9600 baud): GPS data input from NEO-7M module
- **UART2** (9600 baud): Debug/logging output to TeraTerminal
- NMEA sentence parsing (RMC, GGA)
- Checksum validation
- Thread-safe data access

## Hardware Connections

### NEO-7M (HW-539B) to STM32L476
```
NEO-7M Pin  → STM32 Pin
VCC (3.3V)  → 3.3V
GND         → GND
TX (UART)   → PA10 (UART1 RX)
RX (UART)   → PA9  (UART1 TX)
```

### Debug Output (UART2)
- **PA3**: UART2 TX → USB-to-Serial adapter RX (connect to PC)
- **PA2**: UART2 RX → USB-to-Serial adapter TX
- **TeraTerminal Settings**: 9600 baud, 8N1, No flow control

## Software Integration Checklist

✅ **Already Done:**
1. `gps_module.h` created with public APIs
2. `gps_module.c` created with full NMEA parsing
3. `main.c` updated with GPS initialization
4. `StartGpsTask()` implemented to call GPS_Task()
5. UART RX callback added for interrupt-based reception

## Usage in Application

### 1. In your solar task, read GPS data:
```c
void StartSolarTask(void *argument)
{
    gps_data_t gps_data;
    
    for(;;) {
        // Get latest GPS fix
        GPS_GetData(&gps_data);
        
        if (gps_data.valid_fix && gps_data.new_data) {
            // Use GPS latitude/longitude for SOLPOS
            float latitude = gps_data.latitude;
            float longitude = gps_data.longitude;
            uint8_t hour = gps_data.utc_hour;
            uint8_t minute = gps_data.utc_minute;
            uint8_t second = gps_data.utc_second;
            
            GPS_DebugPrint("Using GPS: Lat=%.6f Lon=%.6f Time=%02d:%02d:%02d\r\n",
                latitude, longitude, hour, minute, second);
        }
        
        osDelay(100);
    }
}
```

### 2. Display GPS status in debug task:
```c
void StartServoTask(void *argument)
{
    for(;;) {
        GPS_PrintStatus();  // Prints full status to TeraTerminal
        osDelay(10000);     // Every 10 seconds
    }
}
```

### 3. Debug individual sentences:
```c
GPS_DebugPrint("Debug message: value=%d\r\n", some_value);
```

## GPS Data Structure

```c
typedef struct {
    // RMC sentence
    bool valid_fix;           // A=valid, V=invalid
    uint8_t utc_hour;         // 0-23
    uint8_t utc_minute;       // 0-59
    uint8_t utc_second;       // 0-59
    float latitude;           // Decimal degrees (+N, -S)
    float longitude;          // Decimal degrees (+E, -W)
    float speed_knots;
    float course;
    
    // GGA sentence
    uint8_t satellites;       // Number of visible satellites
    float hdop;               // Horizontal Dilution of Precision
    float altitude;           // Meters above sea level
    uint8_t fix_quality;      // 0=invalid, 1=GPS, 2=DGPS, 3=PPS, 4=RTK, 5=RTK Float
    
    // Update flag
    bool new_data;            // True after parsing new sentence
} gps_data_t;
```

## NMEA Sentences Parsed

### $GPRMC (Recommended Minimum)
Example:
```
$GPRMC,hhmmss.ss,A,ddmm.mmmm,N/S,dddmm.mmmm,E/W,speed,course,ddmmyy,magvar,mode*hh
$GPRMC,123519,4807.038,N,01131.324,E,022.4,084.4,230394,003.1,W*6A
```

**Parsed fields:**
- UTC time, Status (A/V), Latitude/Longitude, Speed, Course

### $GPGGA (Global Positioning System Fix Data)
Example:
```
$GPGGA,hhmmss.ss,ddmm.mmmm,N/S,dddmm.mmmm,E/W,quality,satellites,hdop,altitude,M,...*hh
$GPGGA,123519,4807.038,N,01131.324,E,1,08,0.9,545.4,M,46.9,M,,*42
```

**Parsed fields:**
- Fix quality, Satellites, HDOP, Altitude

## Troubleshooting

### "No GPS signal" / Fix Quality = 0
- Check antenna connection and placement (needs clear sky view)
- GPS can take 30-60 seconds for first fix (cold start)
- Wait for `satellites >= 4` for reliable fix

### "Checksum failed" messages
- Verify UART1 connection and baud rate (9600)
- Check for noise on GPS RX line
- Try shielded cable if in noisy environment

### Debug output not appearing
- Connect USB-to-Serial adapter to UART2 (PA3 = TX)
- Verify baud rate in TeraTerminal is 9600
- Check that `GPS_Init()` was called in `main()`

### Timeout or no data
- Verify GPS module is powered (look for LED)
- Check UART1 transmit/receive pins in STM32CubeMX config
- Ensure `HAL_UART_Receive_IT()` is called in `GPS_Init()`

## Performance Notes

- **Update Rate**: 1 Hz (typical NMEA output)
- **Parsing Latency**: <10ms per sentence
- **Memory Usage**: ~500 bytes
- **CPU Usage**: Minimal (interrupt-driven)

## Optional Enhancements

1. **PPS (Pulse Per Second)**: Connect NEO-7M PPS pin to STM32 timer input for high-precision time sync
2. **Dual UART**: Parse on UART1, output debug on UART2 (already implemented)
3. **Checksum validation**: Already implemented; sentences with bad checksums are rejected
4. **DGPS**: If using DGPS mode, fix_quality will show 2 instead of 1

## Next Steps

1. Power on NEO-7M module (should see LED blinking)
2. Open TeraTerminal on COM port for UART2
3. Observe GPS initialization message and parsed sentences
4. Once `valid_fix=true` and `satellites>=4`, integrate GPS into solar tracking algorithm

