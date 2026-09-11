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

#ifndef EZDASH_DISPLAY_CONFIG_H
#define EZDASH_DISPLAY_CONFIG_H

/* 
* Set a sensor page to 0 to remove it from startup and page scrolling.
*
* The pages listed below are only available when the "JDASH1" connector is connected,
* If "JDASH1" is not connected, only the UART based POST code page is available.
*/
#define DISPLAY_TEMPERATURE_PAGE_ENABLED   1U
#define DISPLAY_VOLTAGE_PAGE_ENABLED       1U
#define DISPLAY_FAN_PAGE_ENABLED           1U

#if (DISPLAY_TEMPERATURE_PAGE_ENABLED != 0U) && \
    (DISPLAY_TEMPERATURE_PAGE_ENABLED != 1U)
#error "DISPLAY_TEMPERATURE_PAGE_ENABLED must be 0 or 1"
#endif

#if (DISPLAY_VOLTAGE_PAGE_ENABLED != 0U) && \
    (DISPLAY_VOLTAGE_PAGE_ENABLED != 1U)
#error "DISPLAY_VOLTAGE_PAGE_ENABLED must be 0 or 1"
#endif

#if (DISPLAY_FAN_PAGE_ENABLED != 0U) && \
    (DISPLAY_FAN_PAGE_ENABLED != 1U)
#error "DISPLAY_FAN_PAGE_ENABLED must be 0 or 1"
#endif

/* 
* Screen shown at power-up. Sensor screens are JDASH only. 
*/
#define BOOT_SCREEN_POST         0U
#define BOOT_SCREEN_VOLTAGE      1U
#define BOOT_SCREEN_TEMPERATURE  2U
#define BOOT_SCREEN_FAN          3U
#define DEFAULT_BOOT_SCREEN      BOOT_SCREEN_TEMPERATURE

#if (DEFAULT_BOOT_SCREEN != BOOT_SCREEN_POST) && \
    (DEFAULT_BOOT_SCREEN != BOOT_SCREEN_VOLTAGE) && \
    (DEFAULT_BOOT_SCREEN != BOOT_SCREEN_TEMPERATURE) && \
    (DEFAULT_BOOT_SCREEN != BOOT_SCREEN_FAN)
#error "DEFAULT_BOOT_SCREEN must be POST, VOLTAGE, TEMPERATURE, or FAN"
#endif

/* 
* After new POST activity, return to DEFAULT_BOOT_SCREEN after this delay.
* Set to 0 to remain on the POST screen. 
*/
#define DISPLAY_POST_RETURN_DELAY_SECONDS 10U

/* 
* Decoding range configuration:
* Set to 0 to ignore P81 (or P80 High byte) and always display only the two-digit P80(Low byte) value.
*
* AMD uses 8-bit P80 and 8-bit P81, with P81 primarily used in ABL/AGESA.
* Intel uses 16-bit P80 in FSP, and P81 remains "0x00" in most cases, but this depends on the platform.
*
* In most cases, AMI UEFI only uses 8-bit P80 for its PEI/DXE checkpoints.
*
* Note: 
* The SIO appears to only buffer 8-bit P80/P81 data, therefore 16-bit P80 cannot be obtained from "JDASH1",
* and only the "JDP1" will decode 32-bits data, address range P80 - P83.
* When the address of the high byte buffer is known, the codebase will be updated.
*/
#ifndef POSTCODE_P81_DECODE_ENABLED
#define POSTCODE_P81_DECODE_ENABLED        0U
#endif

#if (POSTCODE_P81_DECODE_ENABLED != 0U) && \
    (POSTCODE_P81_DECODE_ENABLED != 1U)
#error "POSTCODE_P81_DECODE_ENABLED must be 0 or 1"
#endif

/*  
* OLED Display pixel shifting / dimming / power down configuration:
* Enable pixel shifting to reduce burn-in on OLED displays. 
* The display will shift the entire framebuffer by DISPLAY_PIXEL_SHIFT_PIXELS, 
* in a square pattern by every DISPLAY_PIXEL_SHIFT_INTERVAL_SECONDS. 
*/

// Set either timeout to 0 to disable that power-saving stage.
#define DISPLAY_DIM_TIMEOUT_SECONDS       (2U * 60U)
#define DISPLAY_OFF_TIMEOUT_SECONDS       (10U * 60U)

/* Hide the mode/role/lock top bar after this much inactivity to reduce
 * burn-in. Set to 0 to keep the top bar visible. */
#define DISPLAY_TOP_BAR_TIMEOUT_SECONDS   30U

// Set the distance to 0 to disable pixel shifting completely.
#define DISPLAY_PIXEL_SHIFT_PIXELS        1U
#define DISPLAY_PIXEL_SHIFT_INTERVAL_SECONDS 10U


#if DISPLAY_PIXEL_SHIFT_PIXELS > 127U
#error "DISPLAY_PIXEL_SHIFT_PIXELS must be between 0 and 127"
#endif

#if (DISPLAY_PIXEL_SHIFT_PIXELS > 0U) && \
    (DISPLAY_PIXEL_SHIFT_INTERVAL_SECONDS == 0U)
#error "Pixel-shift interval must be nonzero when pixel shifting is enabled"
#endif

#define DISPLAY_DIM_TIMEOUT_MS \
    (DISPLAY_DIM_TIMEOUT_SECONDS * 1000U)
#define DISPLAY_OFF_TIMEOUT_MS \
    (DISPLAY_OFF_TIMEOUT_SECONDS * 1000U)
#define DISPLAY_TOP_BAR_TIMEOUT_MS \
    (DISPLAY_TOP_BAR_TIMEOUT_SECONDS * 1000U)
#define DISPLAY_PIXEL_SHIFT_INTERVAL_MS \
    (DISPLAY_PIXEL_SHIFT_INTERVAL_SECONDS * 1000U)
#define DISPLAY_POST_RETURN_DELAY_MS \
    (DISPLAY_POST_RETURN_DELAY_SECONDS * 1000U)

#endif
