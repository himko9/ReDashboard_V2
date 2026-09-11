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

#ifndef REDASHBOARD_APP_CONFIG_H
#define REDASHBOARD_APP_CONFIG_H

/* POST polling and SIO communication settings. */
#define POST_POLL_INTERVAL_MS       5U
/* Lightweight P80-only probe interval while UART owns the display. */
#define JDASH_DETECT_POLL_INTERVAL_MS 20U
#define SIO_ERROR_CONFIRM_MS        500U
/* Back off failed polls so I2C recovery cannot starve the software-I2C OLED.
 * A successful SIO poll immediately restores POST_POLL_INTERVAL_MS. */
#define SIO_ERROR_RETRY_INTERVAL_MS 250U
#define PORT81_ZERO_HIDE_POLLS      100U
#define SIO_MONITOR_PAGE            0x01U

/* OLED update, initialization, recovery, and contrast settings. */
#define DISPLAY_CODE_HOLD_MS        100U
#define DISPLAY_POWER_UP_DELAY_MS   1000U
#define DISPLAY_RETRY_INTERVAL_MS   1000U
#define DISPLAY_CONTRAST_FULL       0xFFU
#define DISPLAY_CONTRAST_DIM        0x1FU

/* Button timing and five-click page navigation. */
#define BUTTON_SINGLE_OUTPUT_MS     1000U
#define BUTTON_COMBO_ROLE_MS        1000U
#define BUTTON_COMBO_LOCK_MS        3000U
#define BUTTON_COMBO_CLEAR_MS       5000U
#define BUTTON_BROWSE_TIMEOUT_MS    7000U
#define BUTTON_PAGE_CLICK_GAP_MS    1000U
#define BUTTON_PAGE_CLICK_COUNT     5U

/* Sensor carousel and monitor-register retry timing. */
#define SENSOR_PAGE_INTERVAL_MS     3000U
#define SENSOR_READ_RETRY_MS        100U
/* Wake a dimmed/off OLED when a valid sensor changes by more than this
 * percentage. Set to 0 to disable sensor-change wakeups. */
#define SENSOR_WAKE_CHANGE_PERCENT  10U

/* Do not modify the following lines*/
#if POST_POLL_INTERVAL_MS == 0U
#error "POST_POLL_INTERVAL_MS must be nonzero"
#endif

#if JDASH_DETECT_POLL_INTERVAL_MS == 0U
#error "JDASH_DETECT_POLL_INTERVAL_MS must be nonzero"
#endif

#if SIO_ERROR_RETRY_INTERVAL_MS == 0U
#error "SIO_ERROR_RETRY_INTERVAL_MS must be nonzero"
#endif

#if DISPLAY_RETRY_INTERVAL_MS == 0U
#error "DISPLAY_RETRY_INTERVAL_MS must be nonzero"
#endif

#if BUTTON_PAGE_CLICK_COUNT == 0U
#error "BUTTON_PAGE_CLICK_COUNT must be nonzero"
#endif

#if SENSOR_PAGE_INTERVAL_MS == 0U
#error "SENSOR_PAGE_INTERVAL_MS must be nonzero"
#endif

#if SENSOR_WAKE_CHANGE_PERCENT > 1000U
#error "SENSOR_WAKE_CHANGE_PERCENT must be between 0 and 1000"
#endif

#if BUTTON_COMBO_ROLE_MS > BUTTON_COMBO_LOCK_MS || \
    BUTTON_COMBO_LOCK_MS > BUTTON_COMBO_CLEAR_MS
#error "Button combo thresholds must be in role, lock, clear order"
#endif

#endif
