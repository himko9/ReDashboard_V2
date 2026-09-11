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

#ifndef EZDASH_UART_FAST_RX_H
#define EZDASH_UART_FAST_RX_H

#include <stdbool.h>
#include <stdint.h>

bool uart_fast_rx_init(void);
void uart_fast_rx_service(void);
void uart_fast_rx_systick(void);
void uart_fast_rx_pendsv(void);
bool uart_fast_rx_pop(uint64_t *raw);
uint32_t uart_fast_rx_record_count(void);
uint32_t uart_fast_rx_error_count(void);
uint32_t uart_fast_rx_overflow_count(void);
void uart_fast_rx_dma_irq_handler(void);

#endif
