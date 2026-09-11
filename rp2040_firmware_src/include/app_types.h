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

#ifndef REDASHBOARD_RP2040_APP_TYPES_H
#define REDASHBOARD_RP2040_APP_TYPES_H

#include <stdint.h>

struct PostCode {
    uint16_t value;
    uint8_t width_bytes;
};

enum class DataSourceMode : uint8_t {
    I2cPoll,
    UartInput,
};

enum class ButtonRoleMode : uint8_t {
    ScrollBrowse,
    GpioControl,
};

enum class DisplayPage : uint8_t {
    Post,
    Voltage,
    Temperature,
    Fan,
};

#endif
