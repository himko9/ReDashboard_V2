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

 #ifndef EZDASH_POST_VALUE_FILTER_H
#define EZDASH_POST_VALUE_FILTER_H

#include <stdbool.h>
#include <stdint.h>

/* The dashboard intentionally displays only OUT8 and OUT16 records. */
static inline bool post_value_is_visible(uint32_t value, uint8_t width_bytes)
{
    (void)value;
    return width_bytes != 4U;
}

#endif
