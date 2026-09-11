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

#include "sio_i2c_hw.h"

#include <stddef.h>

#include "board.h"

#define SIO_ADDRESS_7BIT       0x2DU
#define SIO_ADDRESS_HAL        ((uint16_t)(SIO_ADDRESS_7BIT << 1U))
/* Leave ample margin for the PY32F030 repeated-START timing limitation and
   for the relatively slow rise time of the motherboard/SIO bus. */
#define SIO_I2C_SPEED_HZ       50000U
#define SIO_STAGE_TIMEOUT_MS   100U
#define SIO_NO_PROGRESS_IRQ_LIMIT 8U
#define SIO_I2C_ERROR_FLAGS    (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)
#define SIO_PORT80_POINTER_INDEX 0xE3U
#define SIO_PORT81_POINTER_INDEX 0xE8U
#define SIO_PORT81_DATA_INDEX_OR 0x80U
#define SIO_POST_DATA_PAGE       0x11U

typedef enum {
    SIO_POLL_PORT80 = 0,
    SIO_POLL_PORT81,
} SioPollPort;

typedef enum {
    POLL_IDLE = 0,
    POLL_WRITE_POINTER_PAGE,
    POLL_WRITE_POINTER_INDEX,
    POLL_READ_POINTER,
    POLL_WRITE_DATA_PAGE,
    POLL_WRITE_DATA_INDEX,
    POLL_READ_CODE,
} PollStep;

typedef enum {
    TRANSITION_NONE = 0,
    TRANSITION_WRITE_POINTER_INDEX,
    TRANSITION_READ_POINTER,
    TRANSITION_RECEIVE_POINTER,
    TRANSITION_WRITE_DATA_PAGE,
    TRANSITION_WRITE_DATA_INDEX,
    TRANSITION_READ_CODE,
    TRANSITION_RECEIVE_CODE,
    TRANSITION_COMPLETE,
} PendingTransition;

typedef enum {
    FAILURE_NONE = 0,
    FAILURE_START_TRANSMIT,
    FAILURE_START_MEMORY_READ,
    FAILURE_REPEATED_START,
    FAILURE_STAGE_TIMEOUT,
    FAILURE_UNEXPECTED_STATE,
    FAILURE_IRQ_NO_PROGRESS,
    FAILURE_HAL_CALLBACK,
    FAILURE_COUNT,
} FailureSource;

static I2C_HandleTypeDef i2c_handle;
static volatile PollStep poll_step;
static volatile uint32_t stage_deadline_ms;
static volatile bool sample_pending;
static volatile bool register_sample_pending;
static volatile bool error_pending;
static volatile bool recovery_requested;
static volatile bool repeated_start_waiting;
static volatile PendingTransition pending_transition;
static volatile SioPostcodeSample completed_sample;
static volatile SioRegisterSample completed_register_sample;
static volatile SioPollPort active_port;
static volatile bool combined_poll_active;
static volatile uint8_t no_progress_irq_count;
static volatile uint8_t max_no_progress_irq_count;
static volatile uint32_t forced_irq_recovery_count;
static volatile uint32_t last_storm_cr1;
static volatile uint32_t last_storm_cr2;
static volatile uint32_t last_storm_sr1;
static volatile uint32_t last_storm_sr2;
static volatile uint32_t last_storm_state;
static volatile uint32_t last_storm_mode;
static volatile uint32_t last_storm_event_count;
static volatile uint16_t last_storm_xfer_count;
static volatile uint8_t last_storm_poll_step;
static volatile uint32_t failure_counts[FAILURE_COUNT];
static volatile uint32_t last_failure_hal_error;
static volatile uint32_t last_failure_sr1;
static volatile uint32_t last_failure_sr2;
static volatile uint8_t last_failure_source;
static volatile uint8_t last_failure_poll_step;

static uint8_t pointer_page[2] = {0xFFU, 0x10U};
static uint8_t pointer_index[2] = {0xFEU, SIO_PORT80_POINTER_INDEX};
static uint8_t data_page[2] = {0xFFU, SIO_POST_DATA_PAGE};
static uint8_t data_index[2] = {0xFEU, 0x00U};
static uint8_t read_register = 0x00U;
static uint8_t pointer_value;
static uint8_t code_value;
static uint8_t requested_page;
static uint8_t requested_index;

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static void quiesce_i2c_irq(void)
{
    __HAL_I2C_DISABLE_IT(&i2c_handle, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);
    HAL_NVIC_DisableIRQ(I2C1_IRQn);
    HAL_NVIC_ClearPendingIRQ(I2C1_IRQn);
}

static void flag_start_failure(FailureSource source)
{
    ++failure_counts[source];
    last_failure_hal_error = i2c_handle.ErrorCode;
    last_failure_sr1 = i2c_handle.Instance->SR1;
    last_failure_sr2 = i2c_handle.Instance->SR2;
    last_failure_source = (uint8_t)source;
    last_failure_poll_step = (uint8_t)poll_step;
    quiesce_i2c_irq();
    repeated_start_waiting = false;
    poll_step = POLL_IDLE;
    pending_transition = TRANSITION_NONE;
    combined_poll_active = false;
    error_pending = true;
    recovery_requested = true;
}

static bool start_transmit(PollStep step, uint8_t *data, uint16_t length)
{
    poll_step = step;
    stage_deadline_ms = HAL_GetTick() + SIO_STAGE_TIMEOUT_MS;
    if (HAL_I2C_Master_Transmit_IT(&i2c_handle, SIO_ADDRESS_HAL, data, length) != HAL_OK) {
        flag_start_failure(FAILURE_START_TRANSMIT);
        return false;
    }
    return true;
}

static bool start_register_read(PollStep step)
{
    HAL_StatusTypeDef status;

    poll_step = step;
    stage_deadline_ms = HAL_GetTick() + SIO_STAGE_TIMEOUT_MS;
    status = HAL_I2C_Master_Seq_Transmit_IT(&i2c_handle,
                                            SIO_ADDRESS_HAL,
                                            &read_register,
                                            1U,
                                            I2C_FIRST_FRAME);
    if (status != HAL_OK) {
        flag_start_failure(FAILURE_START_MEMORY_READ);
        return false;
    }
    return true;
}

static bool start_register_receive(uint8_t *destination)
{
    HAL_StatusTypeDef status;
    uint32_t primask = __get_PRIMASK();

    /* The sequential receive API enables EVT before returning. Mask CPU
       interrupts around that handoff so a still-asserted transmitter BTF
       cannot enter the vendor ISR before EVT/BUF are disabled below. */
    __disable_irq();
    status = HAL_I2C_Master_Seq_Receive_IT(&i2c_handle,
                                           SIO_ADDRESS_HAL,
                                           destination,
                                           1U,
                                           I2C_LAST_FRAME);
    if (status == HAL_OK) {
        __HAL_I2C_DISABLE_IT(&i2c_handle, I2C_IT_EVT | I2C_IT_BUF);
        repeated_start_waiting = true;
        no_progress_irq_count = 0U;
    }
    if (primask == 0U) {
        __enable_irq();
    }

    if (status != HAL_OK) {
        flag_start_failure(FAILURE_REPEATED_START);
        return false;
    }
    return true;
}

static void recover_bus(void)
{
    uint8_t pulse;
    uint16_t wait;

    quiesce_i2c_irq();
    (void)HAL_I2C_DeInit(&i2c_handle);
    board_sio_i2c_pins_as_gpio();

    HAL_GPIO_WritePin(GPIOA,
                      BOARD_SIO_I2C_SCL_PIN | BOARD_SIO_I2C_SDA_PIN,
                      GPIO_PIN_SET);
    for (wait = 0U; wait < 200U; ++wait) {
        if (HAL_GPIO_ReadPin(BOARD_SIO_I2C_SCL_PORT, BOARD_SIO_I2C_SCL_PIN) == GPIO_PIN_SET) {
            break;
        }
        board_delay_us(5U);
    }

    if (HAL_GPIO_ReadPin(BOARD_SIO_I2C_SCL_PORT, BOARD_SIO_I2C_SCL_PIN) == GPIO_PIN_SET) {
        for (pulse = 0U; pulse < 9U; ++pulse) {
            if (HAL_GPIO_ReadPin(BOARD_SIO_I2C_SDA_PORT, BOARD_SIO_I2C_SDA_PIN) == GPIO_PIN_SET) {
                break;
            }
            HAL_GPIO_WritePin(BOARD_SIO_I2C_SCL_PORT, BOARD_SIO_I2C_SCL_PIN, GPIO_PIN_RESET);
            board_delay_us(5U);
            HAL_GPIO_WritePin(BOARD_SIO_I2C_SCL_PORT, BOARD_SIO_I2C_SCL_PIN, GPIO_PIN_SET);
            board_delay_us(5U);
        }

        /* STOP: SDA low, SCL released, SDA released. */
        HAL_GPIO_WritePin(BOARD_SIO_I2C_SDA_PORT, BOARD_SIO_I2C_SDA_PIN, GPIO_PIN_RESET);
        board_delay_us(5U);
        HAL_GPIO_WritePin(BOARD_SIO_I2C_SCL_PORT, BOARD_SIO_I2C_SCL_PIN, GPIO_PIN_SET);
        board_delay_us(5U);
        HAL_GPIO_WritePin(BOARD_SIO_I2C_SDA_PORT, BOARD_SIO_I2C_SDA_PIN, GPIO_PIN_SET);
        board_delay_us(5U);
    }

    i2c_handle.State = HAL_I2C_STATE_RESET;
    poll_step = POLL_IDLE;
    pending_transition = TRANSITION_NONE;
    active_port = SIO_POLL_PORT80;
    combined_poll_active = false;
    repeated_start_waiting = false;
    no_progress_irq_count = 0U;
    if (HAL_I2C_Init(&i2c_handle) == HAL_OK) {
        recovery_requested = false;
    } else {
        error_pending = true;
        recovery_requested = true;
    }
}

bool sio_i2c_init(void)
{
    poll_step = POLL_IDLE;
    sample_pending = false;
    register_sample_pending = false;
    error_pending = false;
    recovery_requested = false;
    repeated_start_waiting = false;
    pending_transition = TRANSITION_NONE;
    active_port = SIO_POLL_PORT80;
    combined_poll_active = false;
    no_progress_irq_count = 0U;
    max_no_progress_irq_count = 0U;
    forced_irq_recovery_count = 0U;
    last_storm_cr1 = 0U;
    last_storm_cr2 = 0U;
    last_storm_sr1 = 0U;
    last_storm_sr2 = 0U;
    last_storm_state = 0U;
    last_storm_mode = 0U;
    last_storm_event_count = 0U;
    last_storm_xfer_count = 0U;
    last_storm_poll_step = 0U;
    for (uint8_t failure = 0U; failure < (uint8_t)FAILURE_COUNT; ++failure) {
        failure_counts[failure] = 0U;
    }
    last_failure_hal_error = 0U;
    last_failure_sr1 = 0U;
    last_failure_sr2 = 0U;
    last_failure_source = FAILURE_NONE;
    last_failure_poll_step = POLL_IDLE;

    i2c_handle.Instance = BOARD_SIO_I2C_INSTANCE;
    i2c_handle.Init.ClockSpeed = SIO_I2C_SPEED_HZ;
    i2c_handle.Init.DutyCycle = I2C_DUTYCYCLE_2;
    i2c_handle.Init.OwnAddress1 = 0U;
    i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    return HAL_I2C_Init(&i2c_handle) == HAL_OK;
}

bool sio_i2c_start_poll(void)
{
    if ((poll_step != POLL_IDLE) || recovery_requested || sample_pending ||
        register_sample_pending) {
        return false;
    }
    active_port = SIO_POLL_PORT80;
    combined_poll_active = true;
    pointer_index[1] = SIO_PORT80_POINTER_INDEX;
    data_page[1] = SIO_POST_DATA_PAGE;
    return start_transmit(POLL_WRITE_POINTER_PAGE, pointer_page, sizeof(pointer_page));
}

bool sio_i2c_start_data_read(uint8_t page, uint8_t index)
{
    if ((poll_step != POLL_IDLE) || recovery_requested || sample_pending ||
        register_sample_pending) {
        return false;
    }

    combined_poll_active = false;
    requested_page = page;
    requested_index = index;
    data_page[1] = page;
    data_index[1] = index;
    return start_transmit(POLL_WRITE_DATA_PAGE, data_page, sizeof(data_page));
}

void sio_i2c_service(uint32_t now_ms)
{
    PendingTransition transition;

    if (repeated_start_waiting) {
        uint32_t status = i2c_handle.Instance->SR1;

        if ((status & SIO_I2C_ERROR_FLAGS) != 0U) {
            flag_start_failure(FAILURE_REPEATED_START);
        } else if ((status & I2C_SR1_SB) != 0U) {
            /* The target has released SCL and hardware has produced the
               repeated START. Re-enable event handling so HAL can send the
               read address. SB remains set until the ISR writes DR. */
            repeated_start_waiting = false;
            no_progress_irq_count = 0U;
            __HAL_I2C_ENABLE_IT(&i2c_handle, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);
        }
    }

    if ((poll_step != POLL_IDLE) && deadline_reached(now_ms, stage_deadline_ms)) {
        flag_start_failure(FAILURE_STAGE_TIMEOUT);
    }
    if (recovery_requested) {
        recover_bus();
        return;
    }

    transition = pending_transition;
    if (transition == TRANSITION_NONE) {
        return;
    }

    /* The HAL completion callback runs immediately after it requests STOP;
       BUSY can remain asserted until the STOP is actually present on the
       wires, especially when the SIO is stretching SCL. Starting the next
       transfer here would make the HAL spin in its BUSY wait and can turn a
       valid stretch into a false transaction-start failure. Leave the
       transition queued and let the main loop retry without blocking. */
    if ((transition != TRANSITION_RECEIVE_POINTER) &&
        (transition != TRANSITION_RECEIVE_CODE) &&
        (__HAL_I2C_GET_FLAG(&i2c_handle, I2C_FLAG_BUSY) != RESET)) {
        return;
    }
    pending_transition = TRANSITION_NONE;

    switch (transition) {
    case TRANSITION_WRITE_POINTER_INDEX:
        (void)start_transmit(POLL_WRITE_POINTER_INDEX, pointer_index, sizeof(pointer_index));
        break;
    case TRANSITION_READ_POINTER:
        (void)start_register_read(POLL_READ_POINTER);
        break;
    case TRANSITION_RECEIVE_POINTER:
        (void)start_register_receive(&pointer_value);
        break;
    case TRANSITION_WRITE_DATA_PAGE:
        (void)start_transmit(POLL_WRITE_DATA_PAGE, data_page, sizeof(data_page));
        break;
    case TRANSITION_WRITE_DATA_INDEX:
        (void)start_transmit(POLL_WRITE_DATA_INDEX, data_index, sizeof(data_index));
        break;
    case TRANSITION_READ_CODE:
        (void)start_register_read(POLL_READ_CODE);
        break;
    case TRANSITION_RECEIVE_CODE:
        (void)start_register_receive(&code_value);
        break;
    case TRANSITION_COMPLETE:
        if (combined_poll_active && (active_port == SIO_POLL_PORT80)) {
            completed_sample.pointer = pointer_value;
            completed_sample.code = code_value;
            active_port = SIO_POLL_PORT81;
            pointer_index[1] = SIO_PORT81_POINTER_INDEX;
            (void)start_transmit(POLL_WRITE_POINTER_PAGE,
                                 pointer_page,
                                 sizeof(pointer_page));
        } else {
            if (combined_poll_active) {
                completed_sample.port81_pointer = pointer_value;
                completed_sample.port81_code = code_value;
                sample_pending = true;
            } else {
                completed_register_sample.page = requested_page;
                completed_register_sample.index = requested_index;
                completed_register_sample.value = code_value;
                register_sample_pending = true;
            }
            combined_poll_active = false;
            poll_step = POLL_IDLE;
        }
        break;
    default:
        flag_start_failure(FAILURE_UNEXPECTED_STATE);
        break;
    }
}

bool sio_i2c_take_sample(SioPostcodeSample *sample)
{
    uint32_t primask;

    if ((sample == NULL) || !sample_pending) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    sample->pointer = completed_sample.pointer;
    sample->code = completed_sample.code;
    sample->port81_pointer = completed_sample.port81_pointer;
    sample->port81_code = completed_sample.port81_code;
    sample_pending = false;
    if (primask == 0U) {
        __enable_irq();
    }
    return true;
}

bool sio_i2c_take_register_sample(SioRegisterSample *sample)
{
    uint32_t primask;

    if ((sample == NULL) || !register_sample_pending) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    sample->page = completed_register_sample.page;
    sample->index = completed_register_sample.index;
    sample->value = completed_register_sample.value;
    register_sample_pending = false;
    if (primask == 0U) {
        __enable_irq();
    }
    return true;
}

bool sio_i2c_take_error(void)
{
    uint32_t primask;
    bool pending;

    primask = __get_PRIMASK();
    __disable_irq();
    pending = error_pending;
    error_pending = false;
    if (primask == 0U) {
        __enable_irq();
    }
    return pending;
}

bool sio_i2c_is_busy(void)
{
    return (poll_step != POLL_IDLE) || (pending_transition != TRANSITION_NONE);
}

void sio_i2c_irq_handler(void)
{
    uint32_t cr1_before = i2c_handle.Instance->CR1;
    uint32_t cr2_before = i2c_handle.Instance->CR2;
    HAL_I2C_StateTypeDef state_before = i2c_handle.State;
    HAL_I2C_ModeTypeDef mode_before = i2c_handle.Mode;
    uint16_t count_before = i2c_handle.XferCount;
    uint32_t event_before = i2c_handle.EventCount;
    uint8_t *buffer_before = i2c_handle.pBuffPtr;

    /* Do not pre-read SR1 here: on this peripheral an SR1 read followed by
       the HAL's SR2 read can clear ADDR before the HAL handles it. Instead,
       detect an interrupt that made no register or transfer-state progress. */
    HAL_I2C_IRQHandler(&i2c_handle);

    if ((cr1_before != i2c_handle.Instance->CR1) ||
        (cr2_before != i2c_handle.Instance->CR2) ||
        (state_before != i2c_handle.State) ||
        (mode_before != i2c_handle.Mode) ||
        (count_before != i2c_handle.XferCount) ||
        (event_before != i2c_handle.EventCount) ||
        (buffer_before != i2c_handle.pBuffPtr)) {
        no_progress_irq_count = 0U;
        return;
    }

    ++no_progress_irq_count;
    if (no_progress_irq_count > max_no_progress_irq_count) {
        max_no_progress_irq_count = no_progress_irq_count;
    }
    if (no_progress_irq_count > SIO_NO_PROGRESS_IRQ_LIMIT) {
        /* Reading SR1 then SR2 can clear ADDR, so take this snapshot only
           after deciding to abort and reset the peripheral. */
        last_storm_cr1 = i2c_handle.Instance->CR1;
        last_storm_cr2 = i2c_handle.Instance->CR2;
        last_storm_sr1 = i2c_handle.Instance->SR1;
        last_storm_sr2 = i2c_handle.Instance->SR2;
        last_storm_state = (uint32_t)i2c_handle.State;
        last_storm_mode = (uint32_t)i2c_handle.Mode;
        last_storm_event_count = i2c_handle.EventCount;
        last_storm_xfer_count = i2c_handle.XferCount;
        last_storm_poll_step = (uint8_t)poll_step;
        ++forced_irq_recovery_count;
        flag_start_failure(FAILURE_IRQ_NO_PROGRESS);
    }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != &i2c_handle) {
        return;
    }

    switch (poll_step) {
    case POLL_WRITE_POINTER_PAGE:
        pending_transition = TRANSITION_WRITE_POINTER_INDEX;
        break;
    case POLL_WRITE_POINTER_INDEX:
        pending_transition = TRANSITION_READ_POINTER;
        break;
    case POLL_READ_POINTER:
        pending_transition = TRANSITION_RECEIVE_POINTER;
        break;
    case POLL_WRITE_DATA_PAGE:
        pending_transition = TRANSITION_WRITE_DATA_INDEX;
        break;
    case POLL_WRITE_DATA_INDEX:
        pending_transition = TRANSITION_READ_CODE;
        break;
    case POLL_READ_CODE:
        pending_transition = TRANSITION_RECEIVE_CODE;
        break;
    default:
        flag_start_failure(FAILURE_UNEXPECTED_STATE);
        break;
    }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != &i2c_handle) {
        return;
    }

    if (poll_step == POLL_READ_POINTER) {
        uint8_t index_or = (combined_poll_active && (active_port == SIO_POLL_PORT81))
                               ? SIO_PORT81_DATA_INDEX_OR
                               : 0U;
        data_index[1] = (uint8_t)((pointer_value - 1U) | index_or);
        pending_transition = TRANSITION_WRITE_DATA_PAGE;
    } else if (poll_step == POLL_READ_CODE) {
        pending_transition = TRANSITION_COMPLETE;
    } else {
        flag_start_failure(FAILURE_UNEXPECTED_STATE);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &i2c_handle) {
        flag_start_failure(FAILURE_HAL_CALLBACK);
    }
}
