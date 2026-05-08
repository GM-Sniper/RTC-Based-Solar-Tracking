#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include "main.h"

/* Servo control interface */
void Servo_Init(void);
void Servo_SetAngle(uint8_t channel, float angle_deg);
void Servo_Task(void *argument);

/* Configuration */
#define SERVO_CHANNEL_TILT    TIM_CHANNEL_1  /* Tilt / Elevation */
#define SERVO_CHANNEL_PAN     TIM_CHANNEL_2  /* Pan / Azimuth */

#endif // SERVO_H
