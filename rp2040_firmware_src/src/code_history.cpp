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

 #include "code_history.h"

#include <string.h>

void code_buffer_init(CodeBuffer *buffer) {
    memset(buffer->buf, 0, sizeof(buffer->buf));
    buffer->head = 0U;
    buffer->count = 0U;
    buffer->total_pushed = 0U;
    buffer->pending_abs = 0U;
}

uint32_t code_buffer_oldest_abs(const CodeBuffer *buffer) {
    return buffer->total_pushed - (uint32_t)buffer->count;
}

uint32_t code_buffer_newest_abs(const CodeBuffer *buffer) {
    return buffer->total_pushed - 1U;
}

bool code_buffer_get_abs(const CodeBuffer *buffer,
                         uint32_t absolute_index,
                         PostCode *code_out) {
    if (buffer->count == 0U) return false;
    uint32_t oldest = code_buffer_oldest_abs(buffer);
    if (absolute_index < oldest || absolute_index >= buffer->total_pushed) {
        return false;
    }
    size_t relative = (size_t)(absolute_index - oldest);
    size_t index = (buffer->head + relative) % CODE_BUFFER_CAPACITY;
    *code_out = buffer->buf[index];
    return true;
}

void code_buffer_push(CodeBuffer *buffer, PostCode code) {
    if (buffer->count < CODE_BUFFER_CAPACITY) {
        size_t write_index =
            (buffer->head + buffer->count) % CODE_BUFFER_CAPACITY;
        buffer->buf[write_index] = code;
        ++buffer->count;
    } else {
        buffer->buf[buffer->head] = code;
        buffer->head = (buffer->head + 1U) % CODE_BUFFER_CAPACITY;
    }
    ++buffer->total_pushed;

    uint32_t oldest = code_buffer_oldest_abs(buffer);
    if (buffer->pending_abs < oldest) buffer->pending_abs = oldest;
}

void code_buffer_clear_pending(CodeBuffer *buffer) {
    buffer->pending_abs = buffer->total_pushed;
}

bool code_buffer_pending_pop(CodeBuffer *buffer, PostCode *code_out) {
    if (buffer->count == 0U) return false;
    uint32_t oldest = code_buffer_oldest_abs(buffer);
    if (buffer->pending_abs < oldest) buffer->pending_abs = oldest;
    if (buffer->pending_abs >= buffer->total_pushed) return false;
    if (!code_buffer_get_abs(buffer, buffer->pending_abs, code_out)) {
        return false;
    }
    ++buffer->pending_abs;
    return true;
}
