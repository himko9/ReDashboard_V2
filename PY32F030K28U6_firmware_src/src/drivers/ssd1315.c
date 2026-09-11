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

#include "ssd1315.h"

#include <stddef.h>

#include "board.h"
#include "soft_i2c.h"

#define OLED_ADDRESS       0x3CU
#define OLED_WIDTH         128U
#define OLED_HEIGHT        64U
#define OLED_PAGE_COUNT    (OLED_HEIGHT / 8U)
#define OLED_DATA_CHUNK    16U
#define OLED_ALL_PAGES     0xFFU
#define OLED_MEANING_MAX_CHARS 21U

#if BOARD_OLED_ROTATION == 0U
#define OLED_SEGMENT_REMAP_COMMAND 0xA0U
#define OLED_COM_SCAN_COMMAND      0xC0U
#elif BOARD_OLED_ROTATION == 180U
#define OLED_SEGMENT_REMAP_COMMAND 0xA1U
#define OLED_COM_SCAN_COMMAND      0xC8U
#else
#error "BOARD_OLED_ROTATION must be 0 or 180"
#endif

static const uint8_t hex_font_5x7[16][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
};

static const uint8_t alpha_font_5x7[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* A */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* C */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* E */
    {0x7F, 0x09, 0x09, 0x09, 0x01}, /* F */
    {0x3E, 0x41, 0x49, 0x49, 0x3A}, /* G */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* I */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* K */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* L */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, /* M */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* O */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* Q */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* S */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* U */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* V */
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, /* W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* X */
    {0x07, 0x08, 0x70, 0x08, 0x07}, /* Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* Z */
};

static const uint8_t decimal_point_5x7[5] = {
    0x00, 0x00, 0x60, 0x60, 0x00,
};

static const uint8_t minus_sign_5x7[5] = {
    0x08, 0x08, 0x08, 0x08, 0x08,
};

static const uint8_t degree_sign_5x7[5] = {
    0x06, 0x09, 0x09, 0x06, 0x00,
};

static uint8_t framebuffer[OLED_WIDTH * OLED_PAGE_COUNT];
static bool online;
static uint8_t dirty_pages;
static uint8_t flush_page;
static uint8_t flush_offset;
static bool page_address_sent;
static uint8_t shift_index;
static int8_t shift_x;
static int8_t shift_y;

static bool send_commands(const uint8_t *commands, size_t length)
{
    if (!online || !soft_i2c_write(OLED_ADDRESS, 0x00U, commands, length)) {
        online = false;
        return false;
    }
    return true;
}

static void clear_framebuffer(void)
{
    size_t index;
    for (index = 0U; index < sizeof(framebuffer); ++index) {
        framebuffer[index] = 0U;
    }
}

static void set_pixel(int16_t x, int16_t y)
{
    size_t index;

    if ((x < 0) || (x >= (int16_t)OLED_WIDTH) ||
        (y < 0) || (y >= (int16_t)OLED_HEIGHT)) {
        return;
    }
    index = (size_t)((uint16_t)y >> 3U) * OLED_WIDTH + (uint16_t)x;
    framebuffer[index] |= (uint8_t)(1U << ((uint16_t)y & 7U));
}

static const uint8_t *font_for_character(char character)
{
    if ((character >= '0') && (character <= '9')) {
        return hex_font_5x7[(uint8_t)(character - '0')];
    }
    if ((character >= 'A') && (character <= 'Z')) {
        return alpha_font_5x7[(uint8_t)(character - 'A')];
    }
    if (character == '.') {
        return decimal_point_5x7;
    }
    if (character == '-') {
        return minus_sign_5x7;
    }
    if (character == '^') {
        return degree_sign_5x7;
    }
    return NULL;
}

static void draw_glyph(int16_t x, int16_t y, const uint8_t *glyph, uint8_t scale)
{
    uint8_t column;
    uint8_t row;
    uint8_t dx;
    uint8_t dy;

    if (glyph == NULL) {
        return;
    }
    for (column = 0U; column < 5U; ++column) {
        for (row = 0U; row < 7U; ++row) {
            if (((glyph[column] >> row) & 1U) == 0U) {
                continue;
            }
            for (dx = 0U; dx < scale; ++dx) {
                for (dy = 0U; dy < scale; ++dy) {
                    set_pixel((int16_t)(x + (int16_t)column * scale + dx),
                              (int16_t)(y + (int16_t)row * scale + dy));
                }
            }
        }
    }
}

static void draw_text(int16_t x, int16_t y, const char *text)
{
    while ((text != NULL) && (*text != '\0')) {
        draw_glyph(x, y, font_for_character(*text), 1U);
        x = (int16_t)(x + 6);
        ++text;
    }
}

static void draw_centered_text(int16_t y, const char *text)
{
    uint8_t length = 0U;
    int16_t width;
    int16_t x;

    while ((text != NULL) && (text[length] != '\0') &&
           (length < OLED_MEANING_MAX_CHARS)) {
        ++length;
    }
    if (length == 0U) {
        return;
    }
    width = (int16_t)((int16_t)length * 6 - 1);
    x = (int16_t)(((int16_t)OLED_WIDTH - width) / 2 + shift_x);
    draw_text(x, (int16_t)(y + shift_y), text);
}

static void draw_scaled_centered_text(int16_t y, const char *text, uint8_t scale)
{
    int16_t width;
    int16_t x;
    const char *cursor;

    if ((text == NULL) || (*text == '\0')) {
        return;
    }

    width = 0;
    cursor = text;
    while (*cursor != '\0') {
        uint8_t glyph_scale = ((*cursor == '^') && (scale > 1U))
                                  ? (uint8_t)(scale - 1U)
                                  : scale;

        width = (int16_t)(width + (int16_t)(5U * glyph_scale));
        ++cursor;
        if (*cursor != '\0') {
            width = (int16_t)(width + scale);
        }
    }

    x = (int16_t)(((int16_t)OLED_WIDTH - width) / 2 + shift_x);
    while (*text != '\0') {
        uint8_t glyph_scale = ((*text == '^') && (scale > 1U))
                                  ? (uint8_t)(scale - 1U)
                                  : scale;

        draw_glyph(x,
                   (int16_t)(y + shift_y),
                   font_for_character(*text),
                   glyph_scale);
        x = (int16_t)(x + (int16_t)(5U * glyph_scale) + scale);
        ++text;
    }
}

static void draw_scrollbar(uint32_t oldest, uint32_t newest, uint32_t cursor)
{
    const int16_t track_x = 126;
    const int16_t track_y = 2;
    const int16_t track_height = 60;
    const int16_t thumb_height = 10;
    int16_t thumb_y = track_y;
    int16_t y;
    int16_t x;

    for (y = 0; y < track_height; ++y) {
        set_pixel(track_x, (int16_t)(track_y + y));
    }
    if ((newest > oldest) && (cursor >= oldest) && (cursor <= newest)) {
        uint32_t numerator = cursor - oldest;
        uint32_t denominator = newest - oldest;
        uint32_t travel = (uint32_t)(track_height - thumb_height);
        thumb_y = (int16_t)(track_y +
                            (int16_t)((numerator * travel + denominator / 2U) / denominator));
    }
    for (x = 0; x < 2; ++x) {
        for (y = 0; y < thumb_height; ++y) {
            set_pixel((int16_t)(track_x + x), (int16_t)(thumb_y + y));
        }
    }
}

static void draw_lock(void)
{
    static const uint8_t rows[7] = {
        0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x15U, 0x1FU,
    };
    int16_t x;
    int16_t y;

    for (y = 0; y < 7; ++y) {
        for (x = 0; x < 5; ++x) {
            if (((rows[y] >> (4 - x)) & 1U) != 0U) {
                set_pixel((int16_t)(61 + x + shift_x), (int16_t)(1 + y + shift_y));
            }
        }
    }
}

static void request_full_flush(void)
{
    dirty_pages = OLED_ALL_PAGES;
    flush_page = 0U;
    flush_offset = 0U;
    page_address_sent = false;
}

bool ssd1315_init(void)
{
    static const uint8_t init_sequence[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x02,
        OLED_SEGMENT_REMAP_COMMAND, OLED_COM_SCAN_COMMAND, 0xDA, 0x12,
        0x81, 0xFF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,
        0xAF,
    };

    soft_i2c_init();
    online = true;
    dirty_pages = 0U;
    shift_index = 0U;
    shift_x = 0;
    shift_y = 0;
    if (!send_commands(init_sequence, sizeof(init_sequence))) {
        return false;
    }
    clear_framebuffer();
    request_full_flush();
    return true;
}

bool ssd1315_is_online(void)
{
    return online;
}

bool ssd1315_is_busy(void)
{
    return online && (dirty_pages != 0U);
}

static void draw_context_chrome(const DisplayContext *context)
{
    draw_centered_text(54, context->meaning);
    if (context->show_top_bar) {
        draw_text((int16_t)(2 + shift_x),
                  (int16_t)(1 + shift_y),
                  context->source == DISPLAY_SOURCE_UART ? "UART" : "DASH");
        {
            char role[2] = {context->role_tag, '\0'};
            draw_text((int16_t)(120 + shift_x),
                      (int16_t)(1 + shift_y), role);
        }
        if (context->locked) {
            draw_lock();
        }
    }
    if (context->show_scrollbar) {
        draw_scrollbar(context->oldest, context->newest, context->cursor);
    }
}

void ssd1315_render_code(uint32_t code,
                         uint8_t width_bytes,
                         const DisplayContext *context)
{
    uint8_t digits;
    uint8_t scale;
    int16_t glyph_width;
    int16_t digit_gap;
    int16_t byte_gap;
    int16_t total_width;
    int16_t x;
    int16_t y;
    uint8_t digit;

    if (!online || (context == NULL) ||
        ((width_bytes != 1U) && (width_bytes != 2U) &&
         (width_bytes != 4U))) {
        return;
    }

    digits = (uint8_t)(width_bytes * 2U);
    if (width_bytes == 1U) {
        scale = 5U;
        digit_gap = 8;
        byte_gap = 0;
        y = (int16_t)(11 + shift_y);
    } else if (width_bytes == 2U) {
        scale = 4U;
        digit_gap = 4;
        byte_gap = 0;
        y = (int16_t)(14 + shift_y);
    } else {
        scale = 2U;
        digit_gap = 2;
        byte_gap = 3;
        y = (int16_t)(20 + shift_y);
    }
    glyph_width = (int16_t)(5U * scale);
    total_width = (int16_t)((int16_t)digits * glyph_width +
                            (int16_t)(digits - 1U) * digit_gap +
                            (int16_t)(width_bytes - 1U) * byte_gap);
    x = (int16_t)(((int16_t)OLED_WIDTH - total_width) / 2 + shift_x);

    clear_framebuffer();
    for (digit = 0U; digit < digits; ++digit) {
        uint8_t shift = (uint8_t)((digits - 1U - digit) * 4U);

        draw_glyph(x, y, hex_font_5x7[(code >> shift) & 0x0FU], scale);
        x = (int16_t)(x + glyph_width + digit_gap);
        if (((digit & 1U) != 0U) && ((digit + 1U) < digits)) {
            x = (int16_t)(x + byte_gap);
        }
    }
    draw_context_chrome(context);
    request_full_flush();
}

void ssd1315_render_post_pair(uint8_t port80,
                              uint8_t port81,
                              const DisplayContext *context)
{
    uint16_t combined = (uint16_t)(((uint16_t)port80 << 8U) | port81);

    ssd1315_render_code(combined, 2U, context);
}

void ssd1315_render_value(const char *value, const DisplayContext *context)
{
    if (!online || (value == NULL) || (context == NULL)) {
        return;
    }

    clear_framebuffer();
    draw_scaled_centered_text(18, value, 3U);
    draw_context_chrome(context);
    request_full_flush();
}

void ssd1315_service(void)
{
    uint8_t commands[3];
    uint8_t remaining;
    uint8_t chunk;

    if (!online || (dirty_pages == 0U)) {
        return;
    }

    while ((flush_page < OLED_PAGE_COUNT) &&
           ((dirty_pages & (uint8_t)(1U << flush_page)) == 0U)) {
        ++flush_page;
        flush_offset = 0U;
        page_address_sent = false;
    }
    if (flush_page >= OLED_PAGE_COUNT) {
        dirty_pages = 0U;
        return;
    }

    if (!page_address_sent) {
        commands[0] = (uint8_t)(0xB0U | flush_page);
        commands[1] = 0x00U;
        commands[2] = 0x10U;
        if (send_commands(commands, sizeof(commands))) {
            page_address_sent = true;
        }
        return;
    }

    remaining = (uint8_t)(OLED_WIDTH - flush_offset);
    chunk = remaining > OLED_DATA_CHUNK ? OLED_DATA_CHUNK : remaining;
    if (!soft_i2c_write(OLED_ADDRESS,
                        0x40U,
                        &framebuffer[(size_t)flush_page * OLED_WIDTH + flush_offset],
                        chunk)) {
        online = false;
        return;
    }

    flush_offset = (uint8_t)(flush_offset + chunk);
    if (flush_offset >= OLED_WIDTH) {
        dirty_pages &= (uint8_t)~(uint8_t)(1U << flush_page);
        ++flush_page;
        flush_offset = 0U;
        page_address_sent = false;
    }
}

void ssd1315_cycle_shift(uint8_t pixels)
{
    static const int8_t shifts_x[4] = {0, 1, 0, -1};
    static const int8_t shifts_y[4] = {0, 0, 1, 0};
    int8_t distance = (int8_t)pixels;

    shift_index = (uint8_t)((shift_index + 1U) & 3U);
    shift_x = (int8_t)(shifts_x[shift_index] * distance);
    shift_y = (int8_t)(shifts_y[shift_index] * distance);
}

bool ssd1315_set_contrast(uint8_t contrast)
{
    const uint8_t commands[2] = {0x81U, contrast};
    return send_commands(commands, sizeof(commands));
}

bool ssd1315_set_power(bool enabled)
{
    const uint8_t command = enabled ? 0xAFU : 0xAEU;
    return send_commands(&command, 1U);
}
