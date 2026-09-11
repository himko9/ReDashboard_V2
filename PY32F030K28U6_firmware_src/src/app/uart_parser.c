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

#include "uart_parser.h"

#include <stddef.h>

bool uart_postcode_decode(uint8_t byte, uint8_t *code)
{
    if (code == NULL) {
        return false;
    }
    *code = byte;
    return true;
}

bool uart_fast_record_decode(uint64_t raw, UartFastRecord *record)
{
    uint16_t port;

    if (record == NULL) {
        return false;
    }
    port = (uint16_t)(raw & 0xFFFFU);

    if (((raw >> 50U) != 0U) ||
        ((port != 0x0080U) && (port != 0x0081U))) {
        return false;
    }

    record->raw = raw;
    record->port = port;
    record->width = (uint8_t)((raw >> 16U) & 0x03U);
    if (record->width > 2U) {
        return false;
    }
    record->payload = (uint32_t)((raw >> 18U) & 0xFFFFFFFFULL);
    record->data = (uint8_t)(record->payload & 0xFFU);
    return true;
}
