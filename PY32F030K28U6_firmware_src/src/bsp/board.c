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

#include "board.h"

#define HEARTBEAT_TOGGLE_TICKS 500U

static volatile bool heartbeat_ready;
static volatile uint16_t heartbeat_ticks;

static void board_clock_config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clocks = {0};

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    oscillator.HSIState = RCC_HSI_ON;
#if defined(RCC_HSIDIV_SUPPORT)
    oscillator.HSIDiv = RCC_HSI_DIV1;
#endif
    oscillator.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
    oscillator.PLL.PLLState = RCC_PLL_ON;
    oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        for (;;) {
        }
    }

    clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_1) != HAL_OK) {
        for (;;) {
        }
    }
}

static void board_output_write(GPIO_TypeDef *port, uint16_t pin, bool active)
{
    bool high = BOARD_OUTPUT_ACTIVE_LOW ? !active : active;
    HAL_GPIO_WritePin(port, pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    if (HAL_Init() != HAL_OK) {
        for (;;) {
        }
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Start the heartbeat before clock and peripheral initialization. */
    HAL_GPIO_WritePin(BOARD_HEARTBEAT_LED_PORT,
                      BOARD_HEARTBEAT_LED_PIN,
                      BOARD_HEARTBEAT_ACTIVE_HIGH ? GPIO_PIN_RESET : GPIO_PIN_SET);
    gpio.Pin = BOARD_HEARTBEAT_LED_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BOARD_HEARTBEAT_LED_PORT, &gpio);
    heartbeat_ticks = 0U;
    heartbeat_ready = true;

    board_clock_config();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Set inactive levels before changing the MOSFET-control pins to outputs. */
    board_output_power(false);
    board_output_reset(false);
    board_output_clear(false);

    gpio.Pin = BOARD_OUTPUT_PWR_PIN | BOARD_OUTPUT_RST_PIN | BOARD_OUTPUT_CLR_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = BOARD_BUTTON_PREV_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BOARD_BUTTON_PREV_PORT, &gpio);

    gpio.Pin = BOARD_BUTTON_NEXT_PIN;
    HAL_GPIO_Init(BOARD_BUTTON_NEXT_PORT, &gpio);
}

void board_delay_us(uint32_t delay_us)
{
    uint32_t required_ticks = 48U * delay_us;
    uint32_t period_ticks = SysTick->LOAD + 1U;
    uint32_t previous = SysTick->VAL;
    uint32_t elapsed = 0U;

    /* The board clock is fixed at 48 MHz; interrupts may only lengthen this. */
    while (elapsed < required_ticks) {
        uint32_t current = SysTick->VAL;
        if (current <= previous) {
            elapsed += previous - current;
        } else {
            elapsed += previous + period_ticks - current;
        }
        previous = current;
    }
}

bool board_button_prev_pressed(void)
{
    return HAL_GPIO_ReadPin(BOARD_BUTTON_PREV_PORT, BOARD_BUTTON_PREV_PIN) == GPIO_PIN_RESET;
}

bool board_button_next_pressed(void)
{
    return HAL_GPIO_ReadPin(BOARD_BUTTON_NEXT_PORT, BOARD_BUTTON_NEXT_PIN) == GPIO_PIN_RESET;
}

void board_output_power(bool active)
{
    board_output_write(BOARD_OUTPUT_PWR_PORT, BOARD_OUTPUT_PWR_PIN, active);
}

void board_output_reset(bool active)
{
    board_output_write(BOARD_OUTPUT_RST_PORT, BOARD_OUTPUT_RST_PIN, active);
}

void board_output_clear(bool active)
{
    board_output_write(BOARD_OUTPUT_CLR_PORT, BOARD_OUTPUT_CLR_PIN, active);
}

void board_heartbeat_systick(void)
{
    if (!heartbeat_ready) {
        return;
    }
    ++heartbeat_ticks;
    if (heartbeat_ticks >= HEARTBEAT_TOGGLE_TICKS) {
        heartbeat_ticks = 0U;
        HAL_GPIO_TogglePin(BOARD_HEARTBEAT_LED_PORT, BOARD_HEARTBEAT_LED_PIN);
    }
}

void board_sio_i2c_pins_as_gpio(void)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(GPIOA,
                      BOARD_SIO_I2C_SCL_PIN | BOARD_SIO_I2C_SDA_PIN,
                      GPIO_PIN_SET);
    gpio.Pin = BOARD_SIO_I2C_SCL_PIN | BOARD_SIO_I2C_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void board_sio_i2c_pins_as_peripheral(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = BOARD_SIO_I2C_SCL_PIN | BOARD_SIO_I2C_SDA_PIN;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = BOARD_SIO_I2C_GPIO_AF;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != BOARD_SIO_I2C_INSTANCE) {
        return;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_I2C_CLK_ENABLE();
    board_sio_i2c_pins_as_peripheral();

    /* Puya recommends resetting I2C after its GPIO is configured. */
    __HAL_RCC_I2C_FORCE_RESET();
    __HAL_RCC_I2C_RELEASE_RESET();

    HAL_NVIC_ClearPendingIRQ(I2C1_IRQn);
    HAL_NVIC_SetPriority(I2C1_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(I2C1_IRQn);
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != BOARD_SIO_I2C_INSTANCE) {
        return;
    }
    HAL_NVIC_DisableIRQ(I2C1_IRQn);
    HAL_NVIC_ClearPendingIRQ(I2C1_IRQn);
    __HAL_RCC_I2C_FORCE_RESET();
    __HAL_RCC_I2C_RELEASE_RESET();
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance != BOARD_POST_UART_INSTANCE) {
        return;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    gpio.Pin = BOARD_POST_UART_RX_PIN;
    gpio.Alternate = BOARD_POST_UART_GPIO_AF;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(BOARD_POST_UART_RX_PORT, &gpio);

    HAL_NVIC_SetPriority(USART1_IRQn, 3U, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == BOARD_POST_UART_INSTANCE) {
        HAL_NVIC_DisableIRQ(USART1_IRQn);
        HAL_GPIO_DeInit(BOARD_POST_UART_RX_PORT, BOARD_POST_UART_RX_PIN);
        __HAL_RCC_USART1_FORCE_RESET();
        __HAL_RCC_USART1_RELEASE_RESET();
    }
}
