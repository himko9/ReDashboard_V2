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

#include "buttons.h"

#define BUTTON_DEBOUNCE_MS    35U
#define BUTTON_REPEAT_START_MS 450U
#define BUTTON_REPEAT_MS      140U

void button_init(ButtonState *button, bool pressed, uint32_t now_ms)
{
    button->raw_pressed = pressed;
    button->stable_pressed = pressed;
    button->long_fired = false;
    button->last_edge_ms = now_ms;
    button->press_start_ms = now_ms;
    button->last_repeat_ms = now_ms;
}

ButtonEvents button_update(ButtonState *button, bool pressed, uint32_t now_ms)
{
    ButtonEvents events = {false, false, false};

    if (pressed != button->raw_pressed) {
        button->raw_pressed = pressed;
        button->last_edge_ms = now_ms;
    }

    if (((uint32_t)(now_ms - button->last_edge_ms) >= BUTTON_DEBOUNCE_MS) &&
        (button->stable_pressed != button->raw_pressed)) {
        button->stable_pressed = button->raw_pressed;
        if (button->stable_pressed) {
            button->press_start_ms = now_ms;
            button->last_repeat_ms = now_ms;
            button->long_fired = false;
            events.pressed = true;
        } else {
            button->long_fired = false;
            events.released = true;
        }
    }

    if (button->stable_pressed) {
        uint32_t held_ms = now_ms - button->press_start_ms;
        if (!button->long_fired && (held_ms >= BUTTON_REPEAT_START_MS)) {
            button->long_fired = true;
            button->last_repeat_ms = now_ms;
            events.repeated = true;
        } else if (button->long_fired &&
                   ((uint32_t)(now_ms - button->last_repeat_ms) >= BUTTON_REPEAT_MS)) {
            button->last_repeat_ms = now_ms;
            events.repeated = true;
        }
    }

    return events;
}
