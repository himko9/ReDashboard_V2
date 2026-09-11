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

void code_history_init(CodeHistory *history)
{
    size_t i;

    for (i = 0; i < CODE_HISTORY_CAPACITY; ++i) {
        history->values[i] = 0U;
        history->widths[i] = 1U;
    }
    history->head = 0U;
    history->count = 0U;
    history->total_pushed = 0U;
    history->pending_abs = 0U;
}

uint32_t code_history_oldest(const CodeHistory *history)
{
    return history->total_pushed - (uint32_t)history->count;
}

uint32_t code_history_newest(const CodeHistory *history)
{
    return history->total_pushed - 1U;
}

bool code_history_get(const CodeHistory *history,
                      uint32_t absolute_index,
                      PostCode *code)
{
    uint32_t oldest;
    size_t relative;
    size_t index;

    if ((history->count == 0U) || (code == NULL)) {
        return false;
    }

    oldest = code_history_oldest(history);
    if ((absolute_index < oldest) || (absolute_index >= history->total_pushed)) {
        return false;
    }

    relative = (size_t)(absolute_index - oldest);
    index = (history->head + relative) % CODE_HISTORY_CAPACITY;
    code->value = history->values[index];
    code->width_bytes = history->widths[index];
    return true;
}

void code_history_push(CodeHistory *history, PostCode code)
{
    size_t index;

    if (history->count < CODE_HISTORY_CAPACITY) {
        index = (history->head + history->count) % CODE_HISTORY_CAPACITY;
        ++history->count;
    } else {
        index = history->head;
        history->head = (history->head + 1U) % CODE_HISTORY_CAPACITY;
    }
    history->values[index] = code.value;
    history->widths[index] = code.width_bytes;

    ++history->total_pushed;
    if (history->pending_abs < code_history_oldest(history)) {
        history->pending_abs = code_history_oldest(history);
    }
}

void code_history_clear_pending(CodeHistory *history)
{
    history->pending_abs = history->total_pushed;
}

bool code_history_pop_pending(CodeHistory *history, PostCode *code)
{
    uint32_t oldest;

    if ((history->count == 0U) || (code == NULL)) {
        return false;
    }

    oldest = code_history_oldest(history);
    if (history->pending_abs < oldest) {
        history->pending_abs = oldest;
    }
    if (history->pending_abs >= history->total_pushed) {
        return false;
    }
    if (!code_history_get(history, history->pending_abs, code)) {
        return false;
    }

    ++history->pending_abs;
    return true;
}
