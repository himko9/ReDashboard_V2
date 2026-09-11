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

#ifndef REDASHBOARD_RP2040_BOARD_CONFIG_H
#define REDASHBOARD_RP2040_BOARD_CONFIG_H

#include <stdint.h>

#include "hardware/i2c.h"
#include "hardware/uart.h"

#include "app_config.h"
#include "display_config.h"

/*
 * RP2040 board configuration
 *
 * Keep PCB routing, polarity, buses and RP2040-only timing choices here.
 * User-facing display and SIO sensor settings live beside this file in
 * rp2040/include. The RP2040 and PY32 trees intentionally keep independent
 * copies so either firmware can be built and configured on its own.
 */

struct I2CBusConfig {
    i2c_inst_t *inst;
    uint sda_pin;
    uint scl_pin;
    uint32_t baud_hz;
    uint timeout_ms;
};

inline constexpr I2CBusConfig POST_BUS = {i2c1, 22U, 23U, 100000U, 20U};
inline constexpr I2CBusConfig OLED_BUS = {i2c0, 0U, 1U, 400000U, 30U};

/* Monitor-page accesses use the normal 100-kHz SIO bus, with a longer timeout
 * and gaps between stages to accommodate clock stretching. */
inline constexpr uint32_t SIO_MONITOR_I2C_BAUD_HZ = POST_BUS.baud_hz;
inline constexpr uint SIO_MONITOR_I2C_TIMEOUT_MS = 100U;
inline constexpr uint SIO_MONITOR_STAGE_GAP_US = 50U;

inline constexpr uint8_t SIO_I2C_ADDRESS = 0x2DU;
inline constexpr uint8_t OLED_I2C_ADDRESS = 0x3CU;

inline uart_inst_t *const UART_IN_INST = uart1;
inline constexpr uint UART_IN_RX_PIN = 21U;
inline constexpr uint32_t UART_IN_BAUD = 115200U;
/* Ignore otherwise plausible 115200 bytes briefly after a framing/noise error.
 * This prevents the parallel 115200 receiver from decoding the custom
 * 1.5-Mbaud wire format as isolated 00/01 bytes. */
inline constexpr uint UART_CROSS_BAUD_ERROR_GUARD_MS = 50U;

/* Custom 1.5 Mbaud POST stream: port + width + variable payload. */
inline constexpr uint RP2040_FAST_UART_RX_PIN = 20U;
inline constexpr uint32_t RP2040_FAST_UART_BAUD = 1500000U;

inline constexpr uint BTN_PREV_PIN = 10U;  /* physical PWR button */
inline constexpr uint BTN_NEXT_PIN = 28U;  /* physical RST button */

inline constexpr uint MOSFET_PWR_PIN = 17U;
inline constexpr uint MOSFET_RST_PIN = 18U;
inline constexpr uint MOSFET_CLR_PIN = 16U;
inline constexpr bool MOSFET_ACTIVE_LOW = true;

enum class ButtonInputMode : uint8_t {
    DigitalActiveLow,
    DigitalActiveHigh,
    CapacitiveGpio,
};

inline constexpr ButtonInputMode BTN_INPUT_MODE =
    ButtonInputMode::DigitalActiveLow;

inline constexpr uint TOUCH_SAMPLES_PER_READ = 8U;
inline constexpr uint TOUCH_DISCHARGE_US = 3U;
inline constexpr uint16_t TOUCH_MAX_COUNT = 1500U;
inline constexpr uint16_t TOUCH_DELTA_ON = 10U;
inline constexpr uint16_t TOUCH_DELTA_OFF = 5U;
inline constexpr uint16_t TOUCH_DELTA_ON_GPIO_MODE = 18U;
inline constexpr uint16_t TOUCH_DELTA_OFF_GPIO_MODE = 10U;
inline constexpr uint TOUCH_BASELINE_SHIFT = 5U;
inline constexpr uint BTN_DEBOUNCE_MS = 35U;
inline constexpr uint BTN_LONGPRESS_START_MS = 450U;
inline constexpr uint BTN_LONGPRESS_REPEAT_MS = 140U;

inline constexpr uint BTN_SINGLE_TOGGLE_MS = BUTTON_SINGLE_OUTPUT_MS;
inline constexpr uint BTN_COMBO_NAV_MS = BUTTON_COMBO_ROLE_MS;
inline constexpr uint BTN_COMBO_LOCK_MS = BUTTON_COMBO_LOCK_MS;
inline constexpr uint BTN_COMBO_CLEAR_MS = BUTTON_COMBO_CLEAR_MS;
inline constexpr uint BTN_BROWSE_TIMEOUT_MS = BUTTON_BROWSE_TIMEOUT_MS;
inline constexpr uint BTN_PAGE_CLICK_GAP_MS = BUTTON_PAGE_CLICK_GAP_MS;
inline constexpr uint BTN_PAGE_CLICK_COUNT = BUTTON_PAGE_CLICK_COUNT;

inline constexpr uint POST_POLL_INTERVAL_US = POST_POLL_INTERVAL_MS * 1000U;
inline constexpr uint OLED_CODE_HOLD_MS = DISPLAY_CODE_HOLD_MS;
inline constexpr uint OLED_DIM_TIMEOUT_MS = DISPLAY_DIM_TIMEOUT_MS;
inline constexpr uint OLED_SLEEP_TIMEOUT_MS = DISPLAY_OFF_TIMEOUT_MS;
inline constexpr uint OLED_SHIFT_INTERVAL_MS = DISPLAY_PIXEL_SHIFT_INTERVAL_MS;
inline constexpr uint8_t OLED_CONTRAST_FULL = DISPLAY_CONTRAST_FULL;
inline constexpr uint8_t OLED_CONTRAST_DIM = DISPLAY_CONTRAST_DIM;

/* Supported values: 0 or 180 degrees. */
#define RP2040_OLED_ROTATION 0U

#endif
