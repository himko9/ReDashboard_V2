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

#include "oled_display.h"

#include <stdio.h>
#include <string.h>

#include "board_config.h"
#include "i2c_bus.h"
#include "p80_post_codes.h"

static constexpr int OLED_W = 128;
static constexpr int OLED_H = 64;
static constexpr int OLED_PAGES = OLED_H / 8;
static constexpr size_t OLED_PAGE_BYTES = OLED_W;
static constexpr int OLED_BAUD_SCALE = 3;
static constexpr int OLED_BAUD_GAP = 2;
static uint8_t oled_framebuffer[OLED_W * OLED_PAGES];
static uint8_t oled_sent_framebuffer[OLED_W * OLED_PAGES];
static uint8_t oled_sent_page_mask = 0U;

static int oled_shift_x = 0;
static int oled_shift_y = 0;
OledUiContext oled_ui = {
    "JDASH", 'G', true, false, false, false, 0U,
};

#if RP2040_OLED_ROTATION == 0U
static constexpr uint8_t OLED_SEGMENT_REMAP_COMMAND = 0xA0U;
static constexpr uint8_t OLED_COM_SCAN_COMMAND = 0xC0U;
#elif RP2040_OLED_ROTATION == 180U
static constexpr uint8_t OLED_SEGMENT_REMAP_COMMAND = 0xA1U;
static constexpr uint8_t OLED_COM_SCAN_COMMAND = 0xC8U;
#else
#error "RP2040_OLED_ROTATION must be 0 or 180"
#endif

static const uint8_t HEX_FONT_5X7[16][5] = {
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

static const uint8_t ALPHA_FONT_5X7[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x3A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t DECIMAL_POINT_5X7[5] = {0x00, 0x00, 0x60, 0x60, 0x00};
static const uint8_t MINUS_SIGN_5X7[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
static const uint8_t DEGREE_SIGN_5X7[5] = {0x06, 0x09, 0x09, 0x06, 0x00};

static bool oled_send_commands(const uint8_t *commands, size_t length) {
    uint8_t packet[17];
    packet[0] = 0x00U;
    size_t offset = 0U;
    while (offset < length) {
        size_t chunk = length - offset > 16U ? 16U : length - offset;
        memcpy(&packet[1], &commands[offset], chunk);
        if (!i2c_bus_write(OLED_BUS, OLED_I2C_ADDRESS,
                            packet, chunk + 1U)) {
            return false;
        }
        offset += chunk;
    }
    return true;
}

static bool oled_send_data(const uint8_t *data, size_t length) {
    /* One SSD1315 page per I2C transaction. The old 16-byte chunks turned
    * every frame into 64 data transactions and made page changes visibly
    * laggy even though the RP2040 and OLED bus were otherwise idle. */
    uint8_t packet[OLED_PAGE_BYTES + 1U];
    packet[0] = 0x40U;
    size_t offset = 0U;
    while (offset < length) {
        size_t chunk = length - offset > OLED_PAGE_BYTES
                           ? OLED_PAGE_BYTES
                           : length - offset;
        memcpy(&packet[1], &data[offset], chunk);
        if (!i2c_bus_write(OLED_BUS, OLED_I2C_ADDRESS,
                            packet, chunk + 1U)) {
            return false;
        }
        offset += chunk;
    }
    return true;
}

bool oled_init() {
    const uint8_t init_sequence[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        /* Page addressing makes each dirty page independently seekable with
        * B0..B7/column commands; this is required when unchanged pages are
        * skipped instead of always streaming all 1024 bytes in order. */
        0x8D, 0x14, 0x20, 0x02,
        OLED_SEGMENT_REMAP_COMMAND, OLED_COM_SCAN_COMMAND,
        0xDA, 0x12, 0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x40,
        0xA4, 0xA6, 0xAF};
    /* The controller may have reset while the MCU-side framebuffer shadow
    * remained intact, so the first following render must send every page. */
    oled_sent_page_mask = 0U;
    return oled_send_commands(init_sequence, sizeof(init_sequence));
}

bool oled_set_contrast(uint8_t value) {
    const uint8_t commands[] = {0x81U, value};
    return oled_send_commands(commands, sizeof(commands));
}

bool oled_set_power(bool on) {
    const uint8_t command = on ? 0xAFU : 0xAEU;
    return oled_send_commands(&command, 1U);
}

void oled_set_pixel_shift(int x, int y) {
    oled_shift_x = x;
    oled_shift_y = y;
}

static void clear_framebuffer() {
    memset(oled_framebuffer, 0, sizeof(oled_framebuffer));
}

static void set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    int page = y >> 3;
    uint8_t mask = (uint8_t)(1U << (y & 7));
    uint8_t &cell = oled_framebuffer[page * OLED_W + x];
    if (on) cell |= mask;
    else cell &= (uint8_t)~mask;
}

static void draw_hex_glyph_scaled(int x, int y, uint8_t nibble, int scale) {
    const uint8_t *glyph = HEX_FONT_5X7[nibble & 0x0FU];
    for (int column = 0; column < 5; ++column) {
        uint8_t bits = glyph[column];
        for (int row = 0; row < 7; ++row) {
            if (((bits >> row) & 1U) == 0U) continue;
            for (int sx = 0; sx < scale; ++sx) {
                for (int sy = 0; sy < scale; ++sy) {
                    set_pixel(x + column * scale + sx,
                              y + row * scale + sy, true);
                }
            }
        }
    }
}

static const uint8_t *font5x7_for_char(char character) {
    if (character >= '0' && character <= '9') {
        return HEX_FONT_5X7[character - '0'];
    }
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character >= 'A' && character <= 'Z') {
        return ALPHA_FONT_5X7[character - 'A'];
    }
    if (character == '.') return DECIMAL_POINT_5X7;
    if (character == '-') return MINUS_SIGN_5X7;
    if (character == '^') return DEGREE_SIGN_5X7;
    return nullptr;
}

static void draw_char_5x7_scaled(int x, int y, char character, int scale) {
    const uint8_t *glyph = font5x7_for_char(character);
    if (glyph == nullptr) return;
    for (int column = 0; column < 5; ++column) {
        uint8_t bits = glyph[column];
        for (int row = 0; row < 7; ++row) {
            if (((bits >> row) & 1U) == 0U) continue;
            for (int sx = 0; sx < scale; ++sx) {
                for (int sy = 0; sy < scale; ++sy) {
                    set_pixel(x + column * scale + sx,
                              y + row * scale + sy, true);
                }
            }
        }
    }
}

static void draw_text(int x, int y, const char *text,
                      int scale, int spacing) {
    if (text == nullptr) return;
    int cursor = x;
    while (*text != '\0') {
        char character = *text++;
        if (character != ' ') {
            draw_char_5x7_scaled(cursor, y, character, scale);
        }
        cursor += 5 * scale + spacing;
    }
}

static void draw_centered_text(int y, const char *text) {
    if (text == nullptr || *text == '\0') return;
    size_t length = strnlen(text, P80_POST_LABEL_MAX_CHARS);
    int width = (int)length * 6 - 1;
    draw_text((OLED_W - width) / 2 + oled_shift_x,
              y + oled_shift_y, text, 1, 1);
}

static void draw_scaled_centered_text(int y, const char *text, int scale) {
    if (text == nullptr || *text == '\0') return;
    int width = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        int glyph_scale = (*cursor == '^' && scale > 1) ? scale - 1 : scale;
        width += 5 * glyph_scale;
        if (cursor[1] != '\0') width += scale;
    }
    int x = (OLED_W - width) / 2 + oled_shift_x;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        int glyph_scale = (*cursor == '^' && scale > 1) ? scale - 1 : scale;
        draw_char_5x7_scaled(x, y + oled_shift_y, *cursor, glyph_scale);
        x += 5 * glyph_scale + scale;
    }
}

static void draw_mode_tag() {
    if (!oled_ui.top_bar_visible) return;
    draw_text(2 + oled_shift_x, 1 + oled_shift_y,
              oled_ui.mode_tag, 1, 1);
}

static void draw_nav_tag() {
    if (!oled_ui.top_bar_visible) return;
    char text[2] = {oled_ui.nav_tag, '\0'};
    draw_text(OLED_W - 7 + oled_shift_x,
              1 + oled_shift_y, text, 1, 1);
}

static void draw_lock_tag() {
    if (!oled_ui.top_bar_visible || !oled_ui.mode_locked) return;
    static const uint8_t lock_icon[7] = {
        0b01110, 0b10001, 0b10001, 0b11111,
        0b10001, 0b10101, 0b11111};
    int x0 = (OLED_W - 5) / 2 + oled_shift_x;
    int y0 = 1 + oled_shift_y;
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 5; ++x) {
            if ((lock_icon[y] >> (4 - x)) & 1U) {
                set_pixel(x0 + x, y0 + y, true);
            }
        }
    }
}

static bool flush_framebuffer() {
    for (int page = 0; page < OLED_PAGES; ++page) {
        const uint8_t page_bit = (uint8_t)(1U << page);
        uint8_t *page_data = &oled_framebuffer[page * OLED_W];
        uint8_t *sent_page_data = &oled_sent_framebuffer[page * OLED_W];
        if ((oled_sent_page_mask & page_bit) != 0U &&
            memcmp(page_data, sent_page_data, OLED_PAGE_BYTES) == 0) {
            continue;
        }
        const uint8_t commands[] = {
            (uint8_t)(0xB0 | page), 0x00U, 0x10U};
        if (!oled_send_commands(commands, sizeof(commands)) ||
            !oled_send_data(page_data, OLED_PAGE_BYTES)) {
            return false;
        }
        memcpy(sent_page_data, page_data, OLED_PAGE_BYTES);
        oled_sent_page_mask |= page_bit;
    }
    return true;
}

bool oled_show_baud_value(uint32_t baud) {
    char text[12];
    int count = snprintf(text, sizeof(text), "%lu", (unsigned long)baud);
    if (count <= 0) return false;
    if (count > (int)sizeof(text) - 1) count = (int)sizeof(text) - 1;
    size_t length = (size_t)count;
    if (length == 0U) return false;

    clear_framebuffer();
    const int glyph_width = 5 * OLED_BAUD_SCALE;
    const int glyph_height = 7 * OLED_BAUD_SCALE;
    const int total_width =
        (int)length * glyph_width + ((int)length - 1) * OLED_BAUD_GAP;
    int x0 = (OLED_W - total_width) / 2 + oled_shift_x;
    int y0 = (OLED_H - glyph_height) / 2 + oled_shift_y;
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] >= '0' && text[index] <= '9') {
            draw_hex_glyph_scaled(
                x0 + (int)index * (glyph_width + OLED_BAUD_GAP), y0,
                (uint8_t)(text[index] - '0'), OLED_BAUD_SCALE);
        }
    }
    draw_mode_tag();
    draw_nav_tag();
    draw_lock_tag();
    return flush_framebuffer();
}

static void draw_scrollbar(uint32_t oldest,
                           uint32_t newest,
                           uint32_t cursor) {
    const int track_x = OLED_W - 2;
    const int track_y = 2;
    const int track_height = OLED_H - 4;
    const int thumb_height = 10;
    const int travel = track_height - thumb_height;
    for (int y = 0; y < track_height; ++y) {
        set_pixel(track_x, track_y + y, true);
        set_pixel(track_x + 1, track_y + y, false);
    }
    int thumb_y = track_y;
    if (newest > oldest && cursor >= oldest && cursor <= newest) {
        uint32_t numerator = cursor - oldest;
        uint32_t denominator = newest - oldest;
        thumb_y = track_y +
                  (int)((numerator * (uint32_t)travel + denominator / 2U) /
                        denominator);
    }
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < thumb_height; ++y) {
            set_pixel(track_x + x, thumb_y + y, true);
        }
    }
}

static bool show_code(PostCode code,
                      bool show_scroll,
                      uint32_t oldest,
                      uint32_t newest,
                      uint32_t cursor) {
    clear_framebuffer();
    bool stored_pair = code.width_bytes == 2U;
    bool show_pair = stored_pair ||
                     (!show_scroll && !oled_ui.sio_comm_error &&
                      oled_ui.port81_visible && code.width_bytes == 1U);
    uint16_t display_code = stored_pair
                                ? code.value
                                : (show_pair
                                       ? (uint16_t)((code.value << 8U) |
                                                    oled_ui.port81_code)
                                       : code.value);
    const int digits = show_pair ? 4 : 2;
    const int scale = show_pair ? 4 : 5;
    const int glyph_width = 5 * scale;
    const int gap = show_pair ? 4 : 8;
    const int total_width = glyph_width * digits + gap * (digits - 1);
    const int x0 = (OLED_W - total_width) / 2 + oled_shift_x;
    const int y0 = (show_pair ? 14 : 11) + oled_shift_y;
    for (int digit = 0; digit < digits; ++digit) {
        int shift = (digits - 1 - digit) * 4;
        draw_hex_glyph_scaled(x0 + digit * (glyph_width + gap), y0,
                              (uint8_t)((display_code >> shift) & 0x0FU),
                              scale);
    }
    if (show_scroll) draw_scrollbar(oldest, newest, cursor);
    if (oled_ui.sio_comm_error && code.value == 0U &&
        code.width_bytes == 1U && !show_scroll) {
        draw_centered_text(54, "SIO COMM ERROR");
    } else if (!show_pair) {
        draw_centered_text(54, p80_post_code_label((uint8_t)code.value));
    }
    draw_mode_tag();
    draw_nav_tag();
    draw_lock_tag();
    return flush_framebuffer();
}

bool oled_show_sensor_value(const char *value,
                            const char *label,
                            char role_tag) {
    clear_framebuffer();
    draw_scaled_centered_text(18, value, 3);
    draw_centered_text(54, label);
    draw_mode_tag();
    char previous_nav_tag = oled_ui.nav_tag;
    oled_ui.nav_tag = role_tag;
    draw_nav_tag();
    oled_ui.nav_tag = previous_nav_tag;
    draw_lock_tag();
    return flush_framebuffer();
}

bool oled_render_code(bool oled_ok,
                      bool *panel_on,
                      absolute_time_t *last_activity,
                      PostCode code,
                      bool show_scroll,
                      uint32_t oldest,
                      uint32_t newest,
                      uint32_t cursor,
                      bool mark_activity) {
    if (!oled_ok) return false;
    if (mark_activity) {
        oled_ui.top_bar_visible = true;
    }
    if (!*panel_on) {
        oled_ok = oled_set_power(true);
        if (!oled_ok) return false;
        *panel_on = true;
    }
    bool draw_ok = show_code(code, show_scroll, oldest, newest, cursor);
    if (draw_ok && mark_activity) *last_activity = get_absolute_time();
    return draw_ok;
}
