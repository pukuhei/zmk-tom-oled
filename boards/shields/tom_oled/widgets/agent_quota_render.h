/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/* Pure renderer shared by the widget and the host-side pixel test. */

#define TOM_OLED_QUOTA_FRAME_WIDTH 128
#define TOM_OLED_QUOTA_FRAME_HEIGHT 32
#define TOM_OLED_QUOTA_FRAME_BYTES (TOM_OLED_QUOTA_FRAME_WIDTH * TOM_OLED_QUOTA_FRAME_HEIGHT / 8)
#define TOM_OLED_QUOTA_RENDER_UNKNOWN 0xFF

struct tom_oled_quota_render_input {
    const char *source;
    /* 0..100, or TOM_OLED_QUOTA_RENDER_UNKNOWN. */
    uint8_t primary_remaining;
    uint8_t secondary_remaining;
    /* 0 when unknown. */
    uint16_t primary_window_minutes;
    uint16_t secondary_window_minutes;
    /* Negative when unknown. */
    int32_t reset_minutes;
};

/* Row-major, MSB first, 1 = lit pixel. */
void tom_oled_quota_render(uint8_t frame[TOM_OLED_QUOTA_FRAME_BYTES],
                           const struct tom_oled_quota_render_input *input);
