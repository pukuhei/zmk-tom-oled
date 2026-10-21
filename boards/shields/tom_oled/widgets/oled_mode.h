/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>

enum zmk_tom_oled_mode {
    ZMK_TOM_OLED_MODE_BONGO = 0,
    ZMK_TOM_OLED_MODE_AGENT = 1,
    ZMK_TOM_OLED_MODE_QUOTA = 2,
};

enum zmk_tom_oled_mode zmk_tom_oled_mode_get(void);
void zmk_tom_oled_mode_toggle(void);
/* bongo_widget may be NULL when Bongo Cat is disabled; quota_widget covers the whole screen. */
void zmk_tom_oled_mode_display_init(lv_obj_t *bongo_widget, lv_obj_t *agent_widget,
                                    lv_obj_t *quota_widget);
