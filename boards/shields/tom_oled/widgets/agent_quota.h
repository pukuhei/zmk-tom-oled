/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

#define TOM_OLED_QUOTA_WIDTH 128
#define TOM_OLED_QUOTA_HEIGHT 32
#define TOM_OLED_QUOTA_UNKNOWN_PERCENT 0xFF
#define TOM_OLED_QUOTA_UNKNOWN_RESET UINT32_MAX

enum zmk_tom_oled_quota_source {
    ZMK_TOM_OLED_QUOTA_SOURCE_CODEX = 0,
    ZMK_TOM_OLED_QUOTA_SOURCE_CLAUDE = 1,
};

/* Remaining account quota, not cost, context length or tokens. */
struct zmk_tom_oled_quota {
    enum zmk_tom_oled_quota_source source;
    /* 0..100, or TOM_OLED_QUOTA_UNKNOWN_PERCENT. */
    uint8_t primary_remaining;
    uint8_t secondary_remaining;
    /* Window length in minutes (300 = 5H, 10080 = 7D), 0 when unknown. */
    uint16_t primary_window_minutes;
    uint16_t secondary_window_minutes;
    /* Seconds until the primary window resets, or TOM_OLED_QUOTA_UNKNOWN_RESET. */
    uint32_t primary_reset_seconds;
};

struct zmk_tom_oled_agent_quota_changed {
    uint8_t unused;
};

ZMK_EVENT_DECLARE(zmk_tom_oled_agent_quota_changed);

struct zmk_widget_agent_quota {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_draw_buf_t draw_buf;
    LV_ATTRIBUTE_MEM_ALIGN uint8_t
        cbuf[LV_DRAW_BUF_SIZE(TOM_OLED_QUOTA_WIDTH, TOM_OLED_QUOTA_HEIGHT, LV_COLOR_FORMAT_I1)];
};

/* ttl_seconds == 0 keeps the value until replaced. */
int zmk_tom_oled_agent_quota_set(const struct zmk_tom_oled_quota *quota, uint16_t ttl_seconds);
void zmk_tom_oled_agent_quota_clear(void);
/* Returns false when no fresh value exists; *quota then holds the last source only. */
bool zmk_tom_oled_agent_quota_get(struct zmk_tom_oled_quota *quota);

int zmk_widget_agent_quota_init(struct zmk_widget_agent_quota *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_agent_quota_obj(struct zmk_widget_agent_quota *widget);
