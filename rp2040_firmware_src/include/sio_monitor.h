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

#ifndef REDASHBOARD_RP2040_SIO_MONITOR_H
#define REDASHBOARD_RP2040_SIO_MONITOR_H

#include <stdint.h>

bool read_sio_register(uint8_t page, uint8_t index, uint8_t *value_out);
bool read_sio_monitor_value(uint8_t index,
                            bool word_value,
                            uint16_t *value_out);
bool read_post_code_p80(uint8_t *pointer_out, uint8_t *port80_out);
bool read_post_code(uint8_t *pointer_out,
                    uint8_t *port80_out,
                    uint8_t *port81_pointer_out,
                    uint8_t *port81_out);

#endif
