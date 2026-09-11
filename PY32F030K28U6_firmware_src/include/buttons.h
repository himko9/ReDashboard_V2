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

#ifndef EZDASH_BUTTONS_H
#define EZDASH_BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool raw_pressed;
    bool stable_pressed;
    bool long_fired;
    uint32_t last_edge_ms;
    uint32_t press_start_ms;
    uint32_t last_repeat_ms;
} ButtonState;

typedef struct {
    bool pressed;
    bool released;
    bool repeated;
} ButtonEvents;

void button_init(ButtonState *button, bool pressed, uint32_t now_ms);
ButtonEvents button_update(ButtonState *button, bool pressed, uint32_t now_ms);

#endif
