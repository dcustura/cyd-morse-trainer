#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the touch calibration screen: a 5-point sequence (4 corners plus a
 * center precision check) that derives a fresh touch calibration, rejects
 * and restarts on an imprecise result, then saves it to NVS and applies it
 * immediately. On success it navigates to next_screen.
 *
 * The Back button is hidden by default (for the mandatory first-run flow,
 * where there is no existing calibration to fall back to). Call
 * ui_calibration_set_cancel_target() before navigating here to show it for
 * a re-calibration entry point (e.g. from Settings).
 *
 * Must be called while holding the LVGL lock (lvgl_port_lock).
 */
lv_obj_t *ui_calibration_create(lv_obj_t *next_screen);

/**
 * Set where the Back button (shown on the next time this screen loads)
 * navigates to, and thereby whether it's shown at all: pass NULL to hide it
 * (the default, used for the mandatory first-run flow), or a screen to show
 * it and navigate there on Back, discarding any in-progress taps and
 * leaving the previously stored calibration untouched.
 */
void ui_calibration_set_cancel_target(lv_obj_t *cancel_screen);

#ifdef __cplusplus
}
#endif
