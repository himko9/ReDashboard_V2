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

#include "py32f0xx_hal.h"

#include "board.h"
#include "sio_i2c_hw.h"
#include "uart_fast_rx.h"
#include "uart_rx.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    /* Do not leave a deployed board permanently trapped with no heartbeat. */
    NVIC_SystemReset();
    for (;;) {
    }
}

void SVC_Handler(void)
{
}

void PendSV_Handler(void)
{
    uart_fast_rx_pendsv();
}

void SysTick_Handler(void)
{
    HAL_IncTick();
    board_heartbeat_systick();
    /* Schedule a low-priority DMA drain at a guaranteed 1 ms cadence.
       Foreground OLED/I2C work can otherwise leave more than one circular
       buffer of custom-UART edges pending. */
    uart_fast_rx_systick();
}

void I2C1_IRQHandler(void)
{
    sio_i2c_irq_handler();
}

void USART1_IRQHandler(void)
{
    uart_rx_irq_handler();
}

void DMA1_Channel1_IRQHandler(void)
{
    uart_fast_rx_dma_irq_handler();
}

void DMA1_Channel2_3_IRQHandler(void)
{
    uart_fast_rx_dma_irq_handler();
}
