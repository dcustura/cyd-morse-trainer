#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the touch verification screen: a full-screen black canvas where
 * each touch draws a dot at the exact reported (calibrated) coordinate, so
 * a user can visually check whether touches land where they actually
 * pressed. Has a "Clear" button and a "Back" button.
 *
 * The Back button's target isn't known yet at creation time (it's Settings,
 * which is created afterwards) - call ui_touch_test_set_back_target() once
 * that screen exists, before this screen can be navigated to.
 *
 * Must be called while holding the LVGL lock (lvgl_port_lock).
 */
lv_obj_t *ui_touch_test_create(void);

/** Set the screen the Back button navigates to. */
void ui_touch_test_set_back_target(lv_obj_t *back_screen);

#ifdef __cplusplus
}
#endif
