/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include <zephyr/sys/util.h>

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include "widgets/agent_quota.h"
#include "widgets/battery_status.h"
#include "widgets/codex_status.h"
#include "widgets/modifiers.h"
#include "widgets/bongo_cat.h"
#include "widgets/oled_mode.h"
#include "widgets/layer_status.h"
#include "widgets/output_status.h"
#include "widgets/hid_indicators.h"
#include "widgets/wpm_status.h"
#else
#include "widgets/peripheral_status.h"
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static struct zmk_widget_output_status output_status_widget;

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_LAYER)
static struct zmk_widget_layer_status layer_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_MODIFIERS)
static struct zmk_widget_modifiers modifiers_widget;
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

#endif

#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_BONGO_CAT)
static struct zmk_widget_bongo_cat bongo_cat_widget;
#endif
static struct zmk_widget_codex_status codex_status_widget;
static struct zmk_widget_agent_quota agent_quota_widget;

#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_WPM)
static struct zmk_widget_wpm_status wpm_status_widget;
#endif
#else
static struct zmk_widget_peripheral_status peripheral_status_widget;
#endif

lv_style_t global_style;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;

    screen = lv_obj_create(NULL);

    lv_style_init(&global_style);
    lv_style_set_bg_color(&global_style, lv_color_white());
    lv_style_set_bg_opa(&global_style, LV_OPA_COVER);
    lv_style_set_text_color(&global_style, lv_color_black());
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);

#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_WPM)
    zmk_widget_wpm_status_init(&wpm_status_widget, screen);
    lv_obj_align_to(zmk_widget_wpm_status_obj(&wpm_status_widget), zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_OUT_RIGHT_MID, 7, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_BONGO_CAT)
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    /* Keep the 50 px cat between the connection icons and battery text. */
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_CENTER, -5, -3);
#endif

    zmk_widget_codex_status_init(&codex_status_widget, screen);
    lv_obj_align(zmk_widget_codex_status_obj(&codex_status_widget), LV_ALIGN_CENTER, -5, -3);


#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_MODIFIERS)
    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_BOTTOM_LEFT, 0, 5);
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);
    lv_obj_align_to(zmk_widget_hid_indicators_obj(&hid_indicators_widget), zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_OUT_TOP_LEFT, 0, -2);
#endif
#endif

#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_LAYER)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_style_text_align(zmk_widget_layer_status_obj(&layer_status_widget),
                                LV_TEXT_ALIGN_RIGHT, 0);
#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_BONGO_CAT)
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#else
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, -3);
#endif
#endif

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), LV_ALIGN_TOP_RIGHT, 0, 0);
#endif

    /* Created last so the full-screen quota view sits above every status widget. */
    zmk_widget_agent_quota_init(&agent_quota_widget, screen);
    lv_obj_align(zmk_widget_agent_quota_obj(&agent_quota_widget), LV_ALIGN_TOP_LEFT, 0, 0);
    zmk_tom_oled_mode_display_init(
#if IS_ENABLED(CONFIG_ZMK_TOM_OLED_BONGO_CAT)
        zmk_widget_bongo_cat_obj(&bongo_cat_widget),
#else
        NULL,
#endif
        zmk_widget_codex_status_obj(&codex_status_widget),
        zmk_widget_agent_quota_obj(&agent_quota_widget));
#else
    zmk_widget_peripheral_status_init(&peripheral_status_widget, screen);
    lv_obj_align(zmk_widget_peripheral_status_obj(&peripheral_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);
#endif

    return screen;
}
