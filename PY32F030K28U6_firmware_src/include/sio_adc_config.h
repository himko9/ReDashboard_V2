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

#ifndef EZDASH_SIO_ADC_CONFIG_H
#define EZDASH_SIO_ADC_CONFIG_H

#include <stdint.h>

/* Voltage digits after the decimal point: supported range is 0 through 3. */
#define SENSOR_VOLTAGE_DECIMAL_DIGITS 2U

#if SENSOR_VOLTAGE_DECIMAL_DIGITS > 3U
#error "SENSOR_VOLTAGE_DECIMAL_DIGITS must be between 0 and 3"
#endif

/* Temperature display unit: use 'C' for degrees Celsius or 'F' for Fahrenheit. */
#define SENSOR_TEMP_UNIT 'C'

#if (SENSOR_TEMP_UNIT != 'C') && (SENSOR_TEMP_UNIT != 'F')
#error "SENSOR_TEMP_UNIT must be 'C' or 'F'"
#endif

/*
 * Per-sensor configuration.
 *
 * ENABLED:   1 reads and displays the sensor; 0 omits it completely.
 * TYPE:      VIN reads one byte and appears on the voltage page.
 *            THR reads index/index+1 and appears on the temperature page.
 *            TACH reads index/index+1 and appears on the fan page.
 * INDEX:     first Page-01 data register for this monitor slot.
 * LABEL:     OLED text, at most 21 characters.
 * CALCULATE: VIN formulas return millivolts; THR formulas return 0.1 C;
 *            TACH formulas return RPM.
 *
 * A muxed analog pin such as THR6/VIN6 is one physical ADC. Change its use by
 * editing TYPE, INDEX, LABEL, and CALCULATE for that SIO_ADCn entry.
 */
#define SENSOR_TYPE_VIN 0U
#define SENSOR_TYPE_THR 1U
#define SENSOR_TYPE_TACH 2U

/* Default Nuvoton temperature word (signed 8.8 C), rounded to 0.5 C. */
#define SENSOR_THR_RAW_TO_C10(raw)                                      \
    ((int32_t)(((int32_t)(int16_t)(raw) < 0)                            \
                   ? -((int32_t)(((-(int32_t)(int16_t)(raw)) + 64) /   \
                                  128) *                                \
                       5)                                               \
                   : ((int32_t)(((int32_t)(int16_t)(raw) + 64) / 128) * \
                      5)))

/* NCT6686D pin 105: VIN0 / THR0 / V_COMP0. */
#define SIO_ADC0_ENABLED       1U
#define SIO_ADC0_TYPE          SENSOR_TYPE_VIN
#define SIO_ADC0_INDEX         0x20U
#define SIO_ADC0               "SYSTEM 12V"
#define SIO_ADC0_CALCULATE(raw) \
    ((int32_t)((((uint32_t)(raw) * 12264U) + 31U) / 63U))

/* NCT6686D pin 114: VIN1 / THR1 / V_COMP1. */
#define SIO_ADC1_ENABLED       1U
#define SIO_ADC1_TYPE          SENSOR_TYPE_VIN
#define SIO_ADC1_INDEX         0x22U
#define SIO_ADC1               "SYSTEM 5V"
#define SIO_ADC1_CALCULATE(raw) \
    ((int32_t)((uint32_t)(raw) * 80U))

/* NCT6686D pin 115: VIN2 / THR2 / V_COMP2. Currently unused. */
#define SIO_ADC2_ENABLED       0U
#define SIO_ADC2_TYPE          SENSOR_TYPE_VIN
#define SIO_ADC2_INDEX         0x24U
#define SIO_ADC2               "ADC2"
#define SIO_ADC2_CALCULATE(raw) \
    ((int32_t)((uint32_t)(raw) * 16U))

/* NCT6686D pin 116: VIN3 / THR3 / V_COMP3 / ATX5VSB. Unused. */
#define SIO_ADC3_ENABLED       0U
#define SIO_ADC3_TYPE          SENSOR_TYPE_VIN
#define SIO_ADC3_INDEX         0x26U
#define SIO_ADC3               "ADC3"
#define SIO_ADC3_CALCULATE(raw) \
    ((int32_t)((uint32_t)(raw) * 16U))

/* NCT6686D pin 109: VIN5 / THR5. */
#define SIO_ADC5_ENABLED       1U
#define SIO_ADC5_TYPE          SENSOR_TYPE_VIN
#define SIO_ADC5_INDEX         0x28U
#define SIO_ADC5               "VDIMM"
#define SIO_ADC5_CALCULATE(raw) \
    ((int32_t)((((uint32_t)(raw) * 1204U) + 18U) / 37U))

/* NCT6686D pin 106: VIN6 / THR6. */
#define SIO_ADC6_ENABLED       1U
#define SIO_ADC6_TYPE          SENSOR_TYPE_VIN
#define SIO_ADC6_INDEX         0x2AU
#define SIO_ADC6               "VCORE SENSE"
#define SIO_ADC6_CALCULATE(raw) \
    ((int32_t)((uint32_t)(raw) * 16U))

/* NCT6686D pin 107: VIN7 / THR7. */
#define SIO_ADC7_ENABLED       1U
#define SIO_ADC7_TYPE          SENSOR_TYPE_VIN
#define SIO_ADC7_INDEX         0x2CU
#define SIO_ADC7               "VCORE VRM"
#define SIO_ADC7_CALCULATE(raw) \
    ((int32_t)((uint32_t)(raw) * 16U))

/* NCT6686D pin 113: VIN14 / THR14 / TD0P. */
#define SIO_ADC14_ENABLED       1U
#define SIO_ADC14_TYPE          SENSOR_TYPE_THR
#define SIO_ADC14_INDEX         0x02U
#define SIO_ADC14               "CPU SOCKET"
#define SIO_ADC14_CALCULATE(raw) SENSOR_THR_RAW_TO_C10(raw)

/* NCT6686D pin 112: VIN15 / THR15 / TD1P. */
#define SIO_ADC15_ENABLED       1U
#define SIO_ADC15_TYPE          SENSOR_TYPE_THR
#define SIO_ADC15_INDEX         0x04U
#define SIO_ADC15               "SYSTEM TEMP"
#define SIO_ADC15_CALCULATE(raw) SENSOR_THR_RAW_TO_C10(raw)

/* NCT6686D pin 111: VIN16 / THR16 / TP2P. */
#define SIO_ADC16_ENABLED       1U
#define SIO_ADC16_TYPE          SENSOR_TYPE_THR
#define SIO_ADC16_INDEX         0x08U
#define SIO_ADC16               "MOS TEMP"
#define SIO_ADC16_CALCULATE(raw) SENSOR_THR_RAW_TO_C10(raw)

/* Non-ADC monitor sources retained from the confirmed BIOS mapping. */
#define SIO_SOURCE1_ENABLED       1U
#define SIO_SOURCE1_TYPE          SENSOR_TYPE_VIN
#define SIO_SOURCE1_INDEX         0x30U
#define SIO_SOURCE1               "SYSTEM 3V3"
#define SIO_SOURCE1_CALCULATE(raw) \
    ((int32_t)((((uint32_t)(raw) * 3396U) + 106U) / 212U))

#define SIO_SOURCE2_ENABLED       1U
#define SIO_SOURCE2_TYPE          SENSOR_TYPE_THR
#define SIO_SOURCE2_INDEX         0x00U
#define SIO_SOURCE2               "CPU TEMP"
#define SIO_SOURCE2_CALCULATE(raw) SENSOR_THR_RAW_TO_C10(raw)

#define SIO_SOURCE3_ENABLED       1U
#define SIO_SOURCE3_TYPE          SENSOR_TYPE_THR
#define SIO_SOURCE3_INDEX         0x06U
#define SIO_SOURCE3               "PCH TEMP"
#define SIO_SOURCE3_CALCULATE(raw) SENSOR_THR_RAW_TO_C10(raw)

/* Add or reorder entries here to change the OLED carousel order. */
#define SENSOR_TABLE(X)                                                 \
    X(ADC0, SIO_ADC0_ENABLED, SIO_ADC0_TYPE, SIO_ADC0_INDEX,             \
      SIO_ADC0, SIO_ADC0_CALCULATE)                                     \
    X(ADC1, SIO_ADC1_ENABLED, SIO_ADC1_TYPE, SIO_ADC1_INDEX,             \
      SIO_ADC1, SIO_ADC1_CALCULATE)                                     \
    X(SOURCE1, SIO_SOURCE1_ENABLED, SIO_SOURCE1_TYPE, SIO_SOURCE1_INDEX, \
      SIO_SOURCE1, SIO_SOURCE1_CALCULATE)                               \
    X(ADC2, SIO_ADC2_ENABLED, SIO_ADC2_TYPE, SIO_ADC2_INDEX,             \
      SIO_ADC2, SIO_ADC2_CALCULATE)                                     \
    X(ADC3, SIO_ADC3_ENABLED, SIO_ADC3_TYPE, SIO_ADC3_INDEX,             \
      SIO_ADC3, SIO_ADC3_CALCULATE)                                     \
    X(ADC5, SIO_ADC5_ENABLED, SIO_ADC5_TYPE, SIO_ADC5_INDEX,             \
      SIO_ADC5, SIO_ADC5_CALCULATE)                                     \
    X(ADC6, SIO_ADC6_ENABLED, SIO_ADC6_TYPE, SIO_ADC6_INDEX,             \
      SIO_ADC6, SIO_ADC6_CALCULATE)                                     \
    X(ADC7, SIO_ADC7_ENABLED, SIO_ADC7_TYPE, SIO_ADC7_INDEX,             \
      SIO_ADC7, SIO_ADC7_CALCULATE)                                     \
    X(SOURCE2, SIO_SOURCE2_ENABLED, SIO_SOURCE2_TYPE, SIO_SOURCE2_INDEX, \
      SIO_SOURCE2, SIO_SOURCE2_CALCULATE)                               \
    X(ADC14, SIO_ADC14_ENABLED, SIO_ADC14_TYPE, SIO_ADC14_INDEX,         \
      SIO_ADC14, SIO_ADC14_CALCULATE)                                   \
    X(ADC15, SIO_ADC15_ENABLED, SIO_ADC15_TYPE, SIO_ADC15_INDEX,         \
      SIO_ADC15, SIO_ADC15_CALCULATE)                                   \
    X(ADC16, SIO_ADC16_ENABLED, SIO_ADC16_TYPE, SIO_ADC16_INDEX,         \
      SIO_ADC16, SIO_ADC16_CALCULATE)                                   \
    X(SOURCE3, SIO_SOURCE3_ENABLED, SIO_SOURCE3_TYPE, SIO_SOURCE3_INDEX, \
      SIO_SOURCE3, SIO_SOURCE3_CALCULATE)

#endif
