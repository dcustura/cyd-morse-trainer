#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the touch verification screen: a full-screen black canvas (every
 * pixel drawable, including all 4 true corners) where each touch draws a
 * dot at the exact reported (calibrated) coordinate plus a live X/Y
 * readout, so a user can visually check whether touches land where they
 * actually pressed.
 *
 * Back and Clear are triggered by swiping rather than by fixed-position
 * buttons, so they keep working even under fairly severe miscalibration -
 * exactly the situation this screen exists to diagnose - since a swipe's
 * direction survives a linear calibration error even though its absolute
 * position doesn't: swipe down starting near the top edge to go Back, swipe
 * up starting near the bottom edge to Clear.
 *
 * The Back target isn't known yet at creation time (it's Settings, which is
 * created afterwards) - call ui_touch_test_set_back_target() once that
 * screen exists, before this screen can be navigated to.
 *
 * Must be called while holding the LVGL lock (lvgl_port_lock).
 */
lv_obj_t *ui_touch_test_create(void);

/** Set the screen a Back swipe navigates to. */
void ui_touch_test_set_back_target(lv_obj_t *back_screen);

#ifdef __cplusplus
}
#endif
