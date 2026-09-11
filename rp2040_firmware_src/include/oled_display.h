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

#ifndef REDASHBOARD_RP2040_OLED_DISPLAY_H
#define REDASHBOARD_RP2040_OLED_DISPLAY_H

#include <stdint.h>

#include "app_types.h"
#include "pico/stdlib.h"

struct OledUiContext {
    const char *mode_tag;
    char nav_tag;
    bool top_bar_visible;
    bool mode_locked;
    bool sio_comm_error;
    bool port81_visible;
    uint8_t port81_code;
};

/* Live UI context updated by the application before rendering. */
extern OledUiContext oled_ui;

bool oled_init();
bool oled_set_contrast(uint8_t value);
bool oled_set_power(bool on);
void oled_set_pixel_shift(int x, int y);
bool oled_show_baud_value(uint32_t baud);
bool oled_show_sensor_value(const char *value,
                            const char *label,
                            char role_tag);
bool oled_render_code(bool oled_ok,
                      bool *panel_on,
                      absolute_time_t *last_activity,
                      PostCode code,
                      bool show_scroll,
                      uint32_t oldest,
                      uint32_t newest,
                      uint32_t cursor,
                      bool mark_activity = true);

#endif
