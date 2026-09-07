#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the touch calibration screen: a 5-point sequence (4 corners plus a
 * center precision check) that derives a fresh touch calibration, rejects
 * and restarts on an imprecise result, then saves it to NVS and applies it
 * immediately. Has no Back/Cancel control - once shown it always runs to
 * completion. On success it navigates to next_screen.
 *
 * Must be called while holding the LVGL lock (lvgl_port_lock).
 */
lv_obj_t *ui_calibration_create(lv_obj_t *next_screen);

#ifdef __cplusplus
}
#endif
