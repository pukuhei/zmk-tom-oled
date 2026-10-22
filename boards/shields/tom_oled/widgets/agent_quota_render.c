/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Full-screen AI quota layout "B" from docs/design/oled-quota-128x32 in LiTom:
 * source and window label on top, a 2x remaining percentage at the top right,
 * a ten-segment meter (fill = remaining, outline = used) and the secondary
 * window plus reset countdown at the bottom. Unknown data hides the meter and
 * shows NO DATA so it never reads as 0%.
 */

#include "agent_quota_render.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define W TOM_OLED_QUOTA_FRAME_WIDTH
#define H TOM_OLED_QUOTA_FRAME_HEIGHT

#define METER_COUNT 10
#define METER_X 3
#define METER_Y 13
#define METER_W 11
#define METER_H 7
#define METER_GAP 1
#define METER_INTERIOR ((METER_W - 2) * (METER_H - 2))

/* 3x5 glyphs from KYUREN quota-frame.ts, bit 14 = top-left, row-major. */
struct glyph {
    char ch;
    uint16_t bits;
};

#define G(c, s) {c, s}
static const struct glyph glyphs[] = {
    G('0', 0x7B6F), G('1', 0x2C97), G('2', 0x73E7), G('3', 0x73CF), G('4', 0x5BC9),
    G('5', 0x79CF), G('6', 0x79EF), G('7', 0x7292), G('8', 0x7BEF), G('9', 0x7BCF),
    G('A', 0x2BED), G('C', 0x7927), G('D', 0x6B6E), G('E', 0x79A7), G('F', 0x79A4),
    G('H', 0x5BED), G('L', 0x4927), G('M', 0x5FED), G('N', 0x5FFD), G('O', 0x7B6F),
    G('R', 0x6BAD), G('S', 0x79CF), G('T', 0x7492), G('U', 0x5B6F), G('X', 0x5AAD),
    G('%', 0x52A5), G('-', 0x01C0), G(':', 0x0410), G(' ', 0x0000), G('?', 0x7282),
};

static uint16_t glyph_bits(char ch) {
    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        if (glyphs[i].ch == ch) {
            return glyphs[i].bits;
        }
    }
    return glyph_bits('?');
}

static void set_px(uint8_t *frame, int x, int y) {
    /* The outer 1 px ring may be cut off by the panel, so it always stays dark. */
    if (x >= 1 && x < W - 1 && y >= 1 && y < H - 1) {
        frame[(y * W + x) >> 3] |= 0x80 >> (x & 7);
    }
}

static int text_width(const char *s, int scale) {
    int len = (int)strlen(s);
    return len == 0 ? 0 : (len * 4 - 1) * scale;
}

static void text(uint8_t *frame, const char *s, int x, int y, int scale) {
    for (; *s != '\0'; s++, x += 4 * scale) {
        uint16_t bits = glyph_bits(*s);
        for (int i = 0; i < 15; i++) {
            if ((bits & (1 << (14 - i))) == 0) {
                continue;
            }
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    set_px(frame, x + (i % 3) * scale + dx, y + (i / 3) * scale + dy);
                }
            }
        }
    }
}

static void percent(char *out, size_t size, uint8_t value) {
    if (value > 100) {
        snprintf(out, size, "--%%");
    } else {
        snprintf(out, size, "%u%%", value);
    }
}

/* KYUREN windowLabel(): whole days, then whole hours, then minutes. */
static void window_label(char *out, size_t size, uint16_t minutes) {
    if (minutes == 0) {
        snprintf(out, size, "--");
    } else if (minutes % 1440 == 0) {
        snprintf(out, size, "%uD", minutes / 1440);
    } else if (minutes % 60 == 0) {
        snprintf(out, size, "%uH", minutes / 60);
    } else {
        snprintf(out, size, "%uM", minutes);
    }
}

static void reset_label(char *out, size_t size, int32_t minutes) {
    if (minutes < 0) {
        snprintf(out, size, "RST --:--");
    } else if (minutes >= 6000) {
        snprintf(out, size, "RST %dD", (int)(minutes / 1440));
    } else {
        snprintf(out, size, "RST %d:%02d", (int)(minutes / 60), (int)(minutes % 60));
    }
}

static void meter(uint8_t *frame, uint8_t remaining) {
    for (int index = 0; index < METER_COUNT; index++) {
        int left = METER_X + index * (METER_W + METER_GAP);
        int right = left + METER_W - 1;
        int top = METER_Y;
        int bottom = METER_Y + METER_H - 1;

        for (int x = left; x <= right; x++) {
            set_px(frame, x, top);
            set_px(frame, x, bottom);
        }
        for (int y = top; y <= bottom; y++) {
            set_px(frame, left, y);
            set_px(frame, right, y);
        }

        /* Segment share in 1/100ths: remaining * 10 - index * 100, clamped to 0..100. */
        int share = remaining * METER_COUNT - index * 100;
        share = share < 0 ? 0 : share > 100 ? 100 : share;
        /* Round half up like KYUREN's Math.round(). */
        int count = (METER_INTERIOR * share * 2 + 100) / 200;

        /* Fill column by column from the left, bottom row first. */
        for (int x = left + 1; x < right && count > 0; x++) {
            for (int y = bottom - 1; y > top && count > 0; y--, count--) {
                set_px(frame, x, y);
            }
        }
    }
}

void tom_oled_quota_render(uint8_t frame[TOM_OLED_QUOTA_FRAME_BYTES],
                           const struct tom_oled_quota_render_input *input) {
    char buf[16];
    char label[8];
    bool known = input->primary_remaining <= 100;

    memset(frame, 0, TOM_OLED_QUOTA_FRAME_BYTES);

    text(frame, input->source, 3, 3, 1);

    window_label(label, sizeof(label), input->primary_window_minutes);
    snprintf(buf, sizeof(buf), "%s LEFT", label);
    text(frame, buf, 44, 3, 1);

    percent(buf, sizeof(buf), input->primary_remaining);
    text(frame, buf, 125 - text_width(buf, 2), 2, 2);

    if (known) {
        meter(frame, input->primary_remaining);
    } else {
        text(frame, "NO DATA", 50, 14, 1);
    }

    window_label(label, sizeof(label), input->secondary_window_minutes);
    snprintf(buf, sizeof(buf), "%s ", label);
    percent(buf + strlen(buf), sizeof(buf) - strlen(buf), input->secondary_remaining);
    text(frame, buf, 3, 26, 1);

    reset_label(buf, sizeof(buf), known ? input->reset_minutes : -1);
    text(frame, buf, 125 - text_width(buf, 1), 26, 1);
}
