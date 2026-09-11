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

#ifndef REDASHBOARD_P80_POST_CODES_H
#define REDASHBOARD_P80_POST_CODES_H

#include <stdint.h>

#define P80_POST_LABEL_MAX_CHARS 21U

#ifdef __cplusplus
extern "C" {
#endif

/* Returns an uppercase, display-sized AMI Aptio V checkpoint label. */
const char *p80_post_code_label(uint8_t code);

#ifdef __cplusplus
}
#endif

#endif
