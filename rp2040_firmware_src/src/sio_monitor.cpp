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

#include "sio_monitor.h"

#include "board_config.h"
#include "display_config.h"
#include "i2c_bus.h"
#include "pico/stdlib.h"

static bool read_sio_register_on_bus(const I2CBusConfig &bus,
                                     uint8_t page,
                                     uint8_t index,
                                     uint8_t *value_out,
                                     uint stage_gap_us) {
    const uint8_t page_select[] = {0xFFU, page};
    const uint8_t index_select[] = {0xFEU, index};

    if (!i2c_bus_write(bus, SIO_I2C_ADDRESS,
                       page_select, sizeof(page_select))) {
        return false;
    }
    if (stage_gap_us != 0U) sleep_us(stage_gap_us);

    if (!i2c_bus_write(bus, SIO_I2C_ADDRESS,
                       index_select, sizeof(index_select))) {
        return false;
    }
    if (stage_gap_us != 0U) sleep_us(stage_gap_us);

    bool ok = i2c_bus_write_read_byte(bus, SIO_I2C_ADDRESS,
                                      0x00U, value_out);
    if (ok && stage_gap_us != 0U) sleep_us(stage_gap_us);
    return ok;
}

bool read_sio_register(uint8_t page, uint8_t index, uint8_t *value_out) {
    return read_sio_register_on_bus(POST_BUS, page, index, value_out, 0U);
}

bool read_sio_monitor_value(uint8_t index,
                            bool word_value,
                            uint16_t *value_out) {
    if (value_out == nullptr) return false;

    const I2CBusConfig monitor_bus = {
        POST_BUS.inst,
        POST_BUS.sda_pin,
        POST_BUS.scl_pin,
        SIO_MONITOR_I2C_BAUD_HZ,
        SIO_MONITOR_I2C_TIMEOUT_MS,
    };

    const bool baud_change = monitor_bus.baud_hz != POST_BUS.baud_hz;
    if (baud_change) {
        i2c_set_baudrate(monitor_bus.inst, monitor_bus.baud_hz);
    }

    uint8_t high = 0U;
    bool ok = read_sio_register_on_bus(
        monitor_bus, SIO_MONITOR_PAGE, index, &high,
        SIO_MONITOR_STAGE_GAP_US);
    uint8_t low = 0U;
    if (ok && word_value) {
        ok = read_sio_register_on_bus(
            monitor_bus, SIO_MONITOR_PAGE, (uint8_t)(index + 1U), &low,
            SIO_MONITOR_STAGE_GAP_US);
    }

    if (baud_change) {
        i2c_set_baudrate(POST_BUS.inst, POST_BUS.baud_hz);
    }

    if (!ok) return false;
    *value_out = word_value
                     ? (uint16_t)(((uint16_t)high << 8U) | low)
                     : (uint16_t)high;
    return true;
}

bool read_post_code_p80(uint8_t *pointer_out, uint8_t *port80_out) {
    const uint8_t select_pointer_page[] = {0xFFU, 0x10U};
    const uint8_t select_port80_pointer[] = {0xFEU, 0xE3U};
    const uint8_t select_data_page[] = {0xFFU, 0x11U};

    if (!i2c_bus_write(POST_BUS, SIO_I2C_ADDRESS,
                       select_pointer_page, sizeof(select_pointer_page)) ||
        !i2c_bus_write(POST_BUS, SIO_I2C_ADDRESS,
                       select_port80_pointer,
                       sizeof(select_port80_pointer))) {
        return false;
    }

    uint8_t pointer = 0U;
    if (!i2c_bus_write_read_byte(POST_BUS, SIO_I2C_ADDRESS,
                                 0x00U, &pointer)) {
        return false;
    }

    const uint8_t select_port80_data[] = {
        0xFEU, (uint8_t)(pointer - 1U)};
    if (!i2c_bus_write(POST_BUS, SIO_I2C_ADDRESS,
                       select_data_page, sizeof(select_data_page)) ||
        !i2c_bus_write(POST_BUS, SIO_I2C_ADDRESS,
                       select_port80_data, sizeof(select_port80_data))) {
        return false;
    }

    uint8_t port80 = 0U;
    if (!i2c_bus_write_read_byte(POST_BUS, SIO_I2C_ADDRESS,
                                 0x00U, &port80)) {
        return false;
    }

    *pointer_out = pointer;
    *port80_out = port80;
    return true;
}

bool read_post_code(uint8_t *pointer_out,
                    uint8_t *port80_out,
                    uint8_t *port81_pointer_out,
                    uint8_t *port81_out) {
    uint8_t pointer = 0U;
    uint8_t port80 = 0U;

    if (!read_post_code_p80(&pointer, &port80)) {
        return false;
    }

#if POSTCODE_P81_DECODE_ENABLED == 0U
    /* The normal build is deliberately P80-only. Avoid spending another six
    * 100-kHz transactions on P81 during every 5-ms poll; that bus time is
    * then immediately available to voltage/temperature/tach reads. */
    *pointer_out = pointer;
    *port80_out = port80;
    *port81_pointer_out = 0U;
    *port81_out = 0U;
    return true;
#else
    uint8_t port81_pointer = 0U;
    uint8_t port81 = 0U;
    if (!read_sio_register(0x10U, 0xE8U, &port81_pointer) ||
        !read_sio_register(0x11U,
                           (uint8_t)((port81_pointer - 1U) | 0x80U),
                           &port81)) {
        return false;
    }

    *pointer_out = pointer;
    *port80_out = port80;
    *port81_pointer_out = port81_pointer;
    *port81_out = port81;
    return true;
#endif
}
