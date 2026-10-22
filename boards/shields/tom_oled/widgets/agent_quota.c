/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "agent_quota.h"
#include "agent_quota_render.h"

#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <zmk/display.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* The reset countdown is shown in minutes, so redraw often enough to tick it. */
#define QUOTA_REDRAW_PERIOD_MS 15000

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static struct k_spinlock lock;
static struct zmk_tom_oled_quota current = {
    .source = ZMK_TOM_OLED_QUOTA_SOURCE_CODEX,
    .primary_remaining = TOM_OLED_QUOTA_UNKNOWN_PERCENT,
    .secondary_remaining = TOM_OLED_QUOTA_UNKNOWN_PERCENT,
    .primary_reset_seconds = TOM_OLED_QUOTA_UNKNOWN_RESET,
};
static bool have_value;
static int64_t reset_at_ms;

ZMK_EVENT_IMPL(zmk_tom_oled_agent_quota_changed);

static void notify_changed(void) {
    raise_zmk_tom_oled_agent_quota_changed((struct zmk_tom_oled_agent_quota_changed){0});
}

static void quota_timeout_cb(struct k_work *work) {
    ARG_UNUSED(work);
    zmk_tom_oled_agent_quota_clear();
}

static K_WORK_DELAYABLE_DEFINE(quota_timeout_work, quota_timeout_cb);

int zmk_tom_oled_agent_quota_set(const struct zmk_tom_oled_quota *quota, uint16_t ttl_seconds) {
    if (quota->source > ZMK_TOM_OLED_QUOTA_SOURCE_CLAUDE ||
        (quota->primary_remaining > 100 &&
         quota->primary_remaining != TOM_OLED_QUOTA_UNKNOWN_PERCENT) ||
        (quota->secondary_remaining > 100 &&
         quota->secondary_remaining != TOM_OLED_QUOTA_UNKNOWN_PERCENT)) {
        return -EINVAL;
    }

    k_spinlock_key_t key = k_spin_lock(&lock);
    current = *quota;
    have_value = true;
    reset_at_ms = quota->primary_reset_seconds == TOM_OLED_QUOTA_UNKNOWN_RESET
                      ? -1
                      : k_uptime_get() + (int64_t)quota->primary_reset_seconds * 1000;
    k_spin_unlock(&lock, key);

    if (ttl_seconds == 0) {
        k_work_cancel_delayable(&quota_timeout_work);
    } else {
        k_work_reschedule(&quota_timeout_work, K_SECONDS(ttl_seconds));
    }

    notify_changed();
    return 0;
}

void zmk_tom_oled_agent_quota_clear(void) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    /* Keep the source and window labels so the stale screen still says what it was. */
    have_value = false;
    k_spin_unlock(&lock, key);

    k_work_cancel_delayable(&quota_timeout_work);
    notify_changed();
}

bool zmk_tom_oled_agent_quota_get(struct zmk_tom_oled_quota *quota) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    bool fresh = have_value;
    int64_t reset_at = reset_at_ms;
    *quota = current;
    k_spin_unlock(&lock, key);

    if (fresh && reset_at >= 0) {
        int64_t left_ms = reset_at - k_uptime_get();
        /* A window past its reset no longer describes the account (KYUREN freshWindows). */
        if (left_ms <= 0) {
            fresh = false;
        } else {
            quota->primary_reset_seconds = (uint32_t)((left_ms + 999) / 1000);
        }
    }

    if (!fresh) {
        quota->primary_remaining = TOM_OLED_QUOTA_UNKNOWN_PERCENT;
        quota->secondary_remaining = TOM_OLED_QUOTA_UNKNOWN_PERCENT;
        quota->primary_reset_seconds = TOM_OLED_QUOTA_UNKNOWN_RESET;
    }
    return fresh;
}

static void draw(struct zmk_widget_agent_quota *widget) {
    static uint8_t frame[TOM_OLED_QUOTA_FRAME_BYTES];
    struct zmk_tom_oled_quota quota;

    zmk_tom_oled_agent_quota_get(&quota);

    struct tom_oled_quota_render_input input = {
        .source = quota.source == ZMK_TOM_OLED_QUOTA_SOURCE_CLAUDE ? "CLAUDE" : "CODEX",
        .primary_remaining = quota.primary_remaining,
        .secondary_remaining = quota.secondary_remaining,
        .primary_window_minutes = quota.primary_window_minutes,
        .secondary_window_minutes = quota.secondary_window_minutes,
        /* Ceil to minutes like KYUREN so 0:00 only appears at the reset itself. */
        .reset_minutes = quota.primary_reset_seconds == TOM_OLED_QUOTA_UNKNOWN_RESET
                             ? -1
                             : (int32_t)((quota.primary_reset_seconds + 59) / 60),
    };
    tom_oled_quota_render(frame, &input);

    lv_canvas_fill_bg(widget->obj, lv_color_white(), LV_OPA_COVER);
    for (int y = 0; y < TOM_OLED_QUOTA_HEIGHT; y++) {
        for (int x = 0; x < TOM_OLED_QUOTA_WIDTH; x++) {
            if (frame[(y * TOM_OLED_QUOTA_WIDTH + x) >> 3] & (0x80 >> (x & 7))) {
                lv_canvas_set_px(widget->obj, x, y, lv_color_black(), LV_OPA_COVER);
            }
        }
    }
}

static void redraw_all(void) {
    struct zmk_widget_agent_quota *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { draw(widget); }
}

static void quota_redraw_timer_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    redraw_all();
}

static void agent_quota_update_cb(uint8_t unused) {
    ARG_UNUSED(unused);
    redraw_all();
}

static uint8_t agent_quota_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    return 0;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_agent_quota, uint8_t, agent_quota_update_cb,
                            agent_quota_get_state)
ZMK_SUBSCRIPTION(widget_agent_quota, zmk_tom_oled_agent_quota_changed);

int zmk_widget_agent_quota_init(struct zmk_widget_agent_quota *widget, lv_obj_t *parent) {
    widget->obj = lv_canvas_create(parent);
    lv_result_t result = lv_draw_buf_init(
        &widget->draw_buf, TOM_OLED_QUOTA_WIDTH, TOM_OLED_QUOTA_HEIGHT, LV_COLOR_FORMAT_I1,
        LV_DRAW_BUF_STRIDE(TOM_OLED_QUOTA_WIDTH, LV_COLOR_FORMAT_I1), widget->cbuf,
        sizeof(widget->cbuf));
    if (result != LV_RESULT_OK) {
        LOG_ERR("Failed to initialize agent quota draw buffer");
        return -ENOMEM;
    }
    lv_draw_buf_set_flag(&widget->draw_buf, LV_IMAGE_FLAGS_MODIFIABLE);
    lv_canvas_set_draw_buf(widget->obj, &widget->draw_buf);
    lv_canvas_set_palette(widget->obj, 0, lv_color_to_32(lv_color_black(), LV_OPA_COVER));
    lv_canvas_set_palette(widget->obj, 1, lv_color_to_32(lv_color_white(), LV_OPA_COVER));
    lv_obj_set_size(widget->obj, TOM_OLED_QUOTA_WIDTH, TOM_OLED_QUOTA_HEIGHT);
    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);

    sys_slist_append(&widgets, &widget->node);
    widget_agent_quota_init();
    lv_timer_create(quota_redraw_timer_cb, QUOTA_REDRAW_PERIOD_MS, NULL);
    return 0;
}

lv_obj_t *zmk_widget_agent_quota_obj(struct zmk_widget_agent_quota *widget) {
    return widget->obj;
}
