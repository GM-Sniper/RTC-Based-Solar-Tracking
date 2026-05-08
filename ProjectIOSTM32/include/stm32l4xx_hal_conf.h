#ifndef STM32L4xx_HAL_CONF_H
#define STM32L4xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED

#if !defined(HSE_VALUE)
  #define HSE_VALUE ((uint32_t)8000000U)
#endif

#if !defined(MSI_VALUE)
  #define MSI_VALUE ((uint32_t)4000000U)
#endif

#if !defined(HSI_VALUE)
  #define HSI_VALUE ((uint32_t)16000000U)
#endif

#if !defined(LSI_VALUE)
  #define LSI_VALUE 32000U
#endif

#if !defined(LSE_VALUE)
  #define LSE_VALUE 32768U
#endif

#define VDD_VALUE                 3300U
#define TICK_INT_PRIORITY         15U
#define USE_RTOS                  1U
#define PREFETCH_ENABLE           0U
#define INSTRUCTION_CACHE_ENABLE  1U
#define DATA_CACHE_ENABLE         1U

#define USE_HAL_TIM_REGISTER_CALLBACKS   0U
#define USE_HAL_UART_REGISTER_CALLBACKS  0U
#define USE_HAL_GPIO_REGISTER_CALLBACKS  0U
#define USE_HAL_RCC_REGISTER_CALLBACKS   0U

#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_rcc_ex.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_dma.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx_hal_flash.h"
#include "stm32l4xx_hal_flash_ex.h"
#include "stm32l4xx_hal_pwr.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_exti.h"
#include "stm32l4xx_hal_tim.h"
#include "stm32l4xx_hal_tim_ex.h"
#include "stm32l4xx_hal_uart.h"

#ifdef __cplusplus
}
#endif

#endif /* STM32L4xx_HAL_CONF_H */