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

#include "uart_fast_rx.h"

#include <stddef.h>

#include "board.h"
#include "post_value_filter.h"

/* The source uses a non-standard variable-length asynchronous frame:
 *
 *   one start bit
 *   16 LSB-first port bits
 *   two LSB-first width bits (00/01/10 = 8/16/32 payload bits)
 *   8, 16, or 32 LSB-first payload bits
 *   no parity, two stop bits
 *
 * A PY32 USART cannot receive this format. At 48 MHz and 1.5 Mbaud there
 * are exactly 32 timer clocks per bit. TIM1_CH4 captures falling TI4 edges;
 * TIM1_CH3 is mapped indirectly to the same TI4 input and captures rising
 * edges. Two DMA channels store the timestamps. A streaming decoder merges
 * them and visits every transition exactly once; no 1.5 MHz interrupt stream
 * or repeated rescanning of captured edges is required.
 */
#define UART_FAST_TIMER_HZ          48000000U
#define UART_FAST_BAUD              1500000U
#define UART_FAST_TICKS_PER_BIT     (UART_FAST_TIMER_HZ / UART_FAST_BAUD)
#define UART_FAST_HEADER_BITS       18U
#define UART_FAST_MIN_PAYLOAD_BITS  8U
#define UART_FAST_MAX_PAYLOAD_BITS  32U
#define UART_FAST_STOP_BITS         2U
#define UART_FAST_MIN_FRAME_BITS    \
    (1U + UART_FAST_HEADER_BITS + UART_FAST_MIN_PAYLOAD_BITS + \
     UART_FAST_STOP_BITS)
#define UART_FAST_MAX_FRAME_BITS    \
    (1U + UART_FAST_HEADER_BITS + UART_FAST_MAX_PAYLOAD_BITS + \
     UART_FAST_STOP_BITS)
#define UART_FAST_MAX_FRAME_TICKS   \
    (UART_FAST_MAX_FRAME_BITS * UART_FAST_TICKS_PER_BIT)
#define UART_FAST_HEADER_TICKS      \
    ((1U + UART_FAST_HEADER_BITS) * UART_FAST_TICKS_PER_BIT)
#define UART_FAST_FIRST_CENTER      (UART_FAST_TICKS_PER_BIT / 2U)
#define UART_FAST_BOUNDARY_TOLERANCE UART_FAST_FIRST_CENTER

#define DMA_CAPTURE_CAPACITY        768U
#define RAW_RING_CAPACITY           64U
#define RAW_RING_MASK               (RAW_RING_CAPACITY - 1U)

#if (UART_FAST_TIMER_HZ % UART_FAST_BAUD) != 0
#error "The variable-width receiver requires an integer timer-clock/baud ratio"
#endif
#if UART_FAST_TICKS_PER_BIT != 32U
#error "The custom UART timing constants assume 48 MHz / 1.5 Mbaud"
#endif

typedef struct {
    uint32_t low;
    uint32_t high;
} StreamSlots;

static volatile uint16_t dma_rising_ring[DMA_CAPTURE_CAPACITY];
static volatile uint16_t dma_falling_ring[DMA_CAPTURE_CAPACITY];
static volatile uint32_t dma_rising_wrap_count;
static volatile uint32_t dma_falling_wrap_count;
static volatile bool dma_fault_pending;
static uint32_t dma_rising_consumer_total;
static uint32_t dma_falling_consumer_total;
static uint16_t dma_rising_consumer_index;
static uint16_t dma_falling_consumer_index;

static bool stream_active;
static uint16_t stream_start;
static uint8_t stream_level;
static uint8_t stream_run_begin;
static uint8_t stream_frame_bits;
static bool stream_trusted_start;
static StreamSlots stream_slots;
static bool framing_locked;
static bool sync_candidate_pending;
static uint16_t sync_candidate_start;
static uint16_t sync_candidate_frame_ticks;
static uint64_t sync_candidate_raw;
static bool wire_level_high;
static bool idle_high_qualified;
static uint16_t idle_high_since;

static uint64_t raw_ring[RAW_RING_CAPACITY];
static volatile uint8_t raw_head;
static volatile uint8_t raw_tail;
static uint64_t last_post_raw;
static bool last_post_raw_valid;
static volatile bool receiver_active;

static volatile uint32_t record_count;
static volatile uint32_t error_count;
static volatile uint32_t overflow_count;
static volatile uint32_t raw_ring_overflow_count;
static volatile uint32_t edge_queue_overflow_count;
static volatile uint32_t dma_backlog_overflow_count;
static volatile uint32_t dma_backlog_high_water;
static volatile uint32_t filtered_out32_count;
static volatile uint32_t dma_half_wakeup_count;
static volatile uint32_t decoder_service_count;
static volatile uint32_t idle_resync_count;
static volatile uint32_t timer_overcapture_count;
static volatile uint32_t dma_transfer_error_count;

static uint16_t timer_delta(uint16_t later, uint16_t earlier)
{
    return (uint16_t)(later - earlier);
}

static void stream_decoder_reset(void)
{
    stream_active = false;
    stream_start = 0U;
    stream_level = 1U;
    stream_run_begin = 0U;
    stream_frame_bits = 0U;
    stream_trusted_start = false;
    stream_slots.low = 0U;
    stream_slots.high = 0U;
    framing_locked = false;
    sync_candidate_pending = false;
    wire_level_high = false;
    idle_high_qualified = false;
    idle_high_since = 0U;
}

static void decoder_reset(void)
{
    stream_decoder_reset();
    raw_head = 0U;
    raw_tail = 0U;
    last_post_raw = 0U;
    last_post_raw_valid = false;
}

static void raw_push(uint64_t raw)
{
    uint8_t next = (uint8_t)((raw_head + 1U) & RAW_RING_MASK);

    if (next == raw_tail) {
        ++raw_ring_overflow_count;
        ++overflow_count;
        ++error_count;
        return;
    }
    raw_ring[raw_head] = raw;
    raw_head = next;
}

static bool raw_is_post_record(uint64_t raw)
{
    uint16_t port = (uint16_t)raw;

    return ((port == 0x0080U) || (port == 0x0081U)) &&
           (((raw >> 16U) & 0x03U) <= 2U);
}

static void accept_record(uint64_t raw)
{
    ++record_count;
    if (raw_is_post_record(raw)) {
        uint8_t width_bytes =
            (uint8_t)(1U << ((uint8_t)(raw >> 16U) & 0x03U));
        uint32_t payload = (uint32_t)(raw >> 18U);

        if (!post_value_is_visible(payload, width_bytes)) {
            ++filtered_out32_count;
            return;
        }
        /* Firmware-visible history records changes, not bus traffic. This
           collapses repeated visible writes while retaining every OUT8/OUT16
           value or width transition. */
        if (!last_post_raw_valid || (raw != last_post_raw)) {
            raw_push(raw);
            last_post_raw = raw;
            last_post_raw_valid = true;
        }
    }
}

static uint32_t slot_mask32(uint8_t begin, uint8_t end)
{
    uint8_t count;

    if (end <= begin) {
        return 0U;
    }
    count = (uint8_t)(end - begin);
    if (count >= 32U) {
        return 0xFFFFFFFFU;
    }
    return ((1UL << count) - 1UL) << begin;
}

static void stream_set_high_range(uint8_t begin, uint8_t end)
{
    if ((end <= begin) || (begin >= UART_FAST_MAX_FRAME_BITS)) {
        return;
    }
    if (end > UART_FAST_MAX_FRAME_BITS) {
        end = UART_FAST_MAX_FRAME_BITS;
    }
    if (begin < 32U) {
        uint8_t low_end = end < 32U ? end : 32U;

        stream_slots.low |= slot_mask32(begin, low_end);
    }
    if (end > 32U) {
        uint8_t high_begin = begin > 32U ? (uint8_t)(begin - 32U) : 0U;

        stream_slots.high |= slot_mask32(high_begin,
                                          (uint8_t)(end - 32U));
    }
}

static bool stream_fill_to(uint8_t boundary)
{
    if (boundary > UART_FAST_MAX_FRAME_BITS) {
        boundary = UART_FAST_MAX_FRAME_BITS;
    }
    if (boundary < stream_run_begin) {
        return false;
    }
    if (stream_level != 0U) {
        stream_set_high_range(stream_run_begin, boundary);
    }
    stream_run_begin = boundary;
    return true;
}

static void stream_begin(uint16_t timestamp, bool trusted_start)
{
    stream_active = true;
    stream_start = timestamp;
    stream_level = 0U;
    stream_run_begin = 0U;
    stream_frame_bits = 0U;
    stream_trusted_start = trusted_start;
    stream_slots.low = 0U;
    stream_slots.high = 0U;
}

static void stream_reject(void)
{
    stream_active = false;
    stream_frame_bits = 0U;
    stream_trusted_start = false;
    framing_locked = false;
    sync_candidate_pending = false;
}

static bool stream_parse_header(void)
{
    uint32_t header;
    uint8_t payload_bits;

    if ((stream_slots.low & 1U) != 0U) {
        return false;
    }
    header = (stream_slots.low >> 1U) & 0x3FFFFU;
    switch ((header >> 16U) & 0x03U) {
    case 0U:
        payload_bits = 8U;
        break;
    case 1U:
        payload_bits = 16U;
        break;
    case 2U:
        payload_bits = 32U;
        break;
    default:
        return false;
    }
    stream_frame_bits = (uint8_t)(1U + UART_FAST_HEADER_BITS + payload_bits +
                                  UART_FAST_STOP_BITS);
    return true;
}

static void synchronize_record(uint64_t raw)
{
    uint16_t frame_ticks =
        (uint16_t)(stream_frame_bits * UART_FAST_TICKS_PER_BIT);

    if (stream_trusted_start) {
        /* A high idle lasting longer than the longest possible frame proves
           this falling edge cannot be an internal payload transition. This
           lets a lone POST write recover immediately after a DMA fault. */
        framing_locked = true;
        sync_candidate_pending = false;
        ++idle_resync_count;
        accept_record(raw);
    } else if (framing_locked) {
        accept_record(raw);
    } else if (sync_candidate_pending) {
        uint16_t spacing = timer_delta(stream_start, sync_candidate_start);

        if (((uint16_t)(spacing + UART_FAST_BOUNDARY_TOLERANCE) >=
             sync_candidate_frame_ticks) &&
            (spacing <= (uint16_t)(sync_candidate_frame_ticks +
                                   UART_FAST_BOUNDARY_TOLERANCE))) {
            framing_locked = true;
            accept_record(sync_candidate_raw);
            accept_record(raw);
            sync_candidate_pending = false;
        } else {
            sync_candidate_start = stream_start;
            sync_candidate_frame_ticks = frame_ticks;
            sync_candidate_raw = raw;
        }
    } else {
        sync_candidate_pending = true;
        sync_candidate_start = stream_start;
        sync_candidate_frame_ticks = frame_ticks;
        sync_candidate_raw = raw;
    }
}

static bool stream_finish(void)
{
    uint8_t data_bits;
    uint8_t high_data_bits;
    uint32_t raw_low;
    uint32_t raw_high;
    uint64_t raw;

    if ((stream_frame_bits == 0U) ||
        !stream_fill_to(stream_frame_bits)) {
        stream_reject();
        return false;
    }
    data_bits = (uint8_t)(stream_frame_bits - 3U);
    if (((stream_slots.low & 1U) != 0U) ||
        ((data_bits < 31U) &&
         ((stream_slots.low & ((uint32_t)3U << (1U + data_bits))) !=
          ((uint32_t)3U << (1U + data_bits)))) ||
        ((data_bits >= 31U) &&
         ((stream_slots.high &
           ((uint32_t)3U << (1U + data_bits - 32U))) !=
          ((uint32_t)3U << (1U + data_bits - 32U))))) {
        stream_reject();
        return false;
    }

    raw_low = (stream_slots.low >> 1U) |
              ((stream_slots.high & 1U) << 31U);
    if (data_bits < 32U) {
        raw_low &= (1UL << data_bits) - 1UL;
        raw_high = 0U;
    } else {
        high_data_bits = (uint8_t)(data_bits - 32U);
        raw_high = (stream_slots.high >> 1U) &
                   ((1UL << high_data_bits) - 1UL);
    }
    raw = ((uint64_t)raw_high << 32U) | raw_low;
    synchronize_record(raw);
    stream_active = false;
    stream_frame_bits = 0U;
    return true;
}

static void stream_process_edge(uint16_t timestamp, uint8_t level_after)
{
    uint16_t delta;
    uint16_t frame_ticks;
    uint8_t boundary;
    bool trusted_start = (level_after == 0U) && wire_level_high &&
                         idle_high_qualified;

    if (level_after != 0U) {
        wire_level_high = true;
        idle_high_since = timestamp;
        idle_high_qualified = false;
    } else {
        wire_level_high = false;
    }

    if (!stream_active) {
        if (level_after == 0U) {
            stream_begin(timestamp, trusted_start);
        }
        return;
    }

    delta = timer_delta(timestamp, stream_start);
    boundary = (uint8_t)((delta + UART_FAST_FIRST_CENTER) /
                         UART_FAST_TICKS_PER_BIT);
    if (boundary > UART_FAST_MAX_FRAME_BITS) {
        boundary = UART_FAST_MAX_FRAME_BITS;
    }
    if ((stream_frame_bits == 0U) &&
        (boundary >= (1U + UART_FAST_HEADER_BITS))) {
        if (!stream_fill_to(1U + UART_FAST_HEADER_BITS) ||
            !stream_parse_header()) {
            stream_reject();
            if (level_after == 0U) {
                stream_begin(timestamp, trusted_start);
            }
            return;
        }
    }
    if (stream_frame_bits != 0U) {
        frame_ticks =
            (uint16_t)(stream_frame_bits * UART_FAST_TICKS_PER_BIT);
        if ((uint16_t)(delta + UART_FAST_BOUNDARY_TOLERANCE) >=
            frame_ticks) {
            if (!stream_fill_to(stream_frame_bits)) {
                stream_reject();
            } else {
                (void)stream_finish();
            }
            if (level_after == 0U) {
                stream_begin(timestamp, trusted_start);
            }
            return;
        }
    }
    if (!stream_fill_to(boundary)) {
        stream_reject();
        if (level_after == 0U) {
            stream_begin(timestamp, trusted_start);
        }
        return;
    }
    stream_level = level_after;
}

static void stream_advance(uint16_t now)
{
    uint16_t delta;

    if (!stream_active) {
        if (wire_level_high && !idle_high_qualified &&
            (timer_delta(now, idle_high_since) >=
             UART_FAST_MAX_FRAME_TICKS)) {
            idle_high_qualified = true;
        }
        return;
    }
    delta = timer_delta(now, stream_start);
    if ((stream_frame_bits == 0U) && (delta >= UART_FAST_HEADER_TICKS)) {
        if (!stream_fill_to(1U + UART_FAST_HEADER_BITS) ||
            !stream_parse_header()) {
            stream_reject();
            return;
        }
    }
    if ((stream_frame_bits != 0U) &&
        (delta >= (uint16_t)(stream_frame_bits *
                             UART_FAST_TICKS_PER_BIT))) {
        (void)stream_finish();
    }
}

static void dma_account_flags(void)
{
    uint32_t flags = DMA1->ISR;
    uint32_t clear = 0U;
    bool service_requested = false;

    if ((flags & DMA_ISR_HTIF1) != 0U) {
        clear |= DMA_IFCR_CHTIF1;
        service_requested = true;
    }

    if ((flags & DMA_ISR_TCIF1) != 0U) {
        ++dma_rising_wrap_count;
        clear |= DMA_IFCR_CTCIF1;
        service_requested = true;
    }
    if ((flags & DMA_ISR_TEIF1) != 0U) {
        dma_fault_pending = true;
        clear |= DMA_IFCR_CTEIF1;
    }
    if ((flags & DMA_ISR_TCIF2) != 0U) {
        ++dma_falling_wrap_count;
        clear |= DMA_IFCR_CTCIF2;
        service_requested = true;
    }
    if ((flags & DMA_ISR_TEIF2) != 0U) {
        dma_fault_pending = true;
        clear |= DMA_IFCR_CTEIF2;
    }
    if ((flags & DMA_ISR_HTIF2) != 0U) {
        clear |= DMA_IFCR_CHTIF2;
        service_requested = true;
    }
    if (clear != 0U) {
        DMA1->IFCR = clear;
    }
    if (service_requested && receiver_active) {
        if ((flags & (DMA_ISR_HTIF1 | DMA_ISR_HTIF2)) != 0U) {
            ++dma_half_wakeup_count;
        }
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

static void dma_producer_totals(uint32_t *rising, uint32_t *falling)
{
    uint32_t primask = __get_PRIMASK();
    uint32_t rising_remaining_first;
    uint32_t falling_remaining_first;
    uint32_t rising_remaining;
    uint32_t falling_remaining;
    uint32_t rising_wraps;
    uint32_t falling_wraps;
    uint32_t flags_after;

    __disable_irq();
    do {
        dma_account_flags();
        rising_remaining_first = DMA1_Channel1->CNDTR;
        falling_remaining_first = DMA1_Channel2->CNDTR;
        rising_remaining = DMA1_Channel1->CNDTR;
        falling_remaining = DMA1_Channel2->CNDTR;
        flags_after = DMA1->ISR;
        /* CNDTR counts down and reloads to 512 on a circular wrap. If it
           increased between reads, the software wrap counter and remaining
           count would describe different epochs. Account the flag and retry.
           A wrap after the second read is harmless: this snapshot remains a
           consistent pre-wrap value and the next service call sees the flag. */
    } while ((rising_remaining > rising_remaining_first) ||
             (falling_remaining > falling_remaining_first) ||
             ((flags_after & (DMA_ISR_TCIF1 | DMA_ISR_TCIF2)) != 0U));
    rising_wraps = dma_rising_wrap_count;
    falling_wraps = dma_falling_wrap_count;
    if (primask == 0U) {
        __enable_irq();
    }

    *rising = (rising_wraps * DMA_CAPTURE_CAPACITY) +
              (DMA_CAPTURE_CAPACITY - rising_remaining);
    *falling = (falling_wraps * DMA_CAPTURE_CAPACITY) +
               (DMA_CAPTURE_CAPACITY - falling_remaining);
}

bool uart_fast_rx_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    receiver_active = false;
    dma_rising_wrap_count = 0U;
    dma_falling_wrap_count = 0U;
    dma_fault_pending = false;
    dma_rising_consumer_total = 0U;
    dma_falling_consumer_total = 0U;
    dma_rising_consumer_index = 0U;
    dma_falling_consumer_index = 0U;
    record_count = 0U;
    error_count = 0U;
    overflow_count = 0U;
    raw_ring_overflow_count = 0U;
    edge_queue_overflow_count = 0U;
    dma_backlog_overflow_count = 0U;
    dma_backlog_high_water = 0U;
    filtered_out32_count = 0U;
    dma_half_wakeup_count = 0U;
    decoder_service_count = 0U;
    idle_resync_count = 0U;
    timer_overcapture_count = 0U;
    dma_transfer_error_count = 0U;
    decoder_reset();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();

    gpio.Pin = BOARD_FAST_RX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = BOARD_FAST_RX_GPIO_AF;
    HAL_GPIO_Init(BOARD_FAST_RX_PORT, &gpio);

    __HAL_RCC_TIM1_FORCE_RESET();
    __HAL_RCC_TIM1_RELEASE_RESET();

    BOARD_FAST_RX_TIMER->CR1 = 0U;
    BOARD_FAST_RX_TIMER->CR2 = 0U;
    BOARD_FAST_RX_TIMER->SMCR = 0U;
    BOARD_FAST_RX_TIMER->DIER = 0U;
    BOARD_FAST_RX_TIMER->CCER = 0U;
    /* IC3 indirect input is TI4; IC4 direct input is also TI4. */
    BOARD_FAST_RX_TIMER->CCMR2 = TIM_CCMR2_CC3S_1 |
                                 TIM_CCMR2_CC4S_0;
    BOARD_FAST_RX_TIMER->PSC = 0U;
    BOARD_FAST_RX_TIMER->ARR = 0xFFFFU;
    BOARD_FAST_RX_TIMER->EGR = TIM_EGR_UG;
    BOARD_FAST_RX_TIMER->SR = 0U;
    BOARD_FAST_RX_TIMER->CNT = 0U;

    DMA1_Channel1->CCR = 0U;
    DMA1_Channel2->CCR = 0U;
    DMA1->IFCR = DMA_IFCR_CGIF1 | DMA_IFCR_CGIF2;
    MODIFY_REG(SYSCFG->CFGR3,
               SYSCFG_CFGR3_DMA1_MAP_Msk,
               DMA_CHANNEL_MAP_TIM1_CH3);
    MODIFY_REG(SYSCFG->CFGR3,
               SYSCFG_CFGR3_DMA2_MAP_Msk,
               DMA_CHANNEL_MAP_TIM1_CH4 << 8U);
    SET_BIT(SYSCFG->CFGR3, SYSCFG_CFGR3_DMA1_ACKLVL);
    SET_BIT(SYSCFG->CFGR3, SYSCFG_CFGR3_DMA2_ACKLVL);

    /* Channel 1: rising edges from IC3's indirect TI4 input. */
    DMA1_Channel1->CPAR = (uint32_t)(uintptr_t)&BOARD_FAST_RX_TIMER->CCR3;
    DMA1_Channel1->CMAR = (uint32_t)(uintptr_t)dma_rising_ring;
    DMA1_Channel1->CNDTR = DMA_CAPTURE_CAPACITY;
    DMA1_Channel1->CCR = DMA_CCR_MINC |
                         DMA_CCR_CIRC |
                         DMA_CCR_PSIZE_0 |
                         DMA_CCR_MSIZE_0 |
                         DMA_CCR_PL |
                         DMA_CCR_HTIE |
                         DMA_CCR_TCIE |
                         DMA_CCR_TEIE;

    /* Channel 2: falling edges from IC4's direct TI4 input. */
    DMA1_Channel2->CPAR = (uint32_t)(uintptr_t)&BOARD_FAST_RX_TIMER->CCR4;
    DMA1_Channel2->CMAR = (uint32_t)(uintptr_t)dma_falling_ring;
    DMA1_Channel2->CNDTR = DMA_CAPTURE_CAPACITY;
    DMA1_Channel2->CCR = DMA_CCR_MINC |
                         DMA_CCR_CIRC |
                         DMA_CCR_PSIZE_0 |
                         DMA_CCR_MSIZE_0 |
                         DMA_CCR_PL |
                         DMA_CCR_HTIE |
                         DMA_CCR_TCIE |
                         DMA_CCR_TEIE;

    HAL_NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_ClearPendingIRQ(DMA1_Channel2_3_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
    /* Custom-UART capture has the shortest hard deadline in the firmware.
       Run its bounded DMA drain above I2C so an I2C IRQ storm cannot postpone
       decoding beyond one complete edge ring. */
    HAL_NVIC_SetPriority(PendSV_IRQn, 0U, 0U);

    SET_BIT(DMA1_Channel1->CCR, DMA_CCR_EN);
    SET_BIT(DMA1_Channel2->CCR, DMA_CCR_EN);
    SET_BIT(BOARD_FAST_RX_TIMER->DIER, TIM_DIER_CC3DE | TIM_DIER_CC4DE);
    SET_BIT(BOARD_FAST_RX_TIMER->CCER,
            TIM_CCER_CC3E | TIM_CCER_CC4E | TIM_CCER_CC4P);
    SET_BIT(BOARD_FAST_RX_TIMER->CR1, TIM_CR1_CEN);
    wire_level_high =
        (BOARD_FAST_RX_PORT->IDR & BOARD_FAST_RX_PIN) != 0U;
    idle_high_since = (uint16_t)BOARD_FAST_RX_TIMER->CNT;
    idle_high_qualified = false;
    receiver_active = true;
    return true;
}

void uart_fast_rx_systick(void)
{
    if (receiver_active) {
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

void uart_fast_rx_pendsv(void)
{
    if (receiver_active) {
        uart_fast_rx_service();
    }
}

void uart_fast_rx_service(void)
{
    uint32_t rising_produced;
    uint32_t falling_produced;
    uint32_t rising_available;
    uint32_t falling_available;
    bool capture_fault = dma_fault_pending;
    uint16_t merge_now;

    ++decoder_service_count;

    dma_producer_totals(&rising_produced, &falling_produced);
    rising_available = rising_produced - dma_rising_consumer_total;
    falling_available = falling_produced - dma_falling_consumer_total;
    if (rising_available > dma_backlog_high_water) {
        dma_backlog_high_water = rising_available;
    }
    if (falling_available > dma_backlog_high_water) {
        dma_backlog_high_water = falling_available;
    }

    if ((BOARD_FAST_RX_TIMER->SR & (TIM_SR_CC3OF | TIM_SR_CC4OF)) != 0U) {
        BOARD_FAST_RX_TIMER->SR =
            (uint32_t)~(TIM_SR_CC3OF | TIM_SR_CC4OF);
        capture_fault = true;
        ++timer_overcapture_count;
    }

    if (dma_fault_pending) {
        ++dma_transfer_error_count;
    }

    if ((rising_available > DMA_CAPTURE_CAPACITY) ||
        (falling_available > DMA_CAPTURE_CAPACITY)) {
        ++dma_backlog_overflow_count;
        ++overflow_count;
        capture_fault = true;
    }

    if (capture_fault) {
        dma_fault_pending = false;
        ++error_count;
        dma_rising_consumer_total = rising_produced;
        dma_falling_consumer_total = falling_produced;
        dma_rising_consumer_index =
            (uint16_t)(rising_produced % DMA_CAPTURE_CAPACITY);
        dma_falling_consumer_index =
            (uint16_t)(falling_produced % DMA_CAPTURE_CAPACITY);
        /* Preserve already-decoded POST records for the main loop. Only the
           partial edge/frame state became unreliable. */
        stream_decoder_reset();
        wire_level_high =
            (BOARD_FAST_RX_PORT->IDR & BOARD_FAST_RX_PIN) != 0U;
        idle_high_since = (uint16_t)BOARD_FAST_RX_TIMER->CNT;
        return;
    }

    merge_now = (uint16_t)BOARD_FAST_RX_TIMER->CNT;
    while ((dma_rising_consumer_total != rising_produced) ||
           (dma_falling_consumer_total != falling_produced)) {
        bool have_rising = dma_rising_consumer_total != rising_produced;
        bool have_falling = dma_falling_consumer_total != falling_produced;
        uint16_t timestamp;
        uint8_t level_after;

        if (have_rising && have_falling) {
            uint16_t rising_timestamp =
                dma_rising_ring[dma_rising_consumer_index];
            uint16_t falling_timestamp =
                dma_falling_ring[dma_falling_consumer_index];
            uint16_t rising_age = timer_delta(merge_now, rising_timestamp);
            uint16_t falling_age = timer_delta(merge_now, falling_timestamp);

            /* The greater age is the older edge in this service snapshot. */
            if (rising_age >= falling_age) {
                timestamp = rising_timestamp;
                level_after = 1U;
                ++dma_rising_consumer_total;
                if (++dma_rising_consumer_index == DMA_CAPTURE_CAPACITY) {
                    dma_rising_consumer_index = 0U;
                }
            } else {
                timestamp = falling_timestamp;
                level_after = 0U;
                ++dma_falling_consumer_total;
                if (++dma_falling_consumer_index == DMA_CAPTURE_CAPACITY) {
                    dma_falling_consumer_index = 0U;
                }
            }
        } else if (have_rising) {
            timestamp = dma_rising_ring[dma_rising_consumer_index];
            level_after = 1U;
            ++dma_rising_consumer_total;
            if (++dma_rising_consumer_index == DMA_CAPTURE_CAPACITY) {
                dma_rising_consumer_index = 0U;
            }
        } else {
            timestamp = dma_falling_ring[dma_falling_consumer_index];
            level_after = 0U;
            ++dma_falling_consumer_total;
            if (++dma_falling_consumer_index == DMA_CAPTURE_CAPACITY) {
                dma_falling_consumer_index = 0U;
            }
        }

        stream_process_edge(timestamp, level_after);
    }

    stream_advance((uint16_t)BOARD_FAST_RX_TIMER->CNT);
}

bool uart_fast_rx_pop(uint64_t *raw)
{
    uint8_t tail = raw_tail;

    if ((tail == raw_head) || (raw == NULL)) {
        return false;
    }
    *raw = raw_ring[tail];
    raw_tail = (uint8_t)((tail + 1U) & RAW_RING_MASK);
    return true;
}

uint32_t uart_fast_rx_record_count(void)
{
    return record_count;
}

uint32_t uart_fast_rx_error_count(void)
{
    return error_count;
}

uint32_t uart_fast_rx_overflow_count(void)
{
    return overflow_count;
}

void uart_fast_rx_dma_irq_handler(void)
{
    dma_account_flags();
}
