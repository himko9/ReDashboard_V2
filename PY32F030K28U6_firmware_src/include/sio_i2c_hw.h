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

#ifndef EZDASH_SIO_I2C_HW_H
#define EZDASH_SIO_I2C_HW_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t pointer;
    uint8_t code;             /* Data at pointer - 1. */
    uint8_t port81_pointer;
    uint8_t port81_code;
} SioPostcodeSample;

typedef struct {
    uint8_t page;
    uint8_t index;
    uint8_t value;
} SioRegisterSample;

bool sio_i2c_init(void);
bool sio_i2c_start_poll(void);
bool sio_i2c_start_data_read(uint8_t page, uint8_t index);
void sio_i2c_service(uint32_t now_ms);
bool sio_i2c_take_sample(SioPostcodeSample *sample);
bool sio_i2c_take_register_sample(SioRegisterSample *sample);
bool sio_i2c_take_error(void);
bool sio_i2c_is_busy(void);
void sio_i2c_irq_handler(void);

#endif
