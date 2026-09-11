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

#include "board_io.h"

#include "board_config.h"

void board_output_write(uint pin, bool enabled) {
    bool high = MOSFET_ACTIVE_LOW ? !enabled : enabled;
    gpio_put(pin, high ? 1 : 0);
}

void board_outputs_init() {
    const uint pins[] = {MOSFET_PWR_PIN, MOSFET_RST_PIN, MOSFET_CLR_PIN};
    for (uint pin : pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        board_output_write(pin, false);
    }
}

const char *uart_instance_name(uart_inst_t *instance) {
    return instance == uart0 ? "uart0" : "uart1";
}
