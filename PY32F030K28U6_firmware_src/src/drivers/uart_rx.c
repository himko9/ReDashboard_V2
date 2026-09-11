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

#include "uart_rx.h"

#include <stddef.h>

#include "board.h"

#define UART_RX_RING_CAPACITY 128U
#define UART_RX_RING_MASK     (UART_RX_RING_CAPACITY - 1U)
#define UART_ERROR_RETRY_MS   10U

static UART_HandleTypeDef uart_handle;
static uint8_t irq_byte;
static uint8_t rx_ring[UART_RX_RING_CAPACITY];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;
static volatile uint32_t overflow_count;
static volatile uint32_t error_count;
static volatile bool retry_pending;
static volatile bool suspended;
static volatile uint32_t retry_deadline_ms;

static bool time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static void uart_disable_rx_interrupts(void)
{
    volatile uint32_t discard;

    CLEAR_BIT(uart_handle.Instance->CR1,
              USART_CR1_RXNEIE | USART_CR1_PEIE);
    CLEAR_BIT(uart_handle.Instance->CR3, USART_CR3_EIE);
    /* Clear any byte/error left by the inactive baud rate before it is
       eligible again. USART status clears after the SR-then-DR sequence. */
    discard = uart_handle.Instance->SR;
    discard = uart_handle.Instance->DR;
    (void)discard;
    uart_handle.RxState = HAL_UART_STATE_READY;
}

static bool uart_rearm(void)
{
    if (suspended) {
        return false;
    }
    if (HAL_UART_Receive_IT(&uart_handle, &irq_byte, 1U) != HAL_OK) {
        return false;
    }
    retry_pending = false;
    return true;
}

bool uart_rx_init(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    overflow_count = 0U;
    error_count = 0U;
    retry_pending = false;
    suspended = false;
    retry_deadline_ms = 0U;

    uart_handle.Instance = BOARD_POST_UART_INSTANCE;
    uart_handle.Init.BaudRate = 115200U;
    uart_handle.Init.WordLength = UART_WORDLENGTH_8B;
    uart_handle.Init.StopBits = UART_STOPBITS_1;
    uart_handle.Init.Parity = UART_PARITY_NONE;
    uart_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart_handle.Init.Mode = UART_MODE_RX;
    uart_handle.Init.OverSampling = UART_OVERSAMPLING_16;
    uart_handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&uart_handle) != HAL_OK) {
        return false;
    }
    return HAL_UART_Receive_IT(&uart_handle, &irq_byte, 1U) == HAL_OK;
}

void uart_rx_service(uint32_t now_ms, bool enabled)
{
    if (!enabled) {
        if (!suspended) {
            uart_disable_rx_interrupts();
            suspended = true;
            retry_pending = true;
        }
        return;
    }

    if (suspended) {
        suspended = false;
        retry_deadline_ms = now_ms;
    }
    if (retry_pending && time_reached(now_ms, retry_deadline_ms)) {
        uart_handle.ErrorCode = HAL_UART_ERROR_NONE;
        if (!uart_rearm()) {
            retry_deadline_ms = now_ms + UART_ERROR_RETRY_MS;
        }
    }
}

bool uart_rx_pop(uint8_t *byte)
{
    uint8_t tail = rx_tail;

    if ((tail == rx_head) || (byte == NULL)) {
        return false;
    }

    *byte = rx_ring[tail];
    rx_tail = (uint8_t)((tail + 1U) & UART_RX_RING_MASK);
    return true;
}

uint32_t uart_rx_overflow_count(void)
{
    return overflow_count;
}

uint32_t uart_rx_error_count(void)
{
    return error_count;
}

void uart_rx_irq_handler(void)
{
    HAL_UART_IRQHandler(&uart_handle);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t next;

    if (huart != &uart_handle) {
        return;
    }

    next = (uint8_t)((rx_head + 1U) & UART_RX_RING_MASK);
    if (next == rx_tail) {
        ++overflow_count;
    } else {
        rx_ring[rx_head] = irq_byte;
        rx_head = next;
    }
    (void)uart_rearm();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &uart_handle) {
        ++error_count;
        /* Quiesce after a framing/noise error; foreground retries at a
           bounded rate without affecting the independent PA1 decoder. */
        uart_disable_rx_interrupts();
        retry_pending = true;
        retry_deadline_ms = HAL_GetTick() + UART_ERROR_RETRY_MS;
    }
}
