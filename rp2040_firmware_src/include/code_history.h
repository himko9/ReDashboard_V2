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

#ifndef REDASHBOARD_RP2040_CODE_HISTORY_H
#define REDASHBOARD_RP2040_CODE_HISTORY_H

#include <stddef.h>
#include <stdint.h>

#include "app_types.h"

inline constexpr size_t CODE_BUFFER_CAPACITY = 256U;

struct CodeBuffer {
    PostCode buf[CODE_BUFFER_CAPACITY];
    size_t head;
    size_t count;
    uint32_t total_pushed;
    uint32_t pending_abs;
};

void code_buffer_init(CodeBuffer *buffer);
uint32_t code_buffer_oldest_abs(const CodeBuffer *buffer);
uint32_t code_buffer_newest_abs(const CodeBuffer *buffer);
bool code_buffer_get_abs(const CodeBuffer *buffer,
                         uint32_t absolute_index,
                         PostCode *code_out);
void code_buffer_push(CodeBuffer *buffer, PostCode code);
void code_buffer_clear_pending(CodeBuffer *buffer);
bool code_buffer_pending_pop(CodeBuffer *buffer, PostCode *code_out);

#endif
