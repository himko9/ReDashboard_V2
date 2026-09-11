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

#ifndef EZDASH_SSD1315_H
#define EZDASH_SSD1315_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DISPLAY_SOURCE_I2C = 0,
    DISPLAY_SOURCE_UART,
} DisplaySource;

typedef struct {
    DisplaySource source;
    char role_tag;
    bool show_top_bar;
    bool locked;
    bool show_scrollbar;
    const char *meaning;
    uint32_t oldest;
    uint32_t newest;
    uint32_t cursor;
} DisplayContext;

bool ssd1315_init(void);
bool ssd1315_is_online(void);
bool ssd1315_is_busy(void);
void ssd1315_render_code(uint32_t code,
                         uint8_t width_bytes,
                         const DisplayContext *context);
void ssd1315_render_post_pair(uint8_t port80,
                              uint8_t port81,
                              const DisplayContext *context);
void ssd1315_render_value(const char *value, const DisplayContext *context);
void ssd1315_service(void);
void ssd1315_cycle_shift(uint8_t pixels);
bool ssd1315_set_contrast(uint8_t contrast);
bool ssd1315_set_power(bool enabled);

#endif
