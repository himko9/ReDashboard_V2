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

#include "board_config.h"
#include "hardware/sync.h"

static constexpr size_t BUTTON_SLOT_COUNT = 2U;
static ButtonState *button_slots[BUTTON_SLOT_COUNT] = {};

static bool is_digital_mode() {
    return BTN_INPUT_MODE == ButtonInputMode::DigitalActiveLow ||
           BTN_INPUT_MODE == ButtonInputMode::DigitalActiveHigh;
}

static bool digital_level_is_pressed(uint32_t events) {
    if (BTN_INPUT_MODE == ButtonInputMode::DigitalActiveLow) {
        return (events & GPIO_IRQ_EDGE_FALL) != 0U;
    }
    return (events & GPIO_IRQ_EDGE_RISE) != 0U;
}

static void button_gpio_irq(uint gpio, uint32_t events) {
    if (!digital_level_is_pressed(events)) return;

    for (ButtonState *button : button_slots) {
        if (button == nullptr || button->pin != gpio) continue;

        uint32_t now_us = time_us_32();
        if ((uint32_t)(now_us - button->last_irq_press_us) <
            BTN_DEBOUNCE_MS * 1000U) {
            return;
        }
        button->last_irq_press_us = now_us;
        if (button->pending_irq_presses != UINT8_MAX) {
            ++button->pending_irq_presses;
        }
        return;
    }
}

static void register_digital_button(ButtonState *button) {
    for (ButtonState *&slot : button_slots) {
        if (slot == nullptr) {
            slot = button;
            break;
        }
    }
    uint32_t active_edge =
        BTN_INPUT_MODE == ButtonInputMode::DigitalActiveLow
            ? GPIO_IRQ_EDGE_FALL
            : GPIO_IRQ_EDGE_RISE;
    gpio_set_irq_enabled_with_callback(
        button->pin, active_edge, true, button_gpio_irq);
}

static bool take_pending_irq_press(ButtonState *button) {
    uint32_t interrupt_state = save_and_disable_interrupts();
    bool pending = button->pending_irq_presses != 0U;
    if (pending) --button->pending_irq_presses;
    restore_interrupts(interrupt_state);
    return pending;
}

void button_init(ButtonState *button, uint pin) {
    button->pin = pin;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    if (BTN_INPUT_MODE == ButtonInputMode::DigitalActiveLow) {
        gpio_pull_up(pin);
    } else if (BTN_INPUT_MODE == ButtonInputMode::DigitalActiveHigh) {
        gpio_pull_down(pin);
    } else {
        gpio_disable_pulls(pin);
    }
    button->touch_raw_state = false;
    button->touch_baseline = 0U;
    button->touch_filtered = 0U;
    button->touch_delta_on = TOUCH_DELTA_ON;
    button->touch_delta_off = TOUCH_DELTA_OFF;
    bool pressed = false;
    if (BTN_INPUT_MODE == ButtonInputMode::DigitalActiveLow) {
        pressed = !gpio_get(pin);
    } else if (BTN_INPUT_MODE == ButtonInputMode::DigitalActiveHigh) {
        pressed = gpio_get(pin);
    }
    button->raw_pressed = pressed;
    button->stable_pressed = pressed;
    button->last_edge_time = get_absolute_time();
    button->press_start_time = button->last_edge_time;
    button->last_repeat_time = button->last_edge_time;
    button->pending_irq_presses = 0U;
    button->last_irq_press_us = time_us_32();

    if (is_digital_mode()) register_digital_button(button);
}

static uint16_t touch_measure_once(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_disable_pulls(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
    sleep_us(TOUCH_DISCHARGE_US);

    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    uint16_t count = 0U;
    while (!gpio_get(pin) && count < TOUCH_MAX_COUNT) ++count;
    gpio_disable_pulls(pin);
    return count;
}

static uint16_t touch_measure_filtered(ButtonState *button) {
    uint32_t sum = 0U;
    for (uint i = 0U; i < TOUCH_SAMPLES_PER_READ; ++i) {
        sum += touch_measure_once(button->pin);
    }
    uint16_t average = (uint16_t)(sum / TOUCH_SAMPLES_PER_READ);
    if (button->touch_filtered == 0U) {
        button->touch_filtered = average;
    } else {
        button->touch_filtered =
            (uint16_t)(((uint32_t)button->touch_filtered * 3U + average) / 4U);
    }
    if (button->touch_baseline == 0U) {
        button->touch_baseline = button->touch_filtered;
    }
    return button->touch_filtered;
}

static bool button_read_raw(ButtonState *button) {
    if (BTN_INPUT_MODE == ButtonInputMode::DigitalActiveLow) {
        return !gpio_get(button->pin);
    }
    if (BTN_INPUT_MODE == ButtonInputMode::DigitalActiveHigh) {
        return gpio_get(button->pin);
    }

    uint16_t sample = touch_measure_filtered(button);
    uint16_t on_threshold =
        (uint16_t)(button->touch_baseline + button->touch_delta_on);
    uint16_t off_threshold =
        (uint16_t)(button->touch_baseline + button->touch_delta_off);
    if (!button->touch_raw_state) {
        if (sample >= on_threshold) {
            button->touch_raw_state = true;
        } else {
            button->touch_baseline = (uint16_t)(
                ((uint32_t)button->touch_baseline *
                     ((1U << TOUCH_BASELINE_SHIFT) - 1U) +
                 sample) >>
                TOUCH_BASELINE_SHIFT);
        }
    } else if (sample <= off_threshold) {
        button->touch_raw_state = false;
    }
    return button->touch_raw_state;
}

bool button_poll_pressed(ButtonState *button) {
    bool raw = button_read_raw(button);
    absolute_time_t now = get_absolute_time();
    bool polled_press = false;
    if (raw != button->raw_pressed) {
        button->raw_pressed = raw;
        button->last_edge_time = now;
    }
    int64_t stable_us = absolute_time_diff_us(button->last_edge_time, now);
    if (stable_us >= (int64_t)BTN_DEBOUNCE_MS * 1000) {
        if (button->stable_pressed != button->raw_pressed) {
            button->stable_pressed = button->raw_pressed;
            if (button->stable_pressed) {
                button->press_start_time = now;
                button->last_repeat_time = now;
                polled_press = true;
            }
        }
    }
    if (is_digital_mode()) {
        return take_pending_irq_press(button);
    }
    return polled_press;
}

bool button_poll_repeat(ButtonState *button) {
    if (!button->stable_pressed) return false;
    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(button->press_start_time, now) <
        (int64_t)BTN_LONGPRESS_START_MS * 1000) {
        return false;
    }
    if (absolute_time_diff_us(button->last_repeat_time, now) >=
        (int64_t)BTN_LONGPRESS_REPEAT_MS * 1000) {
        button->last_repeat_time = now;
        return true;
    }
    return false;
}

const char *button_mode_name() {
    if (BTN_INPUT_MODE == ButtonInputMode::DigitalActiveLow) {
        return "digital-active-low";
    }
    if (BTN_INPUT_MODE == ButtonInputMode::DigitalActiveHigh) {
        return "digital-active-high";
    }
    return "gpio-cap-touch";
}
