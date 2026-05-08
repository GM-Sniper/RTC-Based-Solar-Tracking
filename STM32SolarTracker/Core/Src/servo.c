#include "servo.h"
#include "cmsis_os.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* External TIM1 handle (defined in main.c) */
extern TIM_HandleTypeDef htim1;

/* Servo state */
static struct {
    float tilt_angle;   /* Tilt servo angle (0-180 degrees) */
    float pan_angle;    /* Pan servo angle (0-180 degrees) */
    bool initialized;
} servo_state = {0};

/**
 * @brief Convert angle (0-180 degrees) to PWM pulse width in timer counts.
 * 
 * TIM1 at 50Hz with Prescaler=31, Period=19999:
 * - Timer frequency: 32MHz / 32 = 1MHz (1 count = 1us)
 * - Period: 20ms (20000 counts for full period)
 * - Servo pulse: 0.5ms (500 counts) to 2.5ms (2500 counts)
 * - 0 degrees   = 0.5ms  = 500 counts (min)
 * - 90 degrees  = 1.5ms  = 1500 counts (center)
 * - 180 degrees = 2.5ms  = 2500 counts (max)
 */
static uint32_t angle_to_pulse_width(float angle_deg)
{
    /* Clamp angle to valid range */
    if (angle_deg < 0.0f) {
        angle_deg = 0.0f;
    }
    if (angle_deg > 180.0f) {
        angle_deg = 180.0f;
    }

    /* Map 0-180 degrees to 500-2500 counts (0.5ms to 2.5ms) */
    uint32_t pulse_width = 500 + (uint32_t)((angle_deg / 180.0f) * 2000.0f);
    return pulse_width;
}

/**
 * @brief Initialize servo control on TIM1.
 * Assumes TIM1 is already configured as PWM at 50Hz by STM32CubeMX.
 */
void Servo_Init(void)
{
    if (servo_state.initialized) {
        return;
    }

    /* Start PWM on both channels */
    if (HAL_TIM_PWM_Start(&htim1, SERVO_CHANNEL_TILT) != HAL_OK) {
        printf("[SERVO] Failed to start PWM on tilt channel\r\n");
        return;
    }

    if (HAL_TIM_PWM_Start(&htim1, SERVO_CHANNEL_PAN) != HAL_OK) {
        printf("[SERVO] Failed to start PWM on pan channel\r\n");
        HAL_TIM_PWM_Stop(&htim1, SERVO_CHANNEL_TILT);
        return;
    }

    /* Initialize to center position */
    servo_state.tilt_angle = 90.0f;
    servo_state.pan_angle = 90.0f;
    servo_state.initialized = true;

    printf("[SERVO] Initialized on TIM1 (50Hz)\r\n");
}

/**
 * @brief Set servo angle for a given channel.
 * @param channel: SERVO_CHANNEL_TILT or SERVO_CHANNEL_PAN
 * @param angle_deg: 0-180 degrees
 */
void Servo_SetAngle(uint8_t channel, float angle_deg)
{
    if (!servo_state.initialized) {
        printf("[SERVO] Not initialized\r\n");
        return;
    }

    uint32_t pulse = angle_to_pulse_width(angle_deg);

    /* Update the PWM pulse width (CCR register) */
    switch (channel) {
        case SERVO_CHANNEL_TILT:
            __HAL_TIM_SET_COMPARE(&htim1, SERVO_CHANNEL_TILT, pulse);
            servo_state.tilt_angle = angle_deg;
            printf("[SERVO] Tilt set to %.1f degrees\r\n", angle_deg);
            break;

        case SERVO_CHANNEL_PAN:
            __HAL_TIM_SET_COMPARE(&htim1, SERVO_CHANNEL_PAN, pulse);
            servo_state.pan_angle = angle_deg;
            printf("[SERVO] Pan set to %.1f degrees\r\n", angle_deg);
            break;

        default:
            printf("[SERVO] Invalid channel %u\r\n", channel);
            break;
    }
}

/**
 * @brief Servo task for RTOS (optional periodic servo control).
 * Called from StartServoTask in main.c.
 */
void Servo_Task(void *argument)
{
    /* Initialize servo control */
    Servo_Init();

    /* Move both servos to center position initially */
    Servo_SetAngle(SERVO_CHANNEL_TILT, 90.0f);
    Servo_SetAngle(SERVO_CHANNEL_PAN, 90.0f);

    printf("[SERVO] Task running\r\n");

    /* Periodic servo management (can be extended with solar tracking logic) */
    while (1) {
        /* Place any periodic servo updates here */
        /* For now, just yield to other tasks */
        osDelay(1000);
    }
}
