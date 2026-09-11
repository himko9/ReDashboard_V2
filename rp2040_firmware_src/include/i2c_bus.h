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

#ifndef REDASHBOARD_RP2040_I2C_BUS_H
#define REDASHBOARD_RP2040_I2C_BUS_H

#include <stddef.h>
#include <stdint.h>

#include "board_config.h"

void i2c_bus_init(const I2CBusConfig &bus);
void i2c_bus_deinit(const I2CBusConfig &bus);
bool i2c_bus_ensure_ready(const I2CBusConfig &bus);
bool i2c_bus_write(const I2CBusConfig &bus,
                   uint8_t address,
                   const uint8_t *data,
                   size_t length,
                   bool no_stop = false);
bool i2c_bus_read(const I2CBusConfig &bus,
                  uint8_t address,
                  uint8_t *data,
                  size_t length);
bool i2c_bus_write_read_byte(const I2CBusConfig &bus,
                             uint8_t address,
                             uint8_t reg,
                             uint8_t *value_out);
const char *i2c_bus_name(i2c_inst_t *instance);

#endif
