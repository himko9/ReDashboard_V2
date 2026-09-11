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

#include "post_uart.h"

#include "board_config.h"
#include "post_uart_rx.pio.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/regs/uart.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

static constexpr uint32_t FAST_DMA_WORD_COUNT = 4096U;
static constexpr uint32_t FAST_DMA_WORD_MASK = FAST_DMA_WORD_COUNT - 1U;
static constexpr uint32_t DMA_TRANSFER_COUNT = 0xFFFFFFFFU;
static_assert((FAST_DMA_WORD_COUNT & FAST_DMA_WORD_MASK) == 0U,
              "fast UART DMA ring size must be a power of two");
alignas(16384) static volatile uint32_t fast_dma_words[FAST_DMA_WORD_COUNT];
static int fast_dma_channel = -1;
static uint32_t fast_dma_consumed = 0U;

static constexpr uint32_t UART_DMA_WORD_COUNT = 2048U;
static constexpr uint32_t UART_DMA_WORD_MASK = UART_DMA_WORD_COUNT - 1U;
static_assert((UART_DMA_WORD_COUNT & UART_DMA_WORD_MASK) == 0U,
              "UART DMA ring size must be a power of two");
/* PL011 UARTDR bits 8..11 carry FE/PE/BE/OE for the associated byte. DMA must
 * retain them; an 8-bit DMA transfer silently turned wrong-baud traffic into
 * apparently valid 00/01 data. */
alignas(4096) static volatile uint16_t uart_dma_words[UART_DMA_WORD_COUNT];
static int uart_dma_channel = -1;
static uint32_t uart_dma_consumed = 0U;
static constexpr uint32_t UART_ERROR_MASK =
    UART_UARTDR_FE_BITS | UART_UARTDR_PE_BITS |
    UART_UARTDR_BE_BITS | UART_UARTDR_OE_BITS;
static constexpr uint32_t UART_CANDIDATE_COUNT = 2048U;
static constexpr uint32_t UART_CANDIDATE_MASK = UART_CANDIDATE_COUNT - 1U;
static_assert((UART_CANDIDATE_COUNT & UART_CANDIDATE_MASK) == 0U,
              "UART candidate queue size must be a power of two");
static uint8_t uart_candidates[UART_CANDIDATE_COUNT];
static uint32_t uart_candidate_head = 0U;
static uint32_t uart_candidate_tail = 0U;
static bool uart_error_guard_active = false;
static absolute_time_t uart_error_guard_until;

bool fast_uart_init() {
    PIO pio = pio0;
    uint state_machine = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &post_uart_rx_program);

    pio_gpio_init(pio, RP2040_FAST_UART_RX_PIN);
    gpio_pull_up(RP2040_FAST_UART_RX_PIN);
    pio_sm_config config = post_uart_rx_program_get_default_config(offset);
    sm_config_set_in_pins(&config, RP2040_FAST_UART_RX_PIN);
    sm_config_set_jmp_pin(&config, RP2040_FAST_UART_RX_PIN);
    sm_config_set_in_shift(&config, true, false, 32U);
    float divider = (float)clock_get_hz(clk_sys) /
                    (8.0f * (float)RP2040_FAST_UART_BAUD);
    sm_config_set_clkdiv(&config, divider);
    pio_sm_init(pio, state_machine, offset, &config);
    pio_sm_set_enabled(pio, state_machine, true);

    fast_dma_channel = dma_claim_unused_channel(true);
    dma_channel_config dma_config =
        dma_channel_get_default_config((uint)fast_dma_channel);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_dreq(
        &dma_config, pio_get_dreq(pio, state_machine, false));
    channel_config_set_ring(&dma_config, true, 14U);
    dma_channel_configure((uint)fast_dma_channel,
                          &dma_config,
                          (void *)fast_dma_words,
                          &pio->rxf[state_machine],
                          DMA_TRANSFER_COUNT,
                          true);
    fast_dma_consumed = 0U;
    return true;
}

bool fast_uart_pop(FastUartRecord *record) {
    if (fast_dma_channel < 0 || record == nullptr) return false;
    uint32_t remaining =
        dma_channel_hw_addr((uint)fast_dma_channel)->transfer_count;
    uint32_t produced = DMA_TRANSFER_COUNT - remaining;
    uint32_t available = produced - fast_dma_consumed;
    if (available > FAST_DMA_WORD_COUNT) {
        fast_dma_consumed = produced - FAST_DMA_WORD_COUNT;
        if ((fast_dma_consumed & 1U) != 0U) ++fast_dma_consumed;
        available = produced - fast_dma_consumed;
    }
    if (available < 2U) return false;

    uint32_t header_word =
        fast_dma_words[fast_dma_consumed & FAST_DMA_WORD_MASK];
    uint32_t payload_word =
        fast_dma_words[(fast_dma_consumed + 1U) & FAST_DMA_WORD_MASK];
    fast_dma_consumed += 2U;

    uint32_t header = header_word >> 14U;
    record->port = (uint16_t)header;
    record->width = (uint8_t)((header >> 16U) & 0x03U);
    if (record->width == 0U) {
        record->payload = payload_word >> 24U;
    } else if (record->width == 1U) {
        record->payload = payload_word >> 16U;
    } else {
        return false;
    }
    return true;
}

bool uart_dma_init() {
    uart_dma_channel = dma_claim_unused_channel(true);
    dma_channel_config config =
        dma_channel_get_default_config((uint)uart_dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, uart_get_dreq(UART_IN_INST, false));
    channel_config_set_ring(&config, true, 12U);
    dma_channel_configure((uint)uart_dma_channel,
                          &config,
                          (void *)uart_dma_words,
                          &uart_get_hw(UART_IN_INST)->dr,
                          DMA_TRANSFER_COUNT,
                          true);
    uart_dma_consumed = 0U;
    uart_candidate_head = 0U;
    uart_candidate_tail = 0U;
    uart_error_guard_active = false;
    uart_error_guard_until = get_absolute_time();
    return true;
}

static void uart_candidate_clear() {
    uart_candidate_tail = uart_candidate_head;
}

static void uart_candidate_push(uint8_t byte) {
    uint32_t next = (uart_candidate_head + 1U) & UART_CANDIDATE_MASK;
    if (next == uart_candidate_tail) {
        uart_candidate_tail =
            (uart_candidate_tail + 1U) & UART_CANDIDATE_MASK;
    }
    uart_candidates[uart_candidate_head] = byte;
    uart_candidate_head = next;
}

static bool uart_dma_drain() {
    if (uart_dma_channel < 0) return false;
    uint32_t remaining =
        dma_channel_hw_addr((uint)uart_dma_channel)->transfer_count;
    uint32_t produced = DMA_TRANSFER_COUNT - remaining;
    if (produced - uart_dma_consumed > UART_DMA_WORD_COUNT) {
        uart_dma_consumed = produced - UART_DMA_WORD_COUNT;
        uart_candidate_clear();
    }
    if (uart_dma_consumed == produced) return true;

    /* Classify the complete snapshot before exposing any byte. A wrong-baud
    * burst often contains a few clean-looking samples before its first FE;
    * scanning the batch first prevents those samples from escaping. */
    bool has_error = false;
    for (uint32_t cursor = uart_dma_consumed; cursor != produced; ++cursor) {
        uint16_t word = uart_dma_words[cursor & UART_DMA_WORD_MASK];
        if ((word & UART_ERROR_MASK) != 0U) {
            has_error = true;
            break;
        }
    }

    if (has_error) {
        uart_dma_consumed = produced;
        uart_candidate_clear();
        uart_error_guard_active = true;
        uart_error_guard_until =
            make_timeout_time_ms(UART_CROSS_BAUD_ERROR_GUARD_MS);
        /* Clear the PL011 sticky receive-error register after retaining the
        * per-byte UARTDR status above. */
        uart_get_hw(UART_IN_INST)->rsr = 0U;
        return true;
    }

    if (uart_error_guard_active) {
        if (!time_reached(uart_error_guard_until)) {
            uart_dma_consumed = produced;
            return true;
        }
        uart_error_guard_active = false;
    }

    while (uart_dma_consumed != produced) {
        uint16_t word =
            uart_dma_words[uart_dma_consumed & UART_DMA_WORD_MASK];
        ++uart_dma_consumed;
        uart_candidate_push((uint8_t)word);
    }
    return true;
}

bool uart_dma_pop(uint8_t *byte_out) {
    if (byte_out == nullptr || !uart_dma_drain()) return false;
    if (uart_candidate_tail == uart_candidate_head) return false;
    *byte_out = uart_candidates[uart_candidate_tail];
    uart_candidate_tail =
        (uart_candidate_tail + 1U) & UART_CANDIDATE_MASK;
    return true;
}
