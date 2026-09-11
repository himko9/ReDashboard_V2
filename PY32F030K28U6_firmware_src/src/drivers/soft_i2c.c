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

#include "soft_i2c.h"

#include "board.h"

#define SOFT_I2C_STRETCH_GUARD_US 1000U

static inline void scl_release(void)
{
    BOARD_OLED_SCL_PORT->BSRR = BOARD_OLED_SCL_PIN;
}

static inline void scl_low(void)
{
    BOARD_OLED_SCL_PORT->BRR = BOARD_OLED_SCL_PIN;
}

static inline void sda_release(void)
{
    BOARD_OLED_SDA_PORT->BSRR = BOARD_OLED_SDA_PIN;
}

static inline void sda_low(void)
{
    BOARD_OLED_SDA_PORT->BRR = BOARD_OLED_SDA_PIN;
}

static void half_period(void)
{
    board_delay_us(BOARD_SOFT_I2C_HALF_PERIOD_US);
}

static bool wait_scl_high(void)
{
    uint32_t guard = SOFT_I2C_STRETCH_GUARD_US;

    while ((BOARD_OLED_SCL_PORT->IDR & BOARD_OLED_SCL_PIN) == 0U) {
        if (guard == 0U) {
            return false;
        }
        --guard;
        board_delay_us(1U);
    }
    return true;
}

static bool i2c_start(void)
{
    sda_release();
    scl_release();
    if (!wait_scl_high()) {
        return false;
    }
    half_period();
    if ((BOARD_OLED_SDA_PORT->IDR & BOARD_OLED_SDA_PIN) == 0U) {
        return false;
    }
    sda_low();
    half_period();
    scl_low();
    return true;
}

static void i2c_stop(void)
{
    sda_low();
    half_period();
    scl_release();
    (void)wait_scl_high();
    half_period();
    sda_release();
    half_period();
}

static bool write_byte(uint8_t value)
{
    uint8_t bit;
    bool acknowledged;

    for (bit = 0U; bit < 8U; ++bit) {
        if ((value & 0x80U) != 0U) {
            sda_release();
        } else {
            sda_low();
        }
        half_period();
        scl_release();
        if (!wait_scl_high()) {
            scl_low();
            return false;
        }
        half_period();
        scl_low();
        value <<= 1U;
    }

    sda_release();
    half_period();
    scl_release();
    if (!wait_scl_high()) {
        scl_low();
        return false;
    }
    half_period();
    acknowledged = (BOARD_OLED_SDA_PORT->IDR & BOARD_OLED_SDA_PIN) == 0U;
    scl_low();
    return acknowledged;
}

void soft_i2c_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t clocks;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, BOARD_OLED_SCL_PIN | BOARD_OLED_SDA_PIN, GPIO_PIN_SET);
    gpio.Pin = BOARD_OLED_SCL_PIN | BOARD_OLED_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    sda_release();
    scl_release();
    half_period();

    /* Release a target that was interrupted mid-byte, then emit STOP. */
    if (((BOARD_OLED_SDA_PORT->IDR & BOARD_OLED_SDA_PIN) == 0U) && wait_scl_high()) {
        for (clocks = 0U; clocks < 9U; ++clocks) {
            scl_low();
            half_period();
            scl_release();
            (void)wait_scl_high();
            half_period();
            if ((BOARD_OLED_SDA_PORT->IDR & BOARD_OLED_SDA_PIN) != 0U) {
                break;
            }
        }
    }
    i2c_stop();
}

bool soft_i2c_write(uint8_t address_7bit,
                    uint8_t control,
                    const uint8_t *data,
                    size_t length)
{
    size_t index;

    if ((length > 0U) && (data == NULL)) {
        return false;
    }
    if (!i2c_start()) {
        i2c_stop();
        return false;
    }
    if (!write_byte((uint8_t)(address_7bit << 1U)) || !write_byte(control)) {
        i2c_stop();
        return false;
    }

    for (index = 0U; index < length; ++index) {
        if (!write_byte(data[index])) {
            i2c_stop();
            return false;
        }
    }

    i2c_stop();
    return true;
}
