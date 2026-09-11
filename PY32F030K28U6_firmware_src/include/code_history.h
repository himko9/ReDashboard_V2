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

#ifndef EZDASH_CODE_HISTORY_H
#define EZDASH_CODE_HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CODE_HISTORY_CAPACITY 256U

typedef struct {
    uint32_t value;
    uint8_t width_bytes;
} PostCode;

typedef struct {
    uint32_t values[CODE_HISTORY_CAPACITY];
    uint8_t widths[CODE_HISTORY_CAPACITY];
    size_t head;
    size_t count;
    uint32_t total_pushed;
    uint32_t pending_abs;
} CodeHistory;

void code_history_init(CodeHistory *history);
void code_history_push(CodeHistory *history, PostCode code);
void code_history_clear_pending(CodeHistory *history);
bool code_history_pop_pending(CodeHistory *history, PostCode *code);
bool code_history_get(const CodeHistory *history,
                      uint32_t absolute_index,
                      PostCode *code);
uint32_t code_history_oldest(const CodeHistory *history);
uint32_t code_history_newest(const CodeHistory *history);

#endif
