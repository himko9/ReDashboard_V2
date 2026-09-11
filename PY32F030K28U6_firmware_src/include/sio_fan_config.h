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

#ifndef EZDASH_SIO_FAN_CONFIG_H
#define EZDASH_SIO_FAN_CONFIG_H

#include <stdint.h>

/*
 * NCT6687D tachometer configuration.
 *
 * ENABLED:   1 reads and displays the tachometer; 0 omits it completely.
 * INDEX:     high byte of the big-endian 16-bit RPM value.
 * LABEL:     OLED text, at most 21 characters.
 * CALCULATE: converts the raw word to RPM; NCT6687D reports RPM directly.
 *
 */
#define SIO_TACH0_ENABLED        1U
#define SIO_TACH0_INDEX          0x40U
#define SIO_TACH0                "CPU FAN"
#define SIO_TACH0_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH1_ENABLED        0U
#define SIO_TACH1_INDEX          0x42U
#define SIO_TACH1                "TACH1"
#define SIO_TACH1_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH2_ENABLED        0U
#define SIO_TACH2_INDEX          0x44U
#define SIO_TACH2                "TACH2"
#define SIO_TACH2_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH3_ENABLED        1U
#define SIO_TACH3_INDEX          0x46U
#define SIO_TACH3                "TACH3"
#define SIO_TACH3_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH4_ENABLED        0U
#define SIO_TACH4_INDEX          0x48U
#define SIO_TACH4                "TACH4"
#define SIO_TACH4_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH5_ENABLED        0U
#define SIO_TACH5_INDEX          0x4AU
#define SIO_TACH5                "TACH5"
#define SIO_TACH5_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH6_ENABLED        0U
#define SIO_TACH6_INDEX          0x4CU
#define SIO_TACH6                "TACH6"
#define SIO_TACH6_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH7_ENABLED        0U
#define SIO_TACH7_INDEX          0x4EU
#define SIO_TACH7                "TACH7"
#define SIO_TACH7_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH8_ENABLED        0U
#define SIO_TACH8_INDEX          0x50U
#define SIO_TACH8                "TACH8"
#define SIO_TACH8_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH9_ENABLED        0U
#define SIO_TACH9_INDEX          0x52U
#define SIO_TACH9                "TACH9"
#define SIO_TACH9_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH10_ENABLED        0U
#define SIO_TACH10_INDEX          0x54U
#define SIO_TACH10                "TACH10"
#define SIO_TACH10_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH11_ENABLED        0U
#define SIO_TACH11_INDEX          0x56U
#define SIO_TACH11                "TACH11"
#define SIO_TACH11_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH12_ENABLED        0U
#define SIO_TACH12_INDEX          0x58U
#define SIO_TACH12                "TACH12"
#define SIO_TACH12_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH13_ENABLED        0U
#define SIO_TACH13_INDEX          0x5AU
#define SIO_TACH13                "TACH13"
#define SIO_TACH13_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH14_ENABLED        0U
#define SIO_TACH14_INDEX          0x5CU
#define SIO_TACH14                "TACH14"
#define SIO_TACH14_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

#define SIO_TACH15_ENABLED        0U
#define SIO_TACH15_INDEX          0x5EU
#define SIO_TACH15                "TACH15"
#define SIO_TACH15_CALCULATE(raw) ((int32_t)(uint16_t)(raw))

/* Add or reorder entries here to change the fan carousel order. */
#define SIO_TACH_TABLE(X)                                                \
    X(TACH0, SIO_TACH0_ENABLED, SENSOR_TYPE_TACH, SIO_TACH0_INDEX,        \
      SIO_TACH0, SIO_TACH0_CALCULATE)                                   \
    X(TACH1, SIO_TACH1_ENABLED, SENSOR_TYPE_TACH, SIO_TACH1_INDEX,        \
      SIO_TACH1, SIO_TACH1_CALCULATE)                                   \
    X(TACH2, SIO_TACH2_ENABLED, SENSOR_TYPE_TACH, SIO_TACH2_INDEX,        \
      SIO_TACH2, SIO_TACH2_CALCULATE)                                   \
    X(TACH3, SIO_TACH3_ENABLED, SENSOR_TYPE_TACH, SIO_TACH3_INDEX,        \
      SIO_TACH3, SIO_TACH3_CALCULATE)                                   \
    X(TACH4, SIO_TACH4_ENABLED, SENSOR_TYPE_TACH, SIO_TACH4_INDEX,        \
      SIO_TACH4, SIO_TACH4_CALCULATE)                                   \
    X(TACH5, SIO_TACH5_ENABLED, SENSOR_TYPE_TACH, SIO_TACH5_INDEX,        \
      SIO_TACH5, SIO_TACH5_CALCULATE)                                   \
    X(TACH6, SIO_TACH6_ENABLED, SENSOR_TYPE_TACH, SIO_TACH6_INDEX,        \
      SIO_TACH6, SIO_TACH6_CALCULATE)                                   \
    X(TACH7, SIO_TACH7_ENABLED, SENSOR_TYPE_TACH, SIO_TACH7_INDEX,        \
      SIO_TACH7, SIO_TACH7_CALCULATE)                                   \
    X(TACH8, SIO_TACH8_ENABLED, SENSOR_TYPE_TACH, SIO_TACH8_INDEX,        \
      SIO_TACH8, SIO_TACH8_CALCULATE)                                   \
    X(TACH9, SIO_TACH9_ENABLED, SENSOR_TYPE_TACH, SIO_TACH9_INDEX,        \
      SIO_TACH9, SIO_TACH9_CALCULATE)                                   \
    X(TACH10, SIO_TACH10_ENABLED, SENSOR_TYPE_TACH, SIO_TACH10_INDEX,     \
      SIO_TACH10, SIO_TACH10_CALCULATE)                                 \
    X(TACH11, SIO_TACH11_ENABLED, SENSOR_TYPE_TACH, SIO_TACH11_INDEX,     \
      SIO_TACH11, SIO_TACH11_CALCULATE)                                 \
    X(TACH12, SIO_TACH12_ENABLED, SENSOR_TYPE_TACH, SIO_TACH12_INDEX,     \
      SIO_TACH12, SIO_TACH12_CALCULATE)                                 \
    X(TACH13, SIO_TACH13_ENABLED, SENSOR_TYPE_TACH, SIO_TACH13_INDEX,     \
      SIO_TACH13, SIO_TACH13_CALCULATE)                                 \
    X(TACH14, SIO_TACH14_ENABLED, SENSOR_TYPE_TACH, SIO_TACH14_INDEX,     \
      SIO_TACH14, SIO_TACH14_CALCULATE)                                 \
    X(TACH15, SIO_TACH15_ENABLED, SENSOR_TYPE_TACH, SIO_TACH15_INDEX,     \
      SIO_TACH15, SIO_TACH15_CALCULATE)

#endif
