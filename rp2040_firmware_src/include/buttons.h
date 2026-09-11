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

#ifndef REDASHBOARD_RP2040_BUTTONS_H
#define REDASHBOARD_RP2040_BUTTONS_H

#include <stdint.h>

#include "pico/stdlib.h"

struct ButtonState {
    uint pin;
    bool raw_pressed;
    bool stable_pressed;
    bool touch_raw_state;
    uint16_t touch_baseline;
    uint16_t touch_filtered;
    uint16_t touch_delta_on;
    uint16_t touch_delta_off;
    absolute_time_t last_edge_time;
    absolute_time_t press_start_time;
    absolute_time_t last_repeat_time;
    volatile uint8_t pending_irq_presses;
    uint32_t last_irq_press_us;
};

void button_init(ButtonState *button, uint pin);
bool button_poll_pressed(ButtonState *button);
bool button_poll_repeat(ButtonState *button);
const char *button_mode_name();

#endif
