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

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "board.h"
#include "p80_post_codes.h"
#include "buttons.h"
#include "code_history.h"
#include "display_config.h"
#include "sio_adc_config.h"
#include "sio_fan_config.h"
#include "sio_i2c_hw.h"
#include "ssd1315.h"
#include "uart_fast_rx.h"
#include "uart_parser.h"
#include "uart_rx.h"

typedef enum {
    SOURCE_I2C = 0,
    SOURCE_UART,
} DataSource;

typedef enum {
    UART_INPUT_NONE = 0,
    UART_INPUT_115200,
    UART_INPUT_1500000,
} UartInput;

typedef enum {
    BUTTON_ROLE_BROWSE = 0,
    BUTTON_ROLE_GPIO,
} ButtonRole;

typedef enum {
    DISPLAY_PAGE_POST = 0,
    DISPLAY_PAGE_VOLTAGE,
    DISPLAY_PAGE_TEMPERATURE,
    DISPLAY_PAGE_FAN,
} DisplayPage;

typedef enum {
    SENSOR_READ_NONE = 0,
    SENSOR_READ_BYTE,
    SENSOR_READ_WORD_HIGH,
    SENSOR_READ_WORD_LOW,
} SensorReadPhase;

typedef int32_t (*SensorCalculate)(uint16_t raw);

typedef struct {
    uint8_t enabled;
    uint8_t type;
    uint8_t index;
    const char *label;
    SensorCalculate calculate;
} SensorDefinition;

#define DEFINE_SENSOR_CALCULATOR(id, enabled, type, index, label, calculate) \
    static int32_t sensor_calculate_##id(uint16_t raw)                      \
    {                                                                       \
        return (int32_t)(calculate(raw));                                    \
    }
SENSOR_TABLE(DEFINE_SENSOR_CALCULATOR)
SIO_TACH_TABLE(DEFINE_SENSOR_CALCULATOR)
#undef DEFINE_SENSOR_CALCULATOR

#define DEFINE_SENSOR_ENTRY(id, enabled, type, index, label, calculate) \
    {(enabled), (type), (index), (label), sensor_calculate_##id},
static const SensorDefinition sensors[] = {
    SENSOR_TABLE(DEFINE_SENSOR_ENTRY)
    SIO_TACH_TABLE(DEFINE_SENSOR_ENTRY)
};
#undef DEFINE_SENSOR_ENTRY

#define VALIDATE_SENSOR_ENTRY(id, enabled, type, index, label, calculate)    \
    _Static_assert(((enabled) == 0U) || ((enabled) == 1U),                  \
                   #id " ENABLED must be 0 or 1");                          \
    _Static_assert(((type) == SENSOR_TYPE_VIN) ||                           \
                       ((type) == SENSOR_TYPE_THR) ||                       \
                       ((type) == SENSOR_TYPE_TACH),                        \
                   #id " TYPE must be VIN, THR, or TACH");                 \
    _Static_assert((index) < 0xFFU, #id " INDEX must be below 0xFF");
SENSOR_TABLE(VALIDATE_SENSOR_ENTRY)
SIO_TACH_TABLE(VALIDATE_SENSOR_ENTRY)
#undef VALIDATE_SENSOR_ENTRY

#define SENSOR_COUNT ((uint8_t)(sizeof(sensors) / sizeof(sensors[0])))
_Static_assert((sizeof(sensors) / sizeof(sensors[0])) <= 32U,
               "SENSOR_TABLE supports at most 32 entries");

typedef struct {
    CodeHistory history;
    ButtonState previous_button;
    ButtonState next_button;

    DataSource source;
    UartInput uart_input;
    uint32_t fast_uart_record_count;
    uint32_t fast_uart_error_count;
    ButtonRole button_role;
    bool mode_locked;

    bool browse_active;
    uint32_t browse_cursor;
    uint32_t browse_enter_total;
    uint32_t browse_deadline_ms;

    DisplayPage display_page;
    uint8_t up_click_count;
    uint8_t down_click_count;
    uint32_t page_click_deadline_ms;
    uint8_t sensor_index;
    uint32_t sensor_page_deadline_ms;
    bool sensor_refresh_pending;
    SensorReadPhase sensor_read_phase;
    bool sensor_read_inflight;
    uint8_t sensor_request_slot;
    uint8_t sensor_word_high;
    uint32_t sensor_retry_ms;
    bool post_override_active;
    uint32_t post_override_deadline_ms;
    int32_t sensor_values[SENSOR_COUNT];
    uint32_t sensor_valid_mask;

    PostCode last_enqueued_code;
    bool last_enqueued_valid;
    PostCode current_code;
    uint8_t port81_code;
    uint8_t port81_zero_polls;
    bool port81_visible;
    bool postcode_error_active;
    bool sio_error_candidate;
    uint32_t sio_error_since_ms;

    bool display_panel_on;
    bool display_dimmed;
    bool display_top_bar_visible;
    bool display_frame_flushing;
    uint32_t display_last_activity_ms;
    uint32_t next_display_ms;
    uint32_t next_shift_ms;
    uint32_t next_display_retry_ms;
    uint32_t next_poll_ms;

    bool both_pressed_latch;
    bool combo_short_armed;
    bool combo_role_fired;
    bool combo_lock_fired;
    bool combo_clear_fired;
    uint32_t combo_start_ms;

    bool previous_output_fired;
    bool next_output_fired;
    bool singles_suppressed_until_release;
    bool power_output_on;
    bool reset_output_on;
    bool clear_output_on;
} Application;

static Application app;

static void leave_browse_mode(uint32_t now_ms);
static void display_redraw(bool mark_activity, uint32_t now_ms);

static bool time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static DisplayContext display_context(bool show_scrollbar)
{
    DisplayContext context;

    context.source = app.source == SOURCE_UART ? DISPLAY_SOURCE_UART : DISPLAY_SOURCE_I2C;
    context.role_tag = app.button_role == BUTTON_ROLE_BROWSE ? 'S' : 'G';
    context.show_top_bar = app.display_top_bar_visible;
    context.locked = app.mode_locked;
    context.show_scrollbar = show_scrollbar;
    context.meaning = NULL;
    context.oldest = app.history.count > 0U ? code_history_oldest(&app.history) : 0U;
    context.newest = app.history.count > 0U ? code_history_newest(&app.history) : 0U;
    context.cursor = app.browse_cursor;
    return context;
}

static void display_wake(uint32_t now_ms)
{
    bool restore_full_contrast;

    if (!ssd1315_is_online()) {
        return;
    }
    app.display_top_bar_visible = true;
    restore_full_contrast = !app.display_panel_on || app.display_dimmed;
    if (!app.display_panel_on) {
        if (!ssd1315_set_power(true)) {
            return;
        }
        app.display_panel_on = true;
    }
    if (restore_full_contrast) {
        if (!ssd1315_set_contrast(DISPLAY_CONTRAST_FULL)) {
            return;
        }
        app.display_dimmed = false;
    }
    app.display_last_activity_ms = now_ms;
}

static void display_code(PostCode code,
                         bool show_scrollbar,
                         bool mark_activity,
                         uint32_t now_ms)
{
    DisplayContext context;

    app.current_code = code;
    if (!ssd1315_is_online()) {
        return;
    }
    if (mark_activity) {
        app.display_top_bar_visible = true;
    }
    if (mark_activity && (!app.display_panel_on || app.display_dimmed)) {
        display_wake(now_ms);
    }
    if (!app.display_panel_on) {
        return;
    }
    context = display_context(show_scrollbar);
    if (app.postcode_error_active && (code.value == 0U) &&
        (code.width_bytes == 1U)) {
        context.meaning = "SIO COMM ERROR";
    } else if (code.width_bytes == 1U) {
        context.meaning = p80_post_code_label((uint8_t)code.value);
    } else {
        context.meaning = NULL;
    }
    if ((app.source == SOURCE_I2C) && app.port81_visible &&
        !show_scrollbar && !app.postcode_error_active &&
        (code.width_bytes == 1U)) {
        ssd1315_render_post_pair((uint8_t)code.value,
                                 app.port81_code,
                                 &context);
    } else {
        ssd1315_render_code(code.value, code.width_bytes, &context);
    }
    app.display_frame_flushing = ssd1315_is_busy();
    if (mark_activity) {
        app.display_last_activity_ms = now_ms;
    }
}

static void format_voltage(int32_t calculated_millivolts, char value[9])
{
    uint32_t millivolts = calculated_millivolts > 0
                              ? (uint32_t)calculated_millivolts
                              : 0U;
    uint32_t rounded_units;
    uint32_t whole;
#if SENSOR_VOLTAGE_DECIMAL_DIGITS > 0U
    uint32_t fraction;
#endif
    uint8_t offset = 0U;

#if SENSOR_VOLTAGE_DECIMAL_DIGITS == 0U
    rounded_units = ((uint32_t)millivolts + 500U) / 1000U;
    whole = rounded_units;
#elif SENSOR_VOLTAGE_DECIMAL_DIGITS == 1U
    rounded_units = ((uint32_t)millivolts + 50U) / 100U;
    whole = rounded_units / 10U;
    fraction = rounded_units % 10U;
#elif SENSOR_VOLTAGE_DECIMAL_DIGITS == 2U
    rounded_units = ((uint32_t)millivolts + 5U) / 10U;
    whole = rounded_units / 100U;
    fraction = rounded_units % 100U;
#else
    rounded_units = millivolts;
    whole = rounded_units / 1000U;
    fraction = rounded_units % 1000U;
#endif

    if (whole >= 100U) {
        value[offset++] = (char)('0' + (whole / 100U) % 10U);
    }
    if (whole >= 10U) {
        value[offset++] = (char)('0' + (whole / 10U) % 10U);
    }
    value[offset++] = (char)('0' + whole % 10U);
#if SENSOR_VOLTAGE_DECIMAL_DIGITS > 0U
    value[offset++] = '.';
#if SENSOR_VOLTAGE_DECIMAL_DIGITS == 3U
    value[offset++] = (char)('0' + fraction / 100U);
#endif
#if SENSOR_VOLTAGE_DECIMAL_DIGITS >= 2U
    value[offset++] = (char)('0' + (fraction / 10U) % 10U);
#endif
    value[offset++] = (char)('0' + fraction % 10U);
#endif
    value[offset++] = 'V';
    value[offset] = '\0';
}

static void format_temperature(int32_t tenths_celsius, char value[9])
{
    int32_t tenths;
    uint32_t magnitude;
    uint32_t whole;
    uint32_t fraction;
    uint8_t offset = 0U;

#if SENSOR_TEMP_UNIT == 'F'
    {
        int32_t fahrenheit_delta = tenths_celsius * 9;

        tenths = (fahrenheit_delta >= 0
                      ? (fahrenheit_delta + 2) / 5
                      : (fahrenheit_delta - 2) / 5) +
                 320;
    }
#else
    tenths = tenths_celsius;
#endif
    magnitude = tenths < 0 ? (uint32_t)(-tenths) : (uint32_t)tenths;
    whole = magnitude / 10U;
    fraction = magnitude % 10U;

    if (tenths < 0) {
        value[offset++] = '-';
    }
    if (whole >= 100U) {
        value[offset++] = (char)('0' + (whole / 100U) % 10U);
    }
    if (whole >= 10U) {
        value[offset++] = (char)('0' + (whole / 10U) % 10U);
    }
    value[offset++] = (char)('0' + whole % 10U);
    value[offset++] = '.';
    value[offset++] = (char)('0' + fraction);
    value[offset++] = '^';
    value[offset++] = SENSOR_TEMP_UNIT;
    value[offset] = '\0';
}

static void format_rpm(int32_t calculated_rpm, char value[9])
{
    uint32_t rpm = calculated_rpm > 0 ? (uint32_t)calculated_rpm : 0U;
    uint32_t divisor = 10000U;
    uint8_t offset = 0U;
    bool started = false;

    if (rpm > UINT16_MAX) {
        rpm = UINT16_MAX;
    }
    while (divisor != 0U) {
        uint8_t digit = (uint8_t)((rpm / divisor) % 10U);

        if ((digit != 0U) || started || (divisor == 1U)) {
            value[offset++] = (char)('0' + digit);
            started = true;
        }
        divisor /= 10U;
    }
    value[offset++] = 'R';
    value[offset++] = 'P';
    value[offset++] = 'M';
    value[offset] = '\0';
}

static void display_sensor_page(bool mark_activity, uint32_t now_ms)
{
    DisplayContext context;
    const SensorDefinition *sensor;
    char value[9] = "WAIT";

    if ((app.display_page == DISPLAY_PAGE_POST) || !ssd1315_is_online()) {
        return;
    }
    if (mark_activity) {
        app.display_top_bar_visible = true;
    }
    if (mark_activity && (!app.display_panel_on || app.display_dimmed)) {
        display_wake(now_ms);
    }
    if (!app.display_panel_on) {
        return;
    }

    context = display_context(false);
    if (app.postcode_error_active) {
        context.meaning = "SIO COMM ERROR";
        context.role_tag = '!';
        ssd1315_render_value("ERROR", &context);
        app.display_frame_flushing = ssd1315_is_busy();
        if (mark_activity) {
            app.display_last_activity_ms = now_ms;
        }
        return;
    }

    sensor = &sensors[app.sensor_index];
    context.meaning = sensor->label;
    if (sensor->type == SENSOR_TYPE_VIN) {
        context.role_tag = 'V';
        if ((app.sensor_valid_mask &
             (UINT32_C(1) << app.sensor_index)) != 0U) {
            format_voltage(app.sensor_values[app.sensor_index], value);
        }
    } else if (sensor->type == SENSOR_TYPE_THR) {
        context.role_tag = 'T';
        if ((app.sensor_valid_mask &
             (UINT32_C(1) << app.sensor_index)) != 0U) {
            format_temperature(app.sensor_values[app.sensor_index], value);
        }
    } else {
        context.role_tag = 'F';
        if ((app.sensor_valid_mask &
             (UINT32_C(1) << app.sensor_index)) != 0U) {
            format_rpm(app.sensor_values[app.sensor_index], value);
        }
    }
    ssd1315_render_value(value, &context);
    app.display_frame_flushing = ssd1315_is_busy();
    if (mark_activity) {
        app.display_last_activity_ms = now_ms;
    }
}

static void reset_page_click_sequence(void)
{
    app.up_click_count = 0U;
    app.down_click_count = 0U;
    app.page_click_deadline_ms = 0U;
}

static bool sensor_matches_page(uint8_t sensor_index, DisplayPage page)
{
    uint8_t required_type;

    if ((sensor_index >= SENSOR_COUNT) ||
        ((page != DISPLAY_PAGE_VOLTAGE) &&
         (page != DISPLAY_PAGE_TEMPERATURE) &&
         (page != DISPLAY_PAGE_FAN))) {
        return false;
    }
    if (page == DISPLAY_PAGE_VOLTAGE) {
#if DISPLAY_VOLTAGE_PAGE_ENABLED == 0U
        return false;
#endif
        required_type = SENSOR_TYPE_VIN;
    } else if (page == DISPLAY_PAGE_TEMPERATURE) {
#if DISPLAY_TEMPERATURE_PAGE_ENABLED == 0U
        return false;
#endif
        required_type = SENSOR_TYPE_THR;
    } else {
#if DISPLAY_FAN_PAGE_ENABLED == 0U
        return false;
#endif
        required_type = SENSOR_TYPE_TACH;
    }
    return (sensors[sensor_index].enabled != 0U) &&
           (sensors[sensor_index].type == required_type);
}

static bool first_sensor_for_page(DisplayPage page, uint8_t *sensor_index)
{
    uint8_t index;

    for (index = 0U; index < SENSOR_COUNT; ++index) {
        if (sensor_matches_page(index, page)) {
            *sensor_index = index;
            return true;
        }
    }
    return false;
}

static uint8_t next_sensor_for_page(DisplayPage page, uint8_t current)
{
    uint8_t offset;

    for (offset = 1U; offset <= SENSOR_COUNT; ++offset) {
        uint8_t candidate = (uint8_t)((current + offset) % SENSOR_COUNT);

        if (sensor_matches_page(candidate, page)) {
            return candidate;
        }
    }
    return current;
}

static void enter_sensor_page(DisplayPage page,
                              uint32_t now_ms,
                              bool mark_activity)
{
    uint8_t first_sensor;

    if ((app.source != SOURCE_I2C) || (app.button_role != BUTTON_ROLE_BROWSE) ||
        ((page != DISPLAY_PAGE_VOLTAGE) &&
         (page != DISPLAY_PAGE_TEMPERATURE) &&
         (page != DISPLAY_PAGE_FAN)) ||
        !first_sensor_for_page(page, &first_sensor)) {
        return;
    }

    if (app.browse_active) {
        leave_browse_mode(now_ms);
    }
    app.display_page = page;
    app.sensor_index = first_sensor;
    app.sensor_page_deadline_ms = now_ms + SENSOR_PAGE_INTERVAL_MS;
    app.sensor_refresh_pending = true;
    app.post_override_active = false;
    reset_page_click_sequence();
    display_sensor_page(mark_activity, now_ms);
}

static void exit_sensor_page(uint32_t now_ms)
{
    if (app.display_page == DISPLAY_PAGE_POST) {
        return;
    }

    app.display_page = DISPLAY_PAGE_POST;
    app.sensor_refresh_pending = false;
    app.post_override_active = false;
    if (!app.sensor_read_inflight) {
        app.sensor_read_phase = SENSOR_READ_NONE;
    }
    reset_page_click_sequence();
    display_redraw(true, now_ms);
}

static void display_redraw(bool mark_activity, uint32_t now_ms)
{
    PostCode code;

    if (app.display_page != DISPLAY_PAGE_POST) {
        display_sensor_page(mark_activity, now_ms);
    } else if (app.browse_active && (app.history.count > 0U) &&
        code_history_get(&app.history, app.browse_cursor, &code)) {
        display_code(code, true, mark_activity, now_ms);
    } else {
        display_code(app.current_code, false, mark_activity, now_ms);
    }
}

static void reset_code_pipeline(uint32_t now_ms)
{
    code_history_init(&app.history);
    app.last_enqueued_code.value = 0U;
    app.last_enqueued_code.width_bytes = 1U;
    app.last_enqueued_valid = false;
    app.browse_active = false;
    app.display_page = DISPLAY_PAGE_POST;
    app.sensor_refresh_pending = false;
    app.post_override_active = false;
    if (!app.sensor_read_inflight) {
        app.sensor_read_phase = SENSOR_READ_NONE;
    }
    reset_page_click_sequence();
    app.port81_code = 0U;
    app.port81_zero_polls = 0U;
    app.port81_visible = false;
    app.postcode_error_active = false;
    app.sio_error_candidate = false;
    app.next_display_ms = now_ms + DISPLAY_CODE_HOLD_MS;
    app.current_code.value = 0U;
    app.current_code.width_bytes = 1U;
    display_code(app.current_code, false, true, now_ms);
}

static void start_post_override(uint32_t now_ms)
{
    if (app.display_page != DISPLAY_PAGE_POST) {
        exit_sensor_page(now_ms);
    } else {
        bool panel_was_off = !app.display_panel_on;

        display_wake(now_ms);
        if (panel_was_off && app.display_panel_on) {
            display_redraw(false, now_ms);
        }
    }

    if ((app.source == SOURCE_I2C) &&
        (DEFAULT_BOOT_SCREEN != BOOT_SCREEN_POST) &&
        (DISPLAY_POST_RETURN_DELAY_MS != 0U)) {
        app.post_override_active = true;
        app.post_override_deadline_ms =
            now_ms + DISPLAY_POST_RETURN_DELAY_MS;
    } else {
        app.post_override_active = false;
    }
}

static void switch_source(DataSource source, uint32_t now_ms)
{
    if (app.source == source) {
        return;
    }
    app.source = source;
    app.next_poll_ms = now_ms;
    reset_code_pipeline(now_ms);
}

static void ingest_code(uint32_t value,
                        uint8_t width_bytes,
                        bool accept_zero,
                        uint32_t now_ms)
{
    PostCode code;

    if ((width_bytes != 1U) && (width_bytes != 2U) &&
        (width_bytes != 4U)) {
        return;
    }
    if (width_bytes == 1U) {
        value &= 0xFFU;
    } else if (width_bytes == 2U) {
        value &= 0xFFFFU;
    }
    if (!accept_zero && (value == 0U)) {
        return;
    }
    code.value = value;
    code.width_bytes = width_bytes;
    if (app.last_enqueued_valid &&
        (app.last_enqueued_code.value == code.value) &&
        (app.last_enqueued_code.width_bytes == code.width_bytes)) {
        return;
    }
    code_history_push(&app.history, code);
    app.last_enqueued_code = code;
    app.last_enqueued_valid = true;
    start_post_override(now_ms);
}

static void ingest_port81(uint8_t code, uint32_t now_ms)
{
#if POSTCODE_P81_DECODE_ENABLED == 0U
    (void)code;
    (void)now_ms;
    return;
#else
    bool redraw = false;

    if (code != 0U) {
        redraw = !app.port81_visible || (app.port81_code != code);
        app.port81_code = code;
        app.port81_zero_polls = 0U;
        app.port81_visible = true;
    } else if (app.port81_visible) {
        if (app.port81_zero_polls < PORT81_ZERO_HIDE_POLLS) {
            ++app.port81_zero_polls;
        }
        if (app.port81_zero_polls >= PORT81_ZERO_HIDE_POLLS) {
            app.port81_zero_polls = 0U;
            app.port81_visible = false;
            redraw = true;
        }
    }

    if (redraw) {
        start_post_override(now_ms);
        if (!app.browse_active &&
            (app.display_page == DISPLAY_PAGE_POST)) {
            display_redraw(true, now_ms);
        }
    }
#endif
}

static void service_uart(uint32_t now_ms)
{
    uint8_t byte;
    uint64_t raw;
    UartFastRecord record;
    uint32_t fast_record_count;
    uint32_t fast_error_count;

    fast_record_count = uart_fast_rx_record_count();
    if (fast_record_count != app.fast_uart_record_count) {
        app.fast_uart_record_count = fast_record_count;
    }
    fast_error_count = uart_fast_rx_error_count();
    if (fast_error_count != app.fast_uart_error_count) {
        app.fast_uart_error_count = fast_error_count;
    }

    /* Service the custom variable-width stream first. Its timer/DMA receiver
       returns one reconstructed port/width/payload word at a time. */
    while (uart_fast_rx_pop(&raw)) {
        if (!uart_fast_record_decode(raw, &record)) {
            continue;
        }
        if (app.uart_input != UART_INPUT_1500000) {
            app.uart_input = UART_INPUT_1500000;
            if (app.source == SOURCE_UART) {
                reset_code_pipeline(now_ms);
            }
        }
        if (app.source != SOURCE_UART) {
            switch_source(SOURCE_UART, now_ms);
        }
        /* UART mode remains Port-80-only, but Port 80 writes retain their
           wire width. OUT16 is displayed in ascending port order: P80, P81. */
        if (record.port == 0x0080U) {
            uint8_t width_bytes = (uint8_t)(1U << record.width);
            uint32_t display_value = record.payload;

            if (width_bytes == 2U) {
                uint8_t port80 = (uint8_t)record.payload;
#if POSTCODE_P81_DECODE_ENABLED != 0U
                uint8_t port81 = (uint8_t)(record.payload >> 8U);

                if ((port81 == 0x00U) || (port81 == 0xFFU)) {
                    display_value = port80;
                    width_bytes = 1U;
                } else {
                    display_value = ((uint32_t)port80 << 8U) | port81;
                }
#else
                display_value = port80;
                width_bytes = 1U;
#endif
            }
            ingest_code(display_value, width_bytes, true, now_ms);
        }
    }

    /* PB2 USART1 and PA1 fast RX are independent inputs. Keep USART1 armed
       even when the custom 1.5 Mbaud decoder was the last active source. */
    uart_rx_service(now_ms, true);

    while (uart_rx_pop(&byte)) {
        uint8_t code = 0U;
        if (uart_postcode_decode(byte, &code)) {
            if (app.uart_input != UART_INPUT_115200) {
                app.uart_input = UART_INPUT_115200;
                if (app.source == SOURCE_UART) {
                    reset_code_pipeline(now_ms);
                }
            }
            if (app.source != SOURCE_UART) {
                switch_source(SOURCE_UART, now_ms);
            }
            ingest_code(code, 1U, true, now_ms);
        }
    }
}

static void enter_postcode_error(uint32_t now_ms)
{
    bool port81_was_visible = app.port81_visible;

    if (!app.postcode_error_active) {
        if (app.last_enqueued_valid && (app.last_enqueued_code.value > 0U)) {
            PostCode error_code = {0U, 1U};

            code_history_push(&app.history, error_code);
            app.last_enqueued_code = error_code;
            app.last_enqueued_valid = true;
        }
        code_history_clear_pending(&app.history);
        app.next_display_ms = now_ms + DISPLAY_CODE_HOLD_MS;
        app.sensor_valid_mask = 0U;
    }
    app.postcode_error_active = true;
    app.port81_visible = false;
    app.port81_zero_polls = 0U;
    if (!app.browse_active) {
        if (app.display_page != DISPLAY_PAGE_POST) {
            display_sensor_page(true, now_ms);
        } else if ((app.current_code.value != 0U) || port81_was_visible) {
            PostCode error_code = {0U, 1U};

            display_code(error_code, false, true, now_ms);
        }
    }
}

static void service_sensor_carousel(uint32_t now_ms)
{
    if (app.display_page == DISPLAY_PAGE_POST) {
        return;
    }
    if ((app.source != SOURCE_I2C) ||
        (app.button_role != BUTTON_ROLE_BROWSE)) {
        exit_sensor_page(now_ms);
        return;
    }
    if (app.postcode_error_active) {
        return;
    }
    if (!time_reached(now_ms, app.sensor_page_deadline_ms)) {
        return;
    }

    app.sensor_index = next_sensor_for_page(app.display_page,
                                            app.sensor_index);
    app.sensor_page_deadline_ms = now_ms + SENSOR_PAGE_INTERVAL_MS;
    app.sensor_refresh_pending = true;
    display_sensor_page(false, now_ms);
}

static uint8_t sensor_request_index(void)
{
    if (app.sensor_read_phase == SENSOR_READ_WORD_LOW) {
        return (uint8_t)(sensors[app.sensor_request_slot].index + 1U);
    }
    return sensors[app.sensor_request_slot].index;
}

static bool sensor_change_exceeds_wake_threshold(int32_t previous,
                                                  int32_t current)
{
#if SENSOR_WAKE_CHANGE_PERCENT == 0U
    (void)previous;
    (void)current;
    return false;
#else
    int64_t delta = (int64_t)current - (int64_t)previous;
    int64_t baseline = previous;

    if (delta < 0) {
        delta = -delta;
    }
    if (baseline < 0) {
        baseline = -baseline;
    }
    if (baseline == 0) {
        return delta != 0;
    }
    return delta * 100 >
           baseline * (int64_t)SENSOR_WAKE_CHANGE_PERCENT;
#endif
}

static void accept_calculated_sensor_value(uint8_t slot,
                                           int32_t calculated,
                                           uint32_t now_ms)
{
    uint32_t sensor_bit = UINT32_C(1) << slot;
    bool significant_change =
        ((app.sensor_valid_mask & sensor_bit) != 0U) &&
        sensor_change_exceeds_wake_threshold(app.sensor_values[slot],
                                             calculated);

    app.sensor_values[slot] = calculated;
    app.sensor_valid_mask |= sensor_bit;
    if (sensor_matches_page(slot, app.display_page) &&
        (app.sensor_index == slot)) {
        display_sensor_page(significant_change, now_ms);
    }
}

static void prepare_sensor_read(uint32_t now_ms)
{
    if ((app.display_page == DISPLAY_PAGE_POST) ||
        app.sio_error_candidate ||
        !app.sensor_refresh_pending ||
        (app.sensor_read_phase != SENSOR_READ_NONE) ||
        !time_reached(now_ms, app.sensor_retry_ms)) {
        return;
    }

    app.sensor_request_slot = app.sensor_index;
    app.sensor_read_phase =
        sensors[app.sensor_request_slot].type == SENSOR_TYPE_VIN
                                ? SENSOR_READ_BYTE
                                : SENSOR_READ_WORD_HIGH;
    app.sensor_refresh_pending = false;
    app.sensor_retry_ms = now_ms;
}

static void accept_sensor_sample(const SioRegisterSample *sample,
                                 uint32_t now_ms)
{
    uint8_t expected_index;
    uint8_t slot;
    uint16_t raw_value;

    if (!app.sensor_read_inflight ||
        (app.sensor_read_phase == SENSOR_READ_NONE)) {
        return;
    }

    expected_index = sensor_request_index();
    slot = app.sensor_request_slot;
    app.sensor_read_inflight = false;
    if ((sample->page != SIO_MONITOR_PAGE) ||
        (sample->index != expected_index)) {
        app.sensor_read_phase = SENSOR_READ_NONE;
        return;
    }

    if (app.sensor_read_phase == SENSOR_READ_BYTE) {
        app.sensor_read_phase = SENSOR_READ_NONE;
        accept_calculated_sensor_value(
            slot, sensors[slot].calculate(sample->value), now_ms);
    } else if (app.sensor_read_phase == SENSOR_READ_WORD_HIGH) {
        app.sensor_word_high = sample->value;
        app.sensor_read_phase = SENSOR_READ_WORD_LOW;
        app.sensor_retry_ms = now_ms;
    } else {
        raw_value =
            (uint16_t)(((uint16_t)app.sensor_word_high << 8U) |
                       sample->value);
        app.sensor_read_phase = SENSOR_READ_NONE;
        accept_calculated_sensor_value(
            slot, sensors[slot].calculate(raw_value), now_ms);
    }
}

static void handle_sensor_read_error(uint32_t now_ms)
{
    app.sensor_read_inflight = false;
    app.sensor_read_phase = SENSOR_READ_NONE;
    if (app.display_page != DISPLAY_PAGE_POST) {
        app.sensor_refresh_pending = true;
        app.sensor_retry_ms = now_ms + SENSOR_READ_RETRY_MS;
    }
}

static void service_sio_i2c(uint32_t now_ms)
{
    SioPostcodeSample sample;
    SioRegisterSample register_sample;
    bool had_error;
    bool have_sample;
    bool have_register_sample;
    bool sensor_error;
    bool recovered_from_error;

    sio_i2c_service(now_ms);
    had_error = sio_i2c_take_error();
    have_sample = sio_i2c_take_sample(&sample);
    have_register_sample = sio_i2c_take_register_sample(&register_sample);
    sensor_error = had_error && app.sensor_read_inflight;
    recovered_from_error =
        (have_sample || have_register_sample) && app.postcode_error_active;

    if (have_sample || have_register_sample) {
        app.sio_error_candidate = false;
        app.postcode_error_active = false;
    }

    if (have_register_sample) {
        accept_sensor_sample(&register_sample, now_ms);
    }
    if (sensor_error) {
        handle_sensor_read_error(now_ms);
    }

    service_sensor_carousel(now_ms);

    if (app.source != SOURCE_I2C) {
        return;
    }
    if (have_sample) {
        app.next_poll_ms = now_ms + POST_POLL_INTERVAL_MS;
        ingest_code(sample.code, 1U, false, now_ms);
        ingest_port81(sample.port81_code, now_ms);
    }
    if (had_error) {
        if (!sensor_error) {
            /* An absent or unpowered SIO can NACK immediately. Retrying at
               the normal 5 ms cadence repeatedly deinitializes and recovers
               hardware I2C, starving the chunked software-I2C OLED flush. */
            app.next_poll_ms = now_ms + SIO_ERROR_RETRY_INTERVAL_MS;
        }
        if (!app.sio_error_candidate) {
            app.sio_error_candidate = true;
            app.sio_error_since_ms = now_ms;
        }
    }

    /* Keep the last valid POST code across isolated NACKs/recoveries. Only a
       continuous loss of valid samples is a user-visible communication error. */
    if (app.sio_error_candidate && !app.postcode_error_active &&
        time_reached(now_ms, app.sio_error_since_ms + SIO_ERROR_CONFIRM_MS)) {
        enter_postcode_error(now_ms);
    }

    if (recovered_from_error &&
        (app.display_page != DISPLAY_PAGE_POST)) {
        app.sensor_refresh_pending = true;
        app.sensor_retry_ms = now_ms;
        display_sensor_page(false, now_ms);
    }

    prepare_sensor_read(now_ms);
    if ((app.sensor_read_phase != SENSOR_READ_NONE) &&
        !app.sensor_read_inflight && !sio_i2c_is_busy() &&
        time_reached(now_ms, app.sensor_retry_ms)) {
        if (sio_i2c_start_data_read(SIO_MONITOR_PAGE,
                                    sensor_request_index())) {
            app.sensor_read_inflight = true;
        } else {
            app.sensor_retry_ms = now_ms + SENSOR_READ_RETRY_MS;
        }
    }

    if (time_reached(now_ms, app.next_poll_ms) && !sio_i2c_is_busy() &&
        !app.sensor_read_inflight) {
        (void)sio_i2c_start_poll();
        app.next_poll_ms = now_ms + POST_POLL_INTERVAL_MS;
    }
}

static void service_combo(bool both_pressed, uint32_t now_ms)
{
    if (both_pressed && !app.both_pressed_latch) {
        app.combo_start_ms = now_ms;
        app.combo_short_armed = true;
        app.combo_role_fired = false;
        app.combo_lock_fired = false;
        app.combo_clear_fired = false;
        reset_page_click_sequence();
    }

    if (both_pressed) {
        uint32_t held_ms = now_ms - app.combo_start_ms;

        if (!app.combo_role_fired && (held_ms >= BUTTON_COMBO_ROLE_MS)) {
            app.combo_role_fired = true;
            app.combo_short_armed = false;
            if (!app.mode_locked) {
                app.button_role = app.button_role == BUTTON_ROLE_BROWSE
                                      ? BUTTON_ROLE_GPIO
                                      : BUTTON_ROLE_BROWSE;
                app.browse_active = false;
                reset_page_click_sequence();
                display_redraw(true, now_ms);
            }
        }
        if (!app.combo_lock_fired && (held_ms >= BUTTON_COMBO_LOCK_MS)) {
            app.combo_lock_fired = true;
            app.combo_short_armed = false;
            app.mode_locked = !app.mode_locked;
            display_redraw(true, now_ms);
        }
        if (!app.combo_clear_fired && (held_ms >= BUTTON_COMBO_CLEAR_MS)) {
            app.combo_clear_fired = true;
            app.combo_short_armed = false;
            app.clear_output_on = !app.clear_output_on;
            board_output_clear(app.clear_output_on);
        }
    }

    if (!both_pressed && app.both_pressed_latch) {
        if (app.combo_short_armed && !app.mode_locked) {
            switch_source(app.source == SOURCE_I2C ? SOURCE_UART : SOURCE_I2C, now_ms);
        }
        app.combo_short_armed = false;
        app.combo_role_fired = false;
        app.combo_lock_fired = false;
        app.combo_clear_fired = false;
    }
    app.both_pressed_latch = both_pressed;
}

static void service_gpio_role(bool both_pressed, uint32_t now_ms)
{
    bool previous_only = !both_pressed && app.previous_button.stable_pressed &&
                         !app.next_button.stable_pressed;
    bool next_only = !both_pressed && app.next_button.stable_pressed &&
                     !app.previous_button.stable_pressed;

    app.browse_active = false;

    if (both_pressed || app.singles_suppressed_until_release) {
        app.previous_output_fired = false;
        app.next_output_fired = false;
        return;
    }

    if (previous_only) {
        if (!app.previous_output_fired &&
            ((uint32_t)(now_ms - app.previous_button.press_start_ms) >= BUTTON_SINGLE_OUTPUT_MS)) {
            app.previous_output_fired = true;
            app.power_output_on = !app.power_output_on;
            board_output_power(app.power_output_on);
        }
    } else {
        app.previous_output_fired = false;
    }

    if (next_only) {
        if (!app.next_output_fired &&
            ((uint32_t)(now_ms - app.next_button.press_start_ms) >= BUTTON_SINGLE_OUTPUT_MS)) {
            app.next_output_fired = true;
            app.reset_output_on = !app.reset_output_on;
            board_output_reset(app.reset_output_on);
        }
    } else {
        app.next_output_fired = false;
    }
}

static DisplayPage adjacent_display_page(DisplayPage page, bool up)
{
    if (up) {
        switch (page) {
        case DISPLAY_PAGE_POST:
            return DISPLAY_PAGE_FAN;
        case DISPLAY_PAGE_TEMPERATURE:
            return DISPLAY_PAGE_POST;
        case DISPLAY_PAGE_VOLTAGE:
            return DISPLAY_PAGE_TEMPERATURE;
        case DISPLAY_PAGE_FAN:
        default:
            return DISPLAY_PAGE_VOLTAGE;
        }
    }

    switch (page) {
    case DISPLAY_PAGE_POST:
        return DISPLAY_PAGE_TEMPERATURE;
    case DISPLAY_PAGE_TEMPERATURE:
        return DISPLAY_PAGE_VOLTAGE;
    case DISPLAY_PAGE_VOLTAGE:
        return DISPLAY_PAGE_FAN;
    case DISPLAY_PAGE_FAN:
    default:
        return DISPLAY_PAGE_POST;
    }
}

static bool scroll_display_page(bool up, uint32_t now_ms)
{
    DisplayPage candidate = app.display_page;
    uint8_t attempts;

    for (attempts = 0U; attempts < 4U; ++attempts) {
        uint8_t first_sensor;

        candidate = adjacent_display_page(candidate, up);
        if (candidate == DISPLAY_PAGE_POST) {
            if (app.display_page != DISPLAY_PAGE_POST) {
                exit_sensor_page(now_ms);
            } else {
                reset_page_click_sequence();
                display_redraw(true, now_ms);
            }
            return true;
        }
        if (first_sensor_for_page(candidate, &first_sensor)) {
            enter_sensor_page(candidate, now_ms, true);
            return true;
        }
    }
    reset_page_click_sequence();
    return false;
}

static bool service_page_click_sequence(bool up_pressed,
                                        bool down_pressed,
                                        uint32_t now_ms)
{
    if ((app.source != SOURCE_I2C) ||
        (app.button_role != BUTTON_ROLE_BROWSE) ||
        (!up_pressed && !down_pressed)) {
        return false;
    }

    if ((app.page_click_deadline_ms != 0U) &&
        time_reached(now_ms, app.page_click_deadline_ms)) {
        reset_page_click_sequence();
    }
    app.page_click_deadline_ms = now_ms + BUTTON_PAGE_CLICK_GAP_MS;

    if (up_pressed) {
        app.down_click_count = 0U;
        if (app.up_click_count < BUTTON_PAGE_CLICK_COUNT) {
            ++app.up_click_count;
        }
        if (app.up_click_count >= BUTTON_PAGE_CLICK_COUNT) {
            return scroll_display_page(true, now_ms);
        }
    } else {
        app.up_click_count = 0U;
        if (app.down_click_count < BUTTON_PAGE_CLICK_COUNT) {
            ++app.down_click_count;
        }
        if (app.down_click_count >= BUTTON_PAGE_CLICK_COUNT) {
            return scroll_display_page(false, now_ms);
        }
    }
    return false;
}

static void service_browse_role(bool both_pressed,
                                ButtonEvents previous_events,
                                ButtonEvents next_events,
                                uint32_t now_ms)
{
    bool previous_action = previous_events.pressed || previous_events.repeated;
    bool next_action = next_events.pressed || next_events.repeated;
    uint32_t oldest;
    uint32_t newest;
    PostCode code;

    app.previous_output_fired = false;
    app.next_output_fired = false;
    if (both_pressed || app.singles_suppressed_until_release ||
        (!previous_action && !next_action) || (app.history.count == 0U)) {
        return;
    }

    oldest = code_history_oldest(&app.history);
    newest = code_history_newest(&app.history);
    if (!app.browse_active) {
        app.browse_active = true;
        app.browse_cursor = newest;
        app.browse_enter_total = app.history.total_pushed;
    }
    if (app.browse_cursor < oldest) {
        app.browse_cursor = oldest;
    }
    if (app.browse_cursor > newest) {
        app.browse_cursor = newest;
    }
    if (previous_action && (app.browse_cursor > oldest)) {
        --app.browse_cursor;
    }
    if (next_action && (app.browse_cursor < newest)) {
        ++app.browse_cursor;
    }

    if (code_history_get(&app.history, app.browse_cursor, &code)) {
        display_code(code, true, true, now_ms);
    }
    app.browse_deadline_ms = now_ms + BUTTON_BROWSE_TIMEOUT_MS;
}

static void leave_browse_mode(uint32_t now_ms)
{
    uint32_t newest;
    PostCode latest;
    bool added_during_browse;

    app.browse_active = false;
    if (app.history.count == 0U) {
        return;
    }

    newest = code_history_newest(&app.history);
    if (!code_history_get(&app.history, newest, &latest)) {
        return;
    }
    added_during_browse = app.history.total_pushed > app.browse_enter_total;
    if (added_during_browse) {
        if (app.history.pending_abs < app.browse_enter_total) {
            app.history.pending_abs = app.browse_enter_total;
        }
        if (app.history.pending_abs >= app.history.total_pushed) {
            display_code(latest, false, true, now_ms);
        }
    } else {
        display_code(latest, false, true, now_ms);
        code_history_clear_pending(&app.history);
        app.last_enqueued_code = latest;
        app.last_enqueued_valid = true;
    }
    app.next_display_ms = now_ms + DISPLAY_CODE_HOLD_MS;
}

static void service_buttons(uint32_t now_ms)
{
    ButtonEvents previous_events;
    ButtonEvents next_events;
    bool both_pressed;
    bool any_event;
    bool panel_was_off;
    bool page_scrolled;

    previous_events = button_update(&app.previous_button,
                                    board_button_prev_pressed(),
                                    now_ms);
    next_events = button_update(&app.next_button,
                                board_button_next_pressed(),
                                now_ms);
    both_pressed = app.previous_button.stable_pressed && app.next_button.stable_pressed;

    if (both_pressed) {
        app.singles_suppressed_until_release = true;
    } else if (!app.previous_button.stable_pressed && !app.next_button.stable_pressed) {
        app.singles_suppressed_until_release = false;
    }

    service_combo(both_pressed, now_ms);

    any_event = previous_events.pressed || previous_events.released || previous_events.repeated ||
                next_events.pressed || next_events.released || next_events.repeated || both_pressed;
    if (any_event) {
        panel_was_off = !app.display_panel_on;
        display_wake(now_ms);
        if (panel_was_off && app.display_panel_on) {
            display_redraw(false, now_ms);
        }
    }

    if ((app.display_page != DISPLAY_PAGE_POST) && both_pressed) {
        exit_sensor_page(now_ms);
        return;
    }

    page_scrolled = service_page_click_sequence(previous_events.pressed,
                                                 next_events.pressed,
                                                 now_ms);
    if (page_scrolled) {
        return;
    }

    /* Individual sensor-page clicks belong to the five-click page gesture.
       Do not reinterpret them as POST-history browsing. */
    if (app.display_page != DISPLAY_PAGE_POST) {
        return;
    }

    if (app.button_role == BUTTON_ROLE_GPIO) {
        service_gpio_role(both_pressed, now_ms);
    } else {
        service_browse_role(both_pressed, previous_events, next_events, now_ms);
    }
}

static void service_post_override_return(uint32_t now_ms)
{
    if (!app.post_override_active) {
        return;
    }
    if (app.source != SOURCE_I2C) {
        app.post_override_active = false;
        return;
    }
    if (!time_reached(now_ms, app.post_override_deadline_ms) ||
        (app.history.pending_abs < app.history.total_pushed) ||
        app.display_frame_flushing || ssd1315_is_busy() ||
        !time_reached(now_ms, app.next_display_ms)) {
        return;
    }

    app.post_override_active = false;
#if DEFAULT_BOOT_SCREEN == BOOT_SCREEN_VOLTAGE
    enter_sensor_page(DISPLAY_PAGE_VOLTAGE, now_ms, false);
#elif DEFAULT_BOOT_SCREEN == BOOT_SCREEN_TEMPERATURE
    enter_sensor_page(DISPLAY_PAGE_TEMPERATURE, now_ms, false);
#elif DEFAULT_BOOT_SCREEN == BOOT_SCREEN_FAN
    enter_sensor_page(DISPLAY_PAGE_FAN, now_ms, false);
#endif
}

static void service_display(uint32_t now_ms)
{
    PostCode queued_code;
    bool display_busy;

    if (!ssd1315_is_online()) {
        if (time_reached(now_ms, app.next_display_retry_ms)) {
            app.next_display_retry_ms = now_ms + DISPLAY_RETRY_INTERVAL_MS;
            if (ssd1315_init()) {
                app.display_panel_on = true;
                app.display_dimmed = false;
                app.display_last_activity_ms = now_ms;
                app.next_shift_ms = now_ms + DISPLAY_PIXEL_SHIFT_INTERVAL_MS;
                display_redraw(false, now_ms);
            }
        }
        return;
    }

    service_post_override_return(now_ms);
    display_busy = ssd1315_is_busy();

    if (app.display_frame_flushing && !display_busy) {
        app.display_frame_flushing = false;
        /* Match the original: hold starts after the complete frame is visible. */
        app.next_display_ms = now_ms + DISPLAY_CODE_HOLD_MS;
    }

    if ((app.display_page == DISPLAY_PAGE_POST) && app.browse_active &&
        time_reached(now_ms, app.browse_deadline_ms)) {
        leave_browse_mode(now_ms);
    }

    if (ssd1315_is_online() && (app.display_page == DISPLAY_PAGE_POST) &&
        !app.browse_active && !display_busy &&
        time_reached(now_ms, app.next_display_ms)) {
        if (code_history_pop_pending(&app.history, &queued_code)) {
            display_code(queued_code, false, true, now_ms);
        }
    }

    if (app.display_top_bar_visible &&
        (DISPLAY_TOP_BAR_TIMEOUT_MS != 0U) &&
        ((uint32_t)(now_ms - app.display_last_activity_ms) >=
         DISPLAY_TOP_BAR_TIMEOUT_MS) &&
        (!app.display_panel_on || !ssd1315_is_busy())) {
        app.display_top_bar_visible = false;
        if (app.display_panel_on) {
            display_redraw(false, now_ms);
            display_busy = ssd1315_is_busy();
        }
    }

    if ((DISPLAY_PIXEL_SHIFT_PIXELS > 0U) && app.display_panel_on &&
        ((DISPLAY_OFF_TIMEOUT_MS == 0U) ||
         ((uint32_t)(now_ms - app.display_last_activity_ms) <
          DISPLAY_OFF_TIMEOUT_MS)) &&
        !ssd1315_is_busy() && time_reached(now_ms, app.next_shift_ms)) {
        ssd1315_cycle_shift(DISPLAY_PIXEL_SHIFT_PIXELS);
        display_redraw(false, now_ms);
        app.next_shift_ms = now_ms + DISPLAY_PIXEL_SHIFT_INTERVAL_MS;
    }

    if (ssd1315_is_online() && app.display_panel_on) {
        uint32_t idle_ms = now_ms - app.display_last_activity_ms;
        if ((DISPLAY_DIM_TIMEOUT_MS != 0U) && !app.display_dimmed &&
            (idle_ms >= DISPLAY_DIM_TIMEOUT_MS)) {
            if (ssd1315_set_contrast(DISPLAY_CONTRAST_DIM)) {
                app.display_dimmed = true;
            }
        }
        if ((DISPLAY_OFF_TIMEOUT_MS != 0U) &&
            (idle_ms >= DISPLAY_OFF_TIMEOUT_MS)) {
            if (ssd1315_set_power(false)) {
                app.display_panel_on = false;
                /* Wake must restore full contrast after powering the panel on. */
                app.display_dimmed = true;
            }
        }
    }

    /* One command or <=16 data bytes per pass; interrupts remain enabled. */
    ssd1315_service();
}

static void application_init(void)
{
    uint32_t now_ms;

    board_init();
    /* Match the proven RP2040 design: allow the OLED rail/controller a full
       second to start before the first address probe. */
    HAL_Delay(DISPLAY_POWER_UP_DELAY_MS);

    (void)sio_i2c_init();
    (void)uart_rx_init();
    (void)uart_fast_rx_init();

    now_ms = HAL_GetTick();
    button_init(&app.previous_button, board_button_prev_pressed(), now_ms);
    button_init(&app.next_button, board_button_next_pressed(), now_ms);

    app.source = SOURCE_I2C;
    app.uart_input = UART_INPUT_NONE;
    app.fast_uart_record_count = uart_fast_rx_record_count();
    app.fast_uart_error_count = uart_fast_rx_error_count();
    app.button_role = BUTTON_ROLE_BROWSE;
    app.mode_locked = false;
    app.browse_active = false;
    app.both_pressed_latch = false;
    app.combo_short_armed = false;
    app.combo_role_fired = false;
    app.combo_lock_fired = false;
    app.combo_clear_fired = false;
    app.previous_output_fired = false;
    app.next_output_fired = false;
    app.singles_suppressed_until_release = false;
    app.power_output_on = false;
    app.reset_output_on = false;
    app.clear_output_on = false;
    board_output_power(false);
    board_output_reset(false);
    board_output_clear(false);

    app.display_panel_on = ssd1315_init();
    app.display_dimmed = false;
    app.display_top_bar_visible = true;
    app.display_frame_flushing = false;
    app.display_last_activity_ms = now_ms;
    app.next_shift_ms = now_ms + DISPLAY_PIXEL_SHIFT_INTERVAL_MS;
    app.next_display_retry_ms = now_ms + DISPLAY_RETRY_INTERVAL_MS;
    app.next_poll_ms = now_ms;
    reset_code_pipeline(now_ms);
#if DEFAULT_BOOT_SCREEN == BOOT_SCREEN_VOLTAGE
    enter_sensor_page(DISPLAY_PAGE_VOLTAGE, now_ms, true);
#elif DEFAULT_BOOT_SCREEN == BOOT_SCREEN_TEMPERATURE
    enter_sensor_page(DISPLAY_PAGE_TEMPERATURE, now_ms, true);
#elif DEFAULT_BOOT_SCREEN == BOOT_SCREEN_FAN
    enter_sensor_page(DISPLAY_PAGE_FAN, now_ms, true);
#endif
}

int main(void)
{
    application_init();

    for (;;) {
        uint32_t now_ms = HAL_GetTick();

        service_uart(now_ms);
        service_sio_i2c(now_ms);
        service_buttons(now_ms);
        service_display(now_ms);
    }
}
