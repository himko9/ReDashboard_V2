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

#ifndef EZDASH_UART_PARSER_H
#define EZDASH_UART_PARSER_H

#include <stdbool.h>
#include <stdint.h>

/* The SIO wire protocol carries one complete POST code in each UART byte. */
bool uart_postcode_decode(uint8_t byte, uint8_t *code);

typedef struct {
    uint64_t raw;
    uint16_t port;
    uint8_t width;
    uint32_t payload;
    uint8_t data;
} UartFastRecord;

bool uart_fast_record_decode(uint64_t raw, UartFastRecord *record);

#endif
