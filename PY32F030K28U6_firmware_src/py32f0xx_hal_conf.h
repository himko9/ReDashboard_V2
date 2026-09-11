#ifndef PY32F0XX_HAL_CONF_H
#define PY32F0XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED

#ifndef HSI_VALUE
#define HSI_VALUE 8000000U
#endif
#ifndef HSE_VALUE
#define HSE_VALUE 24000000U
#endif
#ifndef HSE_STARTUP_TIMEOUT
#define HSE_STARTUP_TIMEOUT 200U
#endif
#ifndef LSI_VALUE
#define LSI_VALUE 32768U
#endif
#ifndef LSE_VALUE
#define LSE_VALUE 32768U
#endif
#ifndef LSE_STARTUP_TIMEOUT
#define LSE_STARTUP_TIMEOUT 5000U
#endif

#define VDD_VALUE          3300U
#define PRIORITY_HIGHEST   0U
#define PRIORITY_HIGH      1U
#define PRIORITY_LOW       2U
#define PRIORITY_LOWEST    3U
/* SysTick must be able to preempt a wedged peripheral IRQ so deadlines,
 * heartbeat, and recovery remain alive. */
#define TICK_INT_PRIORITY  PRIORITY_HIGHEST
#define USE_RTOS           0U
#define PREFETCH_ENABLE    0U
#define USE_HAL_I2C_REGISTER_CALLBACKS  0U
#define USE_HAL_UART_REGISTER_CALLBACKS 0U

#include "py32f0xx_hal.h"
#include "py32f0xx_hal_rcc.h"
#include "py32f0xx_hal_flash.h"
#include "py32f0xx_hal_gpio.h"
#include "py32f0xx_hal_dma.h"
#include "py32f0xx_hal_pwr.h"
#include "py32f0xx_hal_i2c.h"
#include "py32f0xx_hal_uart.h"
#include "py32f0xx_hal_cortex.h"

#define assert_param(expression) ((void)0U)

#ifdef __cplusplus
}
#endif

#endif
