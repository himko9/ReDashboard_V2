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

#ifndef REDASHBOARD_RP2040_SENSORS_H
#define REDASHBOARD_RP2040_SENSORS_H

#include <stddef.h>
#include <stdint.h>

#include "app_types.h"
#include "sio_adc_config.h"
#include "sio_fan_config.h"

using SensorCalculate = int32_t (*)(uint16_t raw);

struct SensorDefinition {
    uint8_t enabled;
    uint8_t type;
    uint8_t index;
    const char *label;
    SensorCalculate calculate;
};

#define COUNT_SENSOR_ENTRY(id, enabled, type, index, label, calculate) +1U
inline constexpr size_t SENSOR_COUNT =
    0U SENSOR_TABLE(COUNT_SENSOR_ENTRY) SIO_TACH_TABLE(COUNT_SENSOR_ENTRY);
#undef COUNT_SENSOR_ENTRY

static_assert(SENSOR_COUNT <= 32U,
              "sensor validity mask supports at most 32 entries");

extern const SensorDefinition SENSORS[SENSOR_COUNT];

bool sensor_matches_page(size_t sensor_index, DisplayPage page);
bool first_sensor_for_page(DisplayPage page, size_t *sensor_index);
size_t next_sensor_for_page(DisplayPage page, size_t current);
bool read_sensor_value(size_t sensor_index, int32_t *calculated_out);
void format_sensor_value(size_t sensor_index,
                         int32_t calculated,
                         char value[9]);
DisplayPage configured_default_page();
DisplayPage adjacent_display_page(DisplayPage page, bool previous);

#endif
