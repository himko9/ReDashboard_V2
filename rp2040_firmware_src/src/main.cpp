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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/uart.h"
#include "pico/stdlib.h"

#include "app_config.h"
#include "board_config.h"
#include "board_io.h"
#include "buttons.h"
#include "code_history.h"
#include "display_config.h"
#include "i2c_bus.h"
#include "oled_display.h"
#include "post_uart.h"
#include "sensors.h"
#include "sio_monitor.h"

#ifndef POST80_DEBUG
#define POST80_DEBUG 0
#endif

#if POST80_DEBUG
#define DEBUG_PRINTF(...)     \
    do {                      \
        printf(__VA_ARGS__);  \
        fflush(stdout);       \
    } while (0)
#else
#define DEBUG_PRINTF(...) \
    do {                  \
    } while (0)
#endif

enum class UiMode : uint8_t {
    Normal,
    Menu,
};

static constexpr uint32_t UART_BAUD_OPTIONS[] = {
    9600U, 19200U, 38400U, 57600U,
    115200U, 230400U, 460800U, 921600U,
};
static constexpr size_t UART_BAUD_OPTIONS_COUNT =
    sizeof(UART_BAUD_OPTIONS) / sizeof(UART_BAUD_OPTIONS[0]);

static size_t find_baud_index(uint32_t baud) {
    for (size_t index = 0U; index < UART_BAUD_OPTIONS_COUNT; ++index) {
        if (UART_BAUD_OPTIONS[index] == baud) return index;
    }
    return 0U;
}

static bool sensor_change_exceeds_wake_threshold(int32_t previous,
                                                  int32_t current) {
#if SENSOR_WAKE_CHANGE_PERCENT == 0U
    (void)previous;
    (void)current;
    return false;
#else
    int64_t delta = (int64_t)current - (int64_t)previous;
    int64_t baseline = previous;
    if (delta < 0) delta = -delta;
    if (baseline < 0) baseline = -baseline;
    if (baseline == 0) return delta != 0;
    return delta * 100 >
           baseline * (int64_t)SENSOR_WAKE_CHANGE_PERCENT;
#endif
}

int main() {
    stdio_init_all();  // USB CDC serial output (ASCII)

    uint32_t uart_baud_current = UART_IN_BAUD;
    i2c_bus_init(POST_BUS);
    i2c_bus_init(OLED_BUS);
    uart_init(UART_IN_INST, uart_baud_current);
    gpio_set_function(UART_IN_RX_PIN, GPIO_FUNC_UART);
    gpio_pull_up(UART_IN_RX_PIN);
    uart_set_hw_flow(UART_IN_INST, false, false);
    uart_set_format(UART_IN_INST, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_IN_INST, true);
    bool uart_dma_ok = uart_dma_init();
    bool fast_uart_ok = fast_uart_init();

    board_outputs_init();
    bool mosfet_pwr_on = false;
    bool mosfet_rst_on = false;
    bool mosfet_clr_on = false;
    board_output_write(MOSFET_PWR_PIN, mosfet_pwr_on);
    board_output_write(MOSFET_RST_PIN, mosfet_rst_on);
    board_output_write(MOSFET_CLR_PIN, mosfet_clr_on);

    sleep_ms(DISPLAY_POWER_UP_DELAY_MS);

    printf("POST80 poller start\r\n");
    printf("POST bus: %s SDA=%u SCL=%u BAUD=%u TARGET=0x%02X\r\n",
           i2c_bus_name(POST_BUS.inst),
           POST_BUS.sda_pin,
           POST_BUS.scl_pin,
           POST_BUS.baud_hz,
           SIO_I2C_ADDRESS);
    printf("OLED bus: %s SDA=%u SCL=%u BAUD=%u OLED=0x%02X\r\n",
           i2c_bus_name(OLED_BUS.inst),
           OLED_BUS.sda_pin,
           OLED_BUS.scl_pin,
           OLED_BUS.baud_hz,
           OLED_I2C_ADDRESS);
    printf("Shared code buffer cap=%u\r\n", (unsigned)CODE_BUFFER_CAPACITY);
    printf("Buttons: PWR=GPIO%u RST=GPIO%u (%s)\r\n",
           BTN_PREV_PIN,
           BTN_NEXT_PIN,
           button_mode_name());
    printf("Button role: history/page scroll (five clicks change page)\r\n");
    printf("Combo hold: >%ums mode toggle, >%ums lock, >%ums CLR toggle\r\n",
           BTN_COMBO_NAV_MS, BTN_COMBO_LOCK_MS, BTN_COMBO_CLEAR_MS);
    printf("UART input: %s RX=GPIO%u BAUD=%u DMA=%s\r\n",
           uart_instance_name(UART_IN_INST), UART_IN_RX_PIN, uart_baud_current,
           uart_dma_ok ? "OK" : "OFFLINE");
    printf("Fast POST input: PIO RX=GPIO%u BAUD=%u %s\r\n",
           RP2040_FAST_UART_RX_PIN,
           RP2040_FAST_UART_BAUD,
           fast_uart_ok ? "OK" : "OFFLINE");
    printf("MOSFET outputs: PWR=GPIO%u RST=GPIO%u CLR=GPIO%u (%s)\r\n",
           MOSFET_PWR_PIN,
           MOSFET_RST_PIN,
           MOSFET_CLR_PIN,
           MOSFET_ACTIVE_LOW ? "active-low gate" : "active-high gate");
    fflush(stdout);

    bool oled_ok = i2c_bus_ensure_ready(OLED_BUS) && oled_init();
    bool oled_panel_on = oled_ok;
    bool oled_dimmed = false;
    absolute_time_t oled_last_activity = get_absolute_time();
    static const int SHIFT_X_SEQ[] = {
        0, (int)DISPLAY_PIXEL_SHIFT_PIXELS, 0,
        -(int)DISPLAY_PIXEL_SHIFT_PIXELS};
    static const int SHIFT_Y_SEQ[] = {
        0, 0, (int)DISPLAY_PIXEL_SHIFT_PIXELS, 0};
    static constexpr size_t SHIFT_SEQ_LEN = sizeof(SHIFT_X_SEQ) / sizeof(SHIFT_X_SEQ[0]);
    size_t shift_idx = 0;
    oled_set_pixel_shift(SHIFT_X_SEQ[shift_idx], SHIFT_Y_SEQ[shift_idx]);
    absolute_time_t next_shift_update = make_timeout_time_ms(OLED_SHIFT_INTERVAL_MS);
    absolute_time_t next_oled_retry =
        make_timeout_time_ms(DISPLAY_RETRY_INTERVAL_MS);
    if (oled_ok) {
        oled_ok = oled_set_contrast(OLED_CONTRAST_FULL);
    }
    if (oled_ok) {
        oled_ok = oled_render_code(oled_ok,
                                   &oled_panel_on,
                                   &oled_last_activity,
                                   PostCode{0U, 1U},
                                   false, 0, 0, 0);
    }
    printf("OLED=%s\r\n", oled_ok ? "OK" : "OFFLINE");
    fflush(stdout);

    PostCode last_seen_code = {};
    bool last_seen_valid = false;
    PostCode last_enqueued_code = {};
    bool last_enqueued_valid = false;
    PostCode current_oled_code = {0U, 1U};
    bool post_error_active = false;
    bool sio_error_candidate = false;
    absolute_time_t sio_error_since = get_absolute_time();
    absolute_time_t next_sio_poll = get_absolute_time();
    uint8_t jdash_last_p80 = 0U;
    bool jdash_last_p80_valid = false;
    uint32_t err_count = 0;
    DataSourceMode data_mode = DataSourceMode::I2cPoll;
    ButtonRoleMode button_role_mode = ButtonRoleMode::ScrollBrowse;
    oled_ui.nav_tag =
        (button_role_mode == ButtonRoleMode::GpioControl) ? 'G' : 'S';
    UiMode ui_mode = UiMode::Normal;
    size_t uart_baud_idx_current = find_baud_index(uart_baud_current);
    size_t menu_baud_idx_edit = uart_baud_idx_current;
    CodeBuffer code_buf = {};
    code_buffer_init(&code_buf);
    ButtonState btn_prev = {};
    ButtonState btn_next = {};
    button_init(&btn_prev, BTN_PREV_PIN);
    button_init(&btn_next, BTN_NEXT_PIN);
    bool btn_mode_locked = false;
    oled_ui.mode_locked = btn_mode_locked;
    bool both_pressed_latch = false;
    absolute_time_t both_press_start = get_absolute_time();
    bool both_short_armed = false;
    bool combo_nav_fired = false;
    bool combo_lock_fired = false;
    bool combo_clear_fired = false;
    bool prev_single_fired = false;
    bool next_single_fired = false;
    bool browse_mode = false;
    uint32_t browse_cursor_abs = 0;
    uint32_t browse_enter_total_pushed = 0;
    absolute_time_t browse_deadline = get_absolute_time();
    absolute_time_t next_oled_update = make_timeout_time_ms(OLED_CODE_HOLD_MS);
    DisplayPage display_page = configured_default_page();
    size_t sensor_index = 0U;
    size_t sensor_refresh_index = 0U;
    int32_t sensor_values[SENSOR_COUNT] = {};
    uint32_t sensor_valid_mask = 0U;
    bool sensor_refresh_pending = false;
    bool sensor_wait_rendered = false;
    bool sensor_refresh_marks_activity = true;
    absolute_time_t sensor_page_deadline =
        make_timeout_time_ms(SENSOR_PAGE_INTERVAL_MS);
    absolute_time_t sensor_retry_deadline = get_absolute_time();
    uint8_t port81_zero_polls = 0U;
    uint8_t prev_page_click_count = 0U;
    uint8_t next_page_click_count = 0U;
    absolute_time_t page_click_deadline = get_absolute_time();
    bool page_click_sequence_active = false;
    bool post_override_active = false;
    absolute_time_t post_override_deadline = get_absolute_time();

    if (display_page != DisplayPage::Post &&
        first_sensor_for_page(display_page, &sensor_index)) {
        sensor_refresh_index = sensor_index;
        sensor_refresh_pending = true;
    } else {
        display_page = DisplayPage::Post;
    }

    auto reset_code_pipeline = [&]() {
        code_buffer_init(&code_buf);
        last_seen_code = {};
        last_seen_valid = false;
        last_enqueued_code = {};
        last_enqueued_valid = false;
        post_error_active = false;
        sio_error_candidate = false;
        oled_ui.sio_comm_error = false;
        oled_ui.port81_visible = false;
        oled_ui.port81_code = 0U;
        port81_zero_polls = 0U;
        err_count = 0;
        browse_mode = false;
        display_page = DisplayPage::Post;
        sensor_refresh_pending = false;
        sensor_wait_rendered = false;
        sensor_refresh_marks_activity = false;
        sensor_valid_mask = 0U;
        sensor_retry_deadline = get_absolute_time();
        post_override_active = false;
        prev_page_click_count = 0U;
        next_page_click_count = 0U;
        page_click_sequence_active = false;
        both_pressed_latch = false;
        both_short_armed = false;
        combo_nav_fired = false;
        combo_lock_fired = false;
        combo_clear_fired = false;
        prev_single_fired = false;
        next_single_fired = false;
        browse_cursor_abs = 0;
        browse_enter_total_pushed = 0;
        next_oled_update = make_timeout_time_ms(OLED_CODE_HOLD_MS);
    };

    auto ingest_code = [&](uint16_t value,
                           uint8_t width_bytes,
                           bool allow_zero) {
        if ((width_bytes != 1U && width_bytes != 2U) ||
            (!allow_zero && value == 0U)) {
            return;
        }
        PostCode code = {value, width_bytes};
        if (!last_seen_valid || last_seen_code.value != code.value ||
            last_seen_code.width_bytes != code.width_bytes) {
            last_seen_code = code;
            last_seen_valid = true;
            if (width_bytes == 1U) {
                printf("%02X\r\n", (unsigned)value);
            } else {
                printf("%04X\r\n", (unsigned)value);
            }
            fflush(stdout);
        }
        if (!last_enqueued_valid ||
            last_enqueued_code.value != code.value ||
            last_enqueued_code.width_bytes != code.width_bytes) {
            last_enqueued_code = code;
            last_enqueued_valid = true;
            code_buffer_push(&code_buf, code);
            display_page = DisplayPage::Post;
            sensor_refresh_pending = false;
            sensor_refresh_marks_activity = false;
            if (data_mode == DataSourceMode::I2cPoll &&
                DEFAULT_BOOT_SCREEN != BOOT_SCREEN_POST &&
                DISPLAY_POST_RETURN_DELAY_MS != 0U) {
                post_override_active = true;
                post_override_deadline =
                    make_timeout_time_ms(DISPLAY_POST_RETURN_DELAY_MS);
            } else {
                post_override_active = false;
            }
        }
    };

    auto switch_to_uart_mode = [&]() {
        if (data_mode == DataSourceMode::UartInput) {
            return;
        }
        data_mode = DataSourceMode::UartInput;
        oled_ui.mode_tag = "JUART";
        reset_code_pipeline();
        printf("MODE UART\r\n");
        fflush(stdout);
    };

    auto switch_to_i2c_mode = [&]() {
        if (data_mode == DataSourceMode::I2cPoll) {
            return;
        }
        data_mode = DataSourceMode::I2cPoll;
        oled_ui.mode_tag = "JDASH";
        reset_code_pipeline();
        next_sio_poll = get_absolute_time();
        printf("MODE I2C\r\n");
        fflush(stdout);
    };

    auto menu_render_baud = [&]() {
        if (oled_ok) {
            oled_ui.top_bar_visible = true;
            if (!oled_panel_on) {
                oled_ok = oled_set_power(true);
                if (oled_ok) {
                    oled_panel_on = true;
                }
            }
            if (oled_ok) {
                oled_ok = oled_show_baud_value(UART_BAUD_OPTIONS[menu_baud_idx_edit]);
                if (oled_ok) {
                    oled_last_activity = get_absolute_time();
                }
            }
        }
        printf("MENU BAUD idx=%u val=%u\r\n",
               (unsigned)menu_baud_idx_edit,
               (unsigned)UART_BAUD_OPTIONS[menu_baud_idx_edit]);
        fflush(stdout);
    };

    auto render_sensor_page = [&](bool mark_activity) {
        if (!oled_ok || display_page == DisplayPage::Post ||
            sensor_index >= SENSOR_COUNT) {
            return;
        }
        if (!oled_panel_on) {
            oled_ok = oled_set_power(true);
            if (!oled_ok) return;
            oled_panel_on = true;
        }
        if (mark_activity) {
            oled_ui.top_bar_visible = true;
        }
        char value[9] = "WAIT";
        const char *label = SENSORS[sensor_index].label;
        char role = SENSORS[sensor_index].type == SENSOR_TYPE_VIN
                        ? 'V'
                        : (SENSORS[sensor_index].type == SENSOR_TYPE_THR ? 'T' : 'F');
        if (oled_ui.sio_comm_error) {
            memcpy(value, "ERROR", 6U);
            label = "SIO COMM ERROR";
            role = '!';
        } else if ((sensor_valid_mask & (UINT32_C(1) << sensor_index)) != 0U) {
            format_sensor_value(sensor_index, sensor_values[sensor_index], value);
        }
        oled_ok = oled_show_sensor_value(value, label, role);
        if (oled_ok && mark_activity) {
            oled_last_activity = get_absolute_time();
        }
    };

    auto redraw_current_oled = [&](bool mark_activity = true) {
        if (!oled_ok) {
            return;
        }
        if (ui_mode == UiMode::Menu) {
            menu_render_baud();
            return;
        }
        if (display_page != DisplayPage::Post) {
            /* Page entry schedules an immediate SIO read. Keep the existing
            * frame until that read finishes instead of transmitting a full
            * WAIT frame and then a second full value frame. */
            if (sensor_refresh_pending && !oled_ui.sio_comm_error) {
                return;
            }
            render_sensor_page(mark_activity);
            return;
        }
        if (button_role_mode == ButtonRoleMode::ScrollBrowse && browse_mode && code_buf.count > 0) {
            uint32_t oldest = code_buffer_oldest_abs(&code_buf);
            uint32_t newest = code_buffer_newest_abs(&code_buf);
            if (browse_cursor_abs < oldest) {
                browse_cursor_abs = oldest;
            }
            if (browse_cursor_abs > newest) {
                browse_cursor_abs = newest;
            }
            PostCode show = {};
            if (code_buffer_get_abs(&code_buf, browse_cursor_abs, &show)) {
                oled_ok = oled_render_code(
                    oled_ok, &oled_panel_on, &oled_last_activity, show, true,
                    oldest, newest, browse_cursor_abs, mark_activity);
                current_oled_code = show;
            }
            return;
        }
        oled_ok = oled_render_code(
            oled_ok, &oled_panel_on, &oled_last_activity,
            current_oled_code, false, 0, 0, 0, mark_activity);
    };

    auto update_button_touch_thresholds = [&]() {
        bool gpio_high_threshold = (ui_mode == UiMode::Normal && button_role_mode == ButtonRoleMode::GpioControl);
        uint16_t on_delta = gpio_high_threshold ? TOUCH_DELTA_ON_GPIO_MODE : TOUCH_DELTA_ON;
        uint16_t off_delta = gpio_high_threshold ? TOUCH_DELTA_OFF_GPIO_MODE : TOUCH_DELTA_OFF;
        btn_prev.touch_delta_on = on_delta;
        btn_prev.touch_delta_off = off_delta;
        btn_next.touch_delta_on = on_delta;
        btn_next.touch_delta_off = off_delta;
    };

    redraw_current_oled();

    while (true) {
        absolute_time_t next_poll = make_timeout_time_us(POST_POLL_INTERVAL_US);
        if (!oled_ok && time_reached(next_oled_retry)) {
            next_oled_retry = make_timeout_time_ms(DISPLAY_RETRY_INTERVAL_MS);
            oled_ok = i2c_bus_ensure_ready(OLED_BUS) && oled_init();
            if (oled_ok) {
                oled_panel_on = true;
                oled_dimmed = false;
                oled_ok = oled_set_contrast(OLED_CONTRAST_FULL);
                if (oled_ok) {
                    oled_last_activity = get_absolute_time();
                    redraw_current_oled(false);
                }
            }
        }
        if (ui_mode == UiMode::Normal) {
            FastUartRecord fast_record = {};
            while (fast_uart_pop(&fast_record)) {
                if (fast_record.port != 0x0080U) {
                    continue;
                }
                if (data_mode != DataSourceMode::UartInput) {
                    switch_to_uart_mode();
                }
                uint8_t port80 = (uint8_t)fast_record.payload;
                uint16_t display_value = port80;
                uint8_t width_bytes = 1U;
#if POSTCODE_P81_DECODE_ENABLED != 0U
                if (fast_record.width == 1U) {
                    uint8_t port81 = (uint8_t)(fast_record.payload >> 8U);
                    if (port81 != 0x00U && port81 != 0xFFU) {
                        display_value = (uint16_t)(((uint16_t)port80 << 8U) |
                                                   port81);
                        width_bytes = 2U;
                    }
                }
#endif
                oled_ui.port81_visible = false;
                ingest_code(display_value, width_bytes, true);
            }
            uint8_t uart_code = 0U;
            while (uart_dma_pop(&uart_code)) {
                /* A disconnected/high line can occasionally be sampled as a
                * lone FF during plug/unplug or reset. FF alone is not enough
                * evidence to leave JDASH mode; once a real 115200 byte has
                * selected UART mode, FF remains a valid POST code. */
                if (data_mode != DataSourceMode::UartInput &&
                    uart_code == 0xFFU) {
                    continue;
                }
                if (data_mode != DataSourceMode::UartInput) {
                    switch_to_uart_mode();
                    redraw_current_oled();
                }
                oled_ui.port81_visible = false;
                /* The SIO sends one raw binary POST byte, not two ASCII hex
                   characters. UART mode intentionally accepts 00. */
                ingest_code(uart_code, 1U, true);
            }
        } else {
            // Ignore UART stream while in menu.
            uint8_t discarded = 0U;
            while (uart_dma_pop(&discarded)) {}
        }

        absolute_time_t io_now = get_absolute_time();
        bool sensor_read_due =
            ui_mode == UiMode::Normal &&
            data_mode == DataSourceMode::I2cPoll &&
            display_page != DisplayPage::Post &&
            !oled_ui.sio_comm_error &&
            !sio_error_candidate &&
                        ((sensor_refresh_pending &&
                            time_reached(sensor_retry_deadline)) ||
                         (!sensor_refresh_pending &&
                            time_reached(sensor_page_deadline)));
        if (ui_mode == UiMode::Normal &&
            time_reached(next_sio_poll) && !sensor_read_due) {
            uint8_t ptr = 0;
            uint8_t code = 0;
            uint8_t port81_ptr = 0U;
            uint8_t port81_code = 0U;
            bool post_ok = false;
            if (i2c_bus_ensure_ready(POST_BUS)) {
                if (data_mode == DataSourceMode::I2cPoll) {
                    post_ok = read_post_code(&ptr,
                                             &code,
                                             &port81_ptr,
                                             &port81_code);
                } else {
                    /* Keep a lightweight P80-only probe armed while UART is
                    * selected. A real P80 transition can then take source
                    * ownership back without wasting time reading P81. */
                    post_ok = read_post_code_p80(&ptr, &code);
                }
            }
            if (!post_ok) {
                next_sio_poll =
                    make_timeout_time_ms(SIO_ERROR_RETRY_INTERVAL_MS);
                if (data_mode == DataSourceMode::I2cPoll) {
                    ++err_count;
                if (!sio_error_candidate) {
                    sio_error_candidate = true;
                    sio_error_since = io_now;
                }
                int64_t failed_us = absolute_time_diff_us(sio_error_since,
                                                          io_now);
                if (!post_error_active &&
                    failed_us >= (int64_t)SIO_ERROR_CONFIRM_MS * 1000) {
                    // AB -> 00(error) -> CD behavior: enqueue one 00 only on error entry,
                    // and only if last enqueued code was non-zero.
                    if (last_enqueued_valid &&
                        last_enqueued_code.value > 0U) {
                        PostCode error_code = {0U, 1U};
                        code_buffer_push(&code_buf, error_code);
                        last_enqueued_code = error_code;
                        last_enqueued_valid = true;
                    }
                    // Drop stale visual backlog when entering error state.
                    code_buffer_clear_pending(&code_buf);
                    next_oled_update = make_timeout_time_ms(OLED_CODE_HOLD_MS);
                    post_error_active = true;
                    oled_ui.sio_comm_error = true;
                    oled_ui.port81_visible = false;
                    port81_zero_polls = 0U;
                    sensor_valid_mask = 0U;

                    // Force a communication-error frame only in live mode;
                    // cached-history browsing remains usable.
                    if (!browse_mode && oled_ok) {
                        if (display_page == DisplayPage::Post) {
                            oled_ok = oled_render_code(
                                oled_ok, &oled_panel_on, &oled_last_activity,
                                PostCode{0U, 1U}, false, 0, 0, 0);
                            current_oled_code = {0U, 1U};
                        } else {
                            render_sensor_page(true);
                        }
                    }
                }
                if ((err_count & 0x3F) == 1) {
                    DEBUG_PRINTF("ERR %lu\r\n", (unsigned long)err_count);
                }
                }
            } else {
                bool p80_changed = jdash_last_p80_valid &&
                                   code != jdash_last_p80;
                jdash_last_p80 = code;
                jdash_last_p80_valid = true;
                next_sio_poll = make_timeout_time_ms(
                    data_mode == DataSourceMode::I2cPoll
                        ? POST_POLL_INTERVAL_MS
                        : JDASH_DETECT_POLL_INTERVAL_MS);

                if (data_mode == DataSourceMode::UartInput &&
                    p80_changed && code != 0U) {
                    switch_to_i2c_mode();
                }

                if (data_mode == DataSourceMode::I2cPoll) {
                bool recovered_from_error = post_error_active;
                sio_error_candidate = false;
                post_error_active = false;
                oled_ui.sio_comm_error = false;
                next_sio_poll = make_timeout_time_ms(POST_POLL_INTERVAL_MS);
                ingest_code(code, 1U, false);  // normal POST mode ignores 00

#if POSTCODE_P81_DECODE_ENABLED != 0U
                bool port81_changed = false;
                if (port81_code != 0U) {
                    port81_changed = !oled_ui.port81_visible ||
                                     oled_ui.port81_code != port81_code;
                    oled_ui.port81_code = port81_code;
                    oled_ui.port81_visible = true;
                    port81_zero_polls = 0U;
                } else if (oled_ui.port81_visible) {
                    if (port81_zero_polls < PORT81_ZERO_HIDE_POLLS) {
                        ++port81_zero_polls;
                    }
                    if (port81_zero_polls >= PORT81_ZERO_HIDE_POLLS) {
                        oled_ui.port81_visible = false;
                        port81_zero_polls = 0U;
                        port81_changed = true;
                    }
                }
                if (port81_changed) {
                    display_page = DisplayPage::Post;
                    sensor_refresh_pending = false;
                    sensor_refresh_marks_activity = false;
                    if (DEFAULT_BOOT_SCREEN != BOOT_SCREEN_POST &&
                        DISPLAY_POST_RETURN_DELAY_MS != 0U) {
                        post_override_active = true;
                        post_override_deadline = make_timeout_time_ms(
                            DISPLAY_POST_RETURN_DELAY_MS);
                    }
                    if (!browse_mode) redraw_current_oled();
                }
#else
                (void)port81_ptr;
                (void)port81_code;
                oled_ui.port81_visible = false;
#endif
                if (recovered_from_error) {
                    if (display_page != DisplayPage::Post) {
                        sensor_refresh_index = sensor_index;
                        sensor_refresh_pending = true;
                        sensor_wait_rendered = false;
                        sensor_refresh_marks_activity = true;
                        sensor_retry_deadline = get_absolute_time();
                        redraw_current_oled();
                    } else if (code == 0U) {
                        /* A successful zero sample proves communication has
                           returned even though normal JDASH ingest ignores
                           zero. Remove the stale error label immediately. */
                        redraw_current_oled();
                    }
                }
                }
            }
        } else if (data_mode != DataSourceMode::I2cPoll) {
            sio_error_candidate = false;
            post_error_active = false;
            oled_ui.sio_comm_error = false;
        }

        absolute_time_t sensor_now = get_absolute_time();
        if (ui_mode == UiMode::Normal &&
            data_mode == DataSourceMode::I2cPoll &&
            display_page != DisplayPage::Post &&
            !oled_ui.sio_comm_error) {
            if (!sensor_refresh_pending &&
                time_reached(sensor_page_deadline)) {
                sensor_refresh_index =
                    next_sensor_for_page(display_page, sensor_index);
                sensor_refresh_pending = true;
                sensor_wait_rendered = false;
                sensor_refresh_marks_activity = false;
                sensor_retry_deadline = sensor_now;
            }
            if (sensor_refresh_pending && !sio_error_candidate &&
                time_reached(sensor_retry_deadline)) {
                int32_t calculated = 0;
                if (i2c_bus_ensure_ready(POST_BUS) &&
                    read_sensor_value(sensor_refresh_index, &calculated)) {
                    uint32_t sensor_bit =
                        UINT32_C(1) << sensor_refresh_index;
                    bool significant_change =
                        (sensor_valid_mask & sensor_bit) != 0U &&
                        sensor_change_exceeds_wake_threshold(
                            sensor_values[sensor_refresh_index], calculated);
                    sensor_values[sensor_refresh_index] = calculated;
                    sensor_valid_mask |= sensor_bit;
                    sensor_index = sensor_refresh_index;
                    sensor_refresh_pending = false;
                    sensor_wait_rendered = false;
                    sensor_page_deadline =
                        make_timeout_time_ms(SENSOR_PAGE_INTERVAL_MS);
                    bool mark_activity = sensor_refresh_marks_activity ||
                                         significant_change;
                    sensor_refresh_marks_activity = false;
                    render_sensor_page(mark_activity);
                } else {
                    sensor_valid_mask &=
                        ~(UINT32_C(1) << sensor_refresh_index);
                    sensor_retry_deadline =
                        make_timeout_time_ms(SENSOR_READ_RETRY_MS);
                    uint32_t visible_sensor_bit =
                        UINT32_C(1) << sensor_index;
                    if ((sensor_valid_mask & visible_sensor_bit) == 0U &&
                        !sensor_wait_rendered) {
                        render_sensor_page(sensor_refresh_marks_activity);
                        sensor_wait_rendered = oled_ok;
                        if (sensor_wait_rendered) {
                            sensor_refresh_marks_activity = false;
                        }
                    }
                    /* Let the next normal P80 poll decide whether this is a
                       missing register or loss of the complete SIO. */
                    next_sio_poll = get_absolute_time();
                }
            }
        }

        if (post_override_active && data_mode == DataSourceMode::I2cPoll &&
            !browse_mode && time_reached(post_override_deadline) &&
            code_buf.pending_abs >= code_buf.total_pushed) {
            post_override_active = false;
            DisplayPage target_page = configured_default_page();
            size_t first_sensor = 0U;
            if (target_page != DisplayPage::Post &&
                first_sensor_for_page(target_page, &first_sensor)) {
                display_page = target_page;
                sensor_index = first_sensor;
                sensor_refresh_index = first_sensor;
                sensor_page_deadline =
                    make_timeout_time_ms(SENSOR_PAGE_INTERVAL_MS);
                sensor_retry_deadline = get_absolute_time();
                sensor_refresh_pending = true;
                sensor_wait_rendered = false;
                sensor_refresh_marks_activity = true;
                redraw_current_oled();
            }
        }

        update_button_touch_thresholds();
        bool prev_pressed = button_poll_pressed(&btn_prev);
        bool next_pressed = button_poll_pressed(&btn_next);
        bool prev_repeat = button_poll_repeat(&btn_prev);
        bool next_repeat = button_poll_repeat(&btn_next);
        bool both_pressed = btn_prev.stable_pressed && btn_next.stable_pressed;
        bool both_pressed_prev = both_pressed_latch;
        bool combo_short_event = false;
        bool combo_nav_event = false;
        bool combo_lock_event = false;
        bool combo_clear_event = false;
        absolute_time_t now = get_absolute_time();
        if (both_pressed && !both_pressed_prev) {
            both_press_start = now;
            both_short_armed = true;
            combo_nav_fired = false;
            combo_lock_fired = false;
            combo_clear_fired = false;
        }
        if (both_pressed) {
            int64_t held_us = absolute_time_diff_us(both_press_start, now);
            if (!combo_nav_fired && held_us >= (int64_t)BTN_COMBO_NAV_MS * 1000) {
                combo_nav_fired = true;
                both_short_armed = false;
                combo_nav_event = true;
                if (btn_mode_locked) {
                    printf("LOCKED\r\n");
                    fflush(stdout);
                } else {
                    if (button_role_mode == ButtonRoleMode::GpioControl) {
                        button_role_mode = ButtonRoleMode::ScrollBrowse;
                        oled_ui.nav_tag = 'S';
                        printf("BTNMODE NAV\r\n");
                    } else {
                        button_role_mode = ButtonRoleMode::GpioControl;
                        oled_ui.nav_tag = 'G';
                        browse_mode = false;
                        printf("BTNMODE GPIO\r\n");
                    }
                    browse_mode = false;
                    fflush(stdout);
                    redraw_current_oled();
                }
            }
            if (!combo_lock_fired && held_us >= (int64_t)BTN_COMBO_LOCK_MS * 1000) {
                combo_lock_fired = true;
                both_short_armed = false;
                combo_lock_event = true;
                btn_mode_locked = !btn_mode_locked;
                oled_ui.mode_locked = btn_mode_locked;
                printf("LOCK %s\r\n", btn_mode_locked ? "ON" : "OFF");
                fflush(stdout);
                redraw_current_oled();
            }
            if (!combo_clear_fired && held_us >= (int64_t)BTN_COMBO_CLEAR_MS * 1000) {
                combo_clear_fired = true;
                both_short_armed = false;
                combo_clear_event = true;
                mosfet_clr_on = !mosfet_clr_on;
                board_output_write(MOSFET_CLR_PIN, mosfet_clr_on);
                printf("CLR %s\r\n", mosfet_clr_on ? "ON" : "OFF");
                fflush(stdout);
            }
        }
        if (!both_pressed && both_pressed_prev) {
            if (both_short_armed) {
                combo_short_event = true;
            }
            both_short_armed = false;
            combo_nav_fired = false;
            combo_lock_fired = false;
            combo_clear_fired = false;
        }
        both_pressed_latch = both_pressed;
        bool combo_active = both_pressed;
        if (prev_pressed || next_pressed || prev_repeat || next_repeat ||
            combo_short_event || combo_nav_event || combo_lock_event || combo_clear_event) {
            oled_last_activity = get_absolute_time();
            if (oled_ok && oled_dimmed) {
                oled_ok = oled_set_contrast(OLED_CONTRAST_FULL);
                if (oled_ok) {
                    oled_dimmed = false;
                }
            }
            if (oled_ok && !oled_panel_on) {
                oled_ok = oled_set_power(true);
                if (oled_ok) {
                    oled_panel_on = true;
                    oled_ok = oled_set_contrast(OLED_CONTRAST_FULL);
                    if (oled_ok) {
                        oled_dimmed = false;
                    }
                    if (ui_mode == UiMode::Menu) {
                        menu_render_baud();
                    } else {
                        redraw_current_oled();
                    }
                }
            }
        }

        if (ui_mode == UiMode::Normal) {
            if (combo_short_event) {
                if (btn_mode_locked) {
                    printf("LOCKED\r\n");
                    fflush(stdout);
                } else if (data_mode == DataSourceMode::I2cPoll) {
                    switch_to_uart_mode();
                    redraw_current_oled();
                } else {
                    switch_to_i2c_mode();
                    redraw_current_oled();
                }
            }
        }

        if (ui_mode == UiMode::Menu) {
            bool changed = false;
            if (prev_pressed || prev_repeat) {
                menu_baud_idx_edit = (menu_baud_idx_edit == 0) ? (UART_BAUD_OPTIONS_COUNT - 1) : (menu_baud_idx_edit - 1);
                changed = true;
            }
            if (next_pressed || next_repeat) {
                menu_baud_idx_edit = (menu_baud_idx_edit + 1) % UART_BAUD_OPTIONS_COUNT;
                changed = true;
            }
            if (changed) {
                menu_render_baud();
            }
        }

        bool page_scrolled = false;
        if (page_click_sequence_active && time_reached(page_click_deadline)) {
            prev_page_click_count = 0U;
            next_page_click_count = 0U;
            page_click_sequence_active = false;
            oled_ui.nav_tag =
                button_role_mode == ButtonRoleMode::ScrollBrowse ? 'S' : 'G';
        }
        if (ui_mode == UiMode::Normal &&
            data_mode == DataSourceMode::I2cPoll &&
            button_role_mode == ButtonRoleMode::ScrollBrowse &&
            !combo_active && (prev_pressed || next_pressed)) {
            page_click_sequence_active = true;
            page_click_deadline = make_timeout_time_ms(BTN_PAGE_CLICK_GAP_MS);
            if (prev_pressed) {
                next_page_click_count = 0U;
                if (prev_page_click_count < BTN_PAGE_CLICK_COUNT) {
                    ++prev_page_click_count;
                }
            } else {
                prev_page_click_count = 0U;
                if (next_page_click_count < BTN_PAGE_CLICK_COUNT) {
                    ++next_page_click_count;
                }
            }

            bool previous = prev_page_click_count >= BTN_PAGE_CLICK_COUNT;
            bool next = next_page_click_count >= BTN_PAGE_CLICK_COUNT;
            if (previous || next) {
                DisplayPage candidate = display_page;
                for (size_t attempt = 0U; attempt < 4U; ++attempt) {
                    candidate = adjacent_display_page(candidate, previous);
                    if (candidate == DisplayPage::Post) {
                        display_page = DisplayPage::Post;
                        sensor_refresh_pending = false;
                        sensor_refresh_marks_activity = false;
                        browse_mode = false;
                        page_scrolled = true;
                        break;
                    }
                    size_t first_sensor = 0U;
                    if (first_sensor_for_page(candidate, &first_sensor)) {
                        display_page = candidate;
                        sensor_index = first_sensor;
                        sensor_refresh_index = first_sensor;
                        sensor_page_deadline = make_timeout_time_ms(
                            SENSOR_PAGE_INTERVAL_MS);
                        sensor_retry_deadline = get_absolute_time();
                        sensor_refresh_pending = true;
                        sensor_wait_rendered = false;
                        sensor_refresh_marks_activity = true;
                        post_override_active = false;
                        browse_mode = false;
                        page_scrolled = true;
                        break;
                    }
                }
                prev_page_click_count = 0U;
                next_page_click_count = 0U;
                page_click_sequence_active = false;
                oled_ui.nav_tag = 'S';
                if (page_scrolled) {
                    redraw_current_oled();
                }
            }
        }

        if (ui_mode == UiMode::Normal && button_role_mode == ButtonRoleMode::GpioControl) {
            browse_mode = false;
            bool prev_only_pressed = !combo_active && btn_prev.stable_pressed && !btn_next.stable_pressed;
            bool next_only_pressed = !combo_active && btn_next.stable_pressed && !btn_prev.stable_pressed;

            if (prev_only_pressed) {
                int64_t held_us = absolute_time_diff_us(btn_prev.press_start_time, now);
                if (!prev_single_fired && held_us >= (int64_t)BTN_SINGLE_TOGGLE_MS * 1000) {
                    prev_single_fired = true;
                    mosfet_pwr_on = !mosfet_pwr_on;
                    board_output_write(MOSFET_PWR_PIN, mosfet_pwr_on);
                    printf("PWR %s\r\n", mosfet_pwr_on ? "ON" : "OFF");
                    fflush(stdout);
                }
            } else {
                prev_single_fired = false;
            }

            if (next_only_pressed) {
                int64_t held_us = absolute_time_diff_us(btn_next.press_start_time, now);
                if (!next_single_fired && held_us >= (int64_t)BTN_SINGLE_TOGGLE_MS * 1000) {
                    next_single_fired = true;
                    mosfet_rst_on = !mosfet_rst_on;
                    board_output_write(MOSFET_RST_PIN, mosfet_rst_on);
                    printf("RST %s\r\n", mosfet_rst_on ? "ON" : "OFF");
                    fflush(stdout);
                }
            } else {
                next_single_fired = false;
            }
        } else if (ui_mode == UiMode::Normal) {
            prev_single_fired = false;
            next_single_fired = false;
            if (display_page != DisplayPage::Post) {
                browse_mode = false;
            } else if (!page_scrolled && !combo_active &&
                       (prev_pressed || next_pressed || prev_repeat || next_repeat) &&
                       code_buf.count > 0) {
                uint32_t oldest = code_buffer_oldest_abs(&code_buf);
                uint32_t newest = code_buffer_newest_abs(&code_buf);

                if (!browse_mode) {
                    browse_mode = true;
                    browse_cursor_abs = newest;
                    browse_enter_total_pushed = code_buf.total_pushed;
                }
                if (browse_cursor_abs < oldest) {
                    browse_cursor_abs = oldest;
                }
                if (browse_cursor_abs > newest) {
                    browse_cursor_abs = newest;
                }

                if ((prev_pressed || prev_repeat) && browse_cursor_abs > oldest) {
                    --browse_cursor_abs;
                }
                if ((next_pressed || next_repeat) && browse_cursor_abs < newest) {
                    ++browse_cursor_abs;
                }

                if (oled_ok) {
                    PostCode show = {};
                    if (code_buffer_get_abs(&code_buf, browse_cursor_abs, &show)) {
                        oled_ok = oled_render_code(
                            oled_ok, &oled_panel_on, &oled_last_activity, show, true, oldest, newest, browse_cursor_abs);
                        current_oled_code = show;
                    }
                }
                browse_deadline = make_timeout_time_ms(BTN_BROWSE_TIMEOUT_MS);
            }
        } else {
            prev_single_fired = false;
            next_single_fired = false;
        }

        if (ui_mode == UiMode::Normal && browse_mode && time_reached(browse_deadline)) {
            browse_mode = false;
            if (code_buf.count > 0) {
                uint32_t newest = code_buffer_newest_abs(&code_buf);
                PostCode latest = {};
                bool have_latest = code_buffer_get_abs(&code_buf, newest, &latest);
                bool added_during_browse = code_buf.total_pushed > browse_enter_total_pushed;

                if (added_during_browse) {
                    // Drop pre-browse backlog, but keep anything captured during browse.
                    if (code_buf.pending_abs < browse_enter_total_pushed) {
                        code_buf.pending_abs = browse_enter_total_pushed;
                    }
                    // If that queue is already consumed, still snap to live latest now.
                    if (code_buf.pending_abs >= code_buf.total_pushed && oled_ok && have_latest) {
                        oled_ok = oled_render_code(
                            oled_ok, &oled_panel_on, &oled_last_activity, latest, false, 0, 0, 0);
                        current_oled_code = latest;
                    }
                } else {
                    // No new data while browsing: hard-jump straight to current latest.
                    if (oled_ok && have_latest) {
                        oled_ok = oled_render_code(
                            oled_ok, &oled_panel_on, &oled_last_activity, latest, false, 0, 0, 0);
                        current_oled_code = latest;
                    }
                    code_buffer_clear_pending(&code_buf);
                    last_enqueued_code = latest;
                    last_enqueued_valid = true;
                }
            }
            next_oled_update = make_timeout_time_ms(OLED_CODE_HOLD_MS);
        }

        // Live OLED updates are paced to a human-readable hold time.
        if (ui_mode == UiMode::Normal &&
            display_page == DisplayPage::Post &&
            !browse_mode && oled_ok && time_reached(next_oled_update)) {
            PostCode queued_code = {};
            if (code_buffer_pending_pop(&code_buf, &queued_code)) {
                oled_ok = oled_render_code(oled_ok, &oled_panel_on, &oled_last_activity, queued_code, false, 0, 0, 0);
                current_oled_code = queued_code;
            }
            next_oled_update = make_timeout_time_ms(OLED_CODE_HOLD_MS);
        }

        // Hide static mode/role chrome after configurable inactivity.
        if (ui_mode == UiMode::Normal && oled_ok &&
            oled_ui.top_bar_visible &&
            DISPLAY_TOP_BAR_TIMEOUT_MS != 0U &&
            absolute_time_diff_us(oled_last_activity, get_absolute_time()) >=
                (int64_t)DISPLAY_TOP_BAR_TIMEOUT_MS * 1000) {
            oled_ui.top_bar_visible = false;
            if (oled_panel_on) {
                redraw_current_oled(false);
            }
        }

        // Burn-in mitigation: periodic pixel shift redraw while panel is on.
        if (DISPLAY_PIXEL_SHIFT_PIXELS > 0U && oled_ok &&
            time_reached(next_shift_update)) {
            shift_idx = (shift_idx + 1) % SHIFT_SEQ_LEN;
            oled_set_pixel_shift(SHIFT_X_SEQ[shift_idx],
                                 SHIFT_Y_SEQ[shift_idx]);
            next_shift_update = make_timeout_time_ms(OLED_SHIFT_INTERVAL_MS);

            if (oled_panel_on) {
                redraw_current_oled(false);
            }
        }

        // Configurable OLED dim/off protection.
        if (oled_ok && oled_panel_on) {
            int64_t idle_us = absolute_time_diff_us(oled_last_activity, get_absolute_time());
            if (OLED_DIM_TIMEOUT_MS != 0U && oled_dimmed &&
                idle_us < (int64_t)OLED_DIM_TIMEOUT_MS * 1000) {
                oled_ok = oled_set_contrast(OLED_CONTRAST_FULL);
                if (oled_ok) {
                    oled_dimmed = false;
                }
            }
            if (OLED_DIM_TIMEOUT_MS != 0U && !oled_dimmed &&
                idle_us >= (int64_t)OLED_DIM_TIMEOUT_MS * 1000) {
                oled_ok = oled_set_contrast(OLED_CONTRAST_DIM);
                if (oled_ok) {
                    oled_dimmed = true;
                }
            }
            if (OLED_SLEEP_TIMEOUT_MS != 0U &&
                idle_us >= (int64_t)OLED_SLEEP_TIMEOUT_MS * 1000) {
                if (oled_dimmed) {
                    oled_ok = oled_set_contrast(OLED_CONTRAST_FULL);
                    if (oled_ok) oled_dimmed = false;
                }
                if (oled_ok) oled_ok = oled_set_power(false);
                if (oled_ok) {
                    oled_panel_on = false;
                    oled_dimmed = false;
                }
            }
        }

        sleep_until(next_poll);
    }
}
