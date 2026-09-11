/*
 * This file is part of the "ReDashboard_V2" distribution.
 *
 * Copyright (C) 2026 @himko9 <me@himko.dev>
 * Github: https://github.com/himko9/ReDashboard_V2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef EZDASH_BOARD_H
#define EZDASH_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "py32f0xx_hal.h"

/*
 * PY32F030K28U6 (QFN32 pinout 2) assignment.
 * Keep all PCB-dependent choices in this file.
 */
#define BOARD_SIO_I2C_INSTANCE       I2C
#define BOARD_SIO_I2C_SCL_PORT       GPIOA
#define BOARD_SIO_I2C_SCL_PIN        GPIO_PIN_8   /* package pin 18, AF12 */
#define BOARD_SIO_I2C_SDA_PORT       GPIOA
#define BOARD_SIO_I2C_SDA_PIN        GPIO_PIN_7   /* package pin 13, AF12 */
#define BOARD_SIO_I2C_GPIO_AF        GPIO_AF12_I2C

#define BOARD_POST_UART_INSTANCE     USART1
#define BOARD_POST_UART_RX_PORT      GPIOB
#define BOARD_POST_UART_RX_PIN       GPIO_PIN_2   /* package pin 17, AF0 */
#define BOARD_POST_UART_GPIO_AF      GPIO_AF0_USART1

#define BOARD_FAST_RX_TIMER          TIM1
#define BOARD_FAST_RX_PORT           GPIOA
#define BOARD_FAST_RX_PIN            GPIO_PIN_1   /* package pin 7, TIM1_CH4 AF13 */
#define BOARD_FAST_RX_GPIO_AF        ((uint8_t)0x0DU)

#define BOARD_HEARTBEAT_LED_PORT     GPIOA
#define BOARD_HEARTBEAT_LED_PIN      GPIO_PIN_2   /* package pin 8 */
#define BOARD_HEARTBEAT_ACTIVE_HIGH  0

#define BOARD_OLED_SCL_PORT          GPIOB
#define BOARD_OLED_SCL_PIN           GPIO_PIN_6   /* package pin 29 */
#define BOARD_OLED_SDA_PORT          GPIOB
#define BOARD_OLED_SDA_PIN           GPIO_PIN_7   /* package pin 30 */
#define BOARD_OLED_ROTATION          0U            /* supported: 0 or 180 */

#define BOARD_BUTTON_PREV_PORT       GPIOA
#define BOARD_BUTTON_PREV_PIN        GPIO_PIN_5   /* package pin 11, SW1 */
#define BOARD_BUTTON_NEXT_PORT       GPIOA
#define BOARD_BUTTON_NEXT_PIN        GPIO_PIN_6   /* package pin 12, SW2 */

#define BOARD_OUTPUT_PWR_PORT        GPIOB
#define BOARD_OUTPUT_PWR_PIN         GPIO_PIN_3   /* package pin 26 */
#define BOARD_OUTPUT_RST_PORT        GPIOB
#define BOARD_OUTPUT_RST_PIN         GPIO_PIN_4   /* package pin 27 */
#define BOARD_OUTPUT_CLR_PORT        GPIOB
#define BOARD_OUTPUT_CLR_PIN         GPIO_PIN_5   /* package pin 28 */
#define BOARD_OUTPUT_ACTIVE_LOW      1

#define BOARD_SOFT_I2C_HALF_PERIOD_US 1U

void board_init(void);
void board_delay_us(uint32_t delay_us);

bool board_button_prev_pressed(void);
bool board_button_next_pressed(void);

void board_output_power(bool active);
void board_output_reset(bool active);
void board_output_clear(bool active);
void board_heartbeat_systick(void);

void board_sio_i2c_pins_as_gpio(void);
void board_sio_i2c_pins_as_peripheral(void);

#endif
