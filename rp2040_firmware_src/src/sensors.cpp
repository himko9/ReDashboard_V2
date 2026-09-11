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

#include "sensors.h"

#include <stdio.h>

#include "app_config.h"
#include "display_config.h"
#include "sio_monitor.h"

#define DEFINE_SENSOR_CALCULATOR(id, enabled, type, index, label, calculate) \
    static int32_t sensor_calculate_##id(uint16_t raw) {                    \
        return (int32_t)(calculate(raw));                                   \
    }
SENSOR_TABLE(DEFINE_SENSOR_CALCULATOR)
SIO_TACH_TABLE(DEFINE_SENSOR_CALCULATOR)
#undef DEFINE_SENSOR_CALCULATOR

#define DEFINE_SENSOR_ENTRY(id, enabled, type, index, label, calculate) \
    {(enabled), (type), (index), (label), sensor_calculate_##id},
const SensorDefinition SENSORS[SENSOR_COUNT] = {
    SENSOR_TABLE(DEFINE_SENSOR_ENTRY)
    SIO_TACH_TABLE(DEFINE_SENSOR_ENTRY)
};
#undef DEFINE_SENSOR_ENTRY

bool sensor_matches_page(size_t sensor_index, DisplayPage page) {
    if (sensor_index >= SENSOR_COUNT || SENSORS[sensor_index].enabled == 0U) {
        return false;
    }
    if (page == DisplayPage::Voltage) {
#if DISPLAY_VOLTAGE_PAGE_ENABLED == 0U
        return false;
#else
        return SENSORS[sensor_index].type == SENSOR_TYPE_VIN;
#endif
    }
    if (page == DisplayPage::Temperature) {
#if DISPLAY_TEMPERATURE_PAGE_ENABLED == 0U
        return false;
#else
        return SENSORS[sensor_index].type == SENSOR_TYPE_THR;
#endif
    }
    if (page == DisplayPage::Fan) {
#if DISPLAY_FAN_PAGE_ENABLED == 0U
        return false;
#else
        return SENSORS[sensor_index].type == SENSOR_TYPE_TACH;
#endif
    }
    return false;
}

bool first_sensor_for_page(DisplayPage page, size_t *sensor_index) {
    for (size_t index = 0U; index < SENSOR_COUNT; ++index) {
        if (sensor_matches_page(index, page)) {
            *sensor_index = index;
            return true;
        }
    }
    return false;
}

size_t next_sensor_for_page(DisplayPage page, size_t current) {
    for (size_t offset = 1U; offset <= SENSOR_COUNT; ++offset) {
        size_t candidate = (current + offset) % SENSOR_COUNT;
        if (sensor_matches_page(candidate, page)) return candidate;
    }
    return current;
}

bool read_sensor_value(size_t sensor_index, int32_t *calculated_out) {
    if (sensor_index >= SENSOR_COUNT || calculated_out == nullptr) {
        return false;
    }
    uint16_t raw = 0U;
    const bool word_value =
        SENSORS[sensor_index].type != SENSOR_TYPE_VIN;
    if (!read_sio_monitor_value(SENSORS[sensor_index].index,
                                word_value, &raw)) {
        return false;
    }
    *calculated_out = SENSORS[sensor_index].calculate(raw);
    return true;
}

static void format_voltage(int32_t calculated_millivolts, char value[9]) {
    uint32_t millivolts = calculated_millivolts > 0
                              ? (uint32_t)calculated_millivolts
                              : 0U;
    uint32_t rounded_units;
    uint32_t whole;
#if SENSOR_VOLTAGE_DECIMAL_DIGITS > 0U
    uint32_t fraction;
#endif
    uint8_t offset = 0U;

#if SENSOR_VOLTAGE_DECIMAL_DIGITS == 0U
    rounded_units = (millivolts + 500U) / 1000U;
    whole = rounded_units;
#elif SENSOR_VOLTAGE_DECIMAL_DIGITS == 1U
    rounded_units = (millivolts + 50U) / 100U;
    whole = rounded_units / 10U;
    fraction = rounded_units % 10U;
#elif SENSOR_VOLTAGE_DECIMAL_DIGITS == 2U
    rounded_units = (millivolts + 5U) / 10U;
    whole = rounded_units / 100U;
    fraction = rounded_units % 100U;
#else
    rounded_units = millivolts;
    whole = rounded_units / 1000U;
    fraction = rounded_units % 1000U;
#endif
    if (whole >= 100U) value[offset++] = (char)('0' + whole / 100U % 10U);
    if (whole >= 10U) value[offset++] = (char)('0' + whole / 10U % 10U);
    value[offset++] = (char)('0' + whole % 10U);
#if SENSOR_VOLTAGE_DECIMAL_DIGITS > 0U
    value[offset++] = '.';
#if SENSOR_VOLTAGE_DECIMAL_DIGITS == 3U
    value[offset++] = (char)('0' + fraction / 100U);
#endif
#if SENSOR_VOLTAGE_DECIMAL_DIGITS >= 2U
    value[offset++] = (char)('0' + fraction / 10U % 10U);
#endif
    value[offset++] = (char)('0' + fraction % 10U);
#endif
    value[offset++] = 'V';
    value[offset] = '\0';
}

static void format_temperature(int32_t tenths_celsius, char value[9]) {
    int32_t tenths;
#if SENSOR_TEMP_UNIT == 'F'
    int32_t delta = tenths_celsius * 9;
    tenths = (delta >= 0 ? (delta + 2) / 5 : (delta - 2) / 5) + 320;
#else
    tenths = tenths_celsius;
#endif
    uint32_t magnitude =
        tenths < 0 ? (uint32_t)(-tenths) : (uint32_t)tenths;
    uint32_t whole = magnitude / 10U;
    uint8_t offset = 0U;
    if (tenths < 0) value[offset++] = '-';
    if (whole >= 100U) value[offset++] = (char)('0' + whole / 100U % 10U);
    if (whole >= 10U) value[offset++] = (char)('0' + whole / 10U % 10U);
    value[offset++] = (char)('0' + whole % 10U);
    value[offset++] = '.';
    value[offset++] = (char)('0' + magnitude % 10U);
    value[offset++] = '^';
    value[offset++] = SENSOR_TEMP_UNIT;
    value[offset] = '\0';
}

static void format_rpm(int32_t calculated_rpm, char value[9]) {
    uint32_t rpm = calculated_rpm > 0 ? (uint32_t)calculated_rpm : 0U;
    if (rpm > UINT16_MAX) rpm = UINT16_MAX;
    snprintf(value, 9U, "%luRPM", (unsigned long)rpm);
}

void format_sensor_value(size_t sensor_index,
                         int32_t calculated,
                         char value[9]) {
    if (SENSORS[sensor_index].type == SENSOR_TYPE_VIN) {
        format_voltage(calculated, value);
    } else if (SENSORS[sensor_index].type == SENSOR_TYPE_THR) {
        format_temperature(calculated, value);
    } else {
        format_rpm(calculated, value);
    }
}

DisplayPage configured_default_page() {
#if DEFAULT_BOOT_SCREEN == BOOT_SCREEN_VOLTAGE
    return DisplayPage::Voltage;
#elif DEFAULT_BOOT_SCREEN == BOOT_SCREEN_TEMPERATURE
    return DisplayPage::Temperature;
#elif DEFAULT_BOOT_SCREEN == BOOT_SCREEN_FAN
    return DisplayPage::Fan;
#else
    return DisplayPage::Post;
#endif
}

DisplayPage adjacent_display_page(DisplayPage page, bool previous) {
    if (previous) {
        switch (page) {
            case DisplayPage::Post: return DisplayPage::Fan;
            case DisplayPage::Temperature: return DisplayPage::Post;
            case DisplayPage::Voltage: return DisplayPage::Temperature;
            case DisplayPage::Fan: return DisplayPage::Voltage;
        }
    }
    switch (page) {
        case DisplayPage::Post: return DisplayPage::Temperature;
        case DisplayPage::Temperature: return DisplayPage::Voltage;
        case DisplayPage::Voltage: return DisplayPage::Fan;
        case DisplayPage::Fan: return DisplayPage::Post;
    }
    return DisplayPage::Post;
}
