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

#include "i2c_bus.h"

#include "pico/stdlib.h"

static void apply_pinmux(const I2CBusConfig &bus) {
    gpio_set_function(bus.sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(bus.scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(bus.sda_pin);
    gpio_pull_up(bus.scl_pin);
}

void i2c_bus_init(const I2CBusConfig &bus) {
    i2c_init(bus.inst, bus.baud_hz);
    apply_pinmux(bus);
}

void i2c_bus_deinit(const I2CBusConfig &bus) {
    i2c_deinit(bus.inst);
}

static void drive_low(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_OUT);
}

static void release_line(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

static bool is_idle(const I2CBusConfig &bus) {
    return gpio_get(bus.scl_pin) && gpio_get(bus.sda_pin);
}

static bool recover(const I2CBusConfig &bus) {
    i2c_deinit(bus.inst);
    release_line(bus.scl_pin);
    release_line(bus.sda_pin);
    sleep_us(5U);
    if (!gpio_get(bus.scl_pin)) {
        i2c_bus_init(bus);
        return false;
    }
    for (int pulse = 0; pulse < 9 && !gpio_get(bus.sda_pin); ++pulse) {
        drive_low(bus.scl_pin);
        sleep_us(5U);
        release_line(bus.scl_pin);
        sleep_us(5U);
    }
    drive_low(bus.sda_pin);
    sleep_us(5U);
    release_line(bus.scl_pin);
    sleep_us(5U);
    release_line(bus.sda_pin);
    sleep_us(5U);
    i2c_bus_init(bus);
    sleep_us(10U);
    return is_idle(bus);
}

bool i2c_bus_ensure_ready(const I2CBusConfig &bus) {
    return is_idle(bus) || recover(bus);
}

bool i2c_bus_write(const I2CBusConfig &bus,
                   uint8_t address,
                   const uint8_t *data,
                   size_t length,
                   bool no_stop) {
    absolute_time_t deadline = make_timeout_time_ms(bus.timeout_ms);
    int result = i2c_write_blocking_until(
        bus.inst, address, data, length, no_stop, deadline);
    if (result < 0 || (size_t)result != length) {
        recover(bus);
        return false;
    }
    return true;
}

bool i2c_bus_read(const I2CBusConfig &bus,
                  uint8_t address,
                  uint8_t *data,
                  size_t length) {
    absolute_time_t deadline = make_timeout_time_ms(bus.timeout_ms);
    int result = i2c_read_blocking_until(
        bus.inst, address, data, length, false, deadline);
    if (result < 0 || (size_t)result != length) {
        recover(bus);
        return false;
    }
    return true;
}

bool i2c_bus_write_read_byte(const I2CBusConfig &bus,
                             uint8_t address,
                             uint8_t reg,
                             uint8_t *value_out) {
    return i2c_bus_write(bus, address, &reg, 1U, true) &&
           i2c_bus_read(bus, address, value_out, 1U);
}

const char *i2c_bus_name(i2c_inst_t *instance) {
    return instance == i2c0 ? "i2c0" : "i2c1";
}
