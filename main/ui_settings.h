#pragma once

#include "lvgl.h"
#include "iambic_keyer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the Settings screen: WPM slider, key-mode dropdown, paddle-swap
 * switch, sidetone-frequency slider with a "Test tone" button, a
 * "Calibrate Touchscreen" button that navigates to calibration_screen, and
 * Save/Back. Widget changes only take effect when Save is pressed (a
 * known, accepted simplification: navigating Back first discards edits).
 * Must be called while holding the LVGL lock (lvgl_port_lock).
 */
lv_obj_t *ui_settings_create(lv_obj_t *menu_screen, lv_obj_t *calibration_screen,
                              iambic_keyer_mode_t initial_mode, uint16_t initial_wpm,
                              bool initial_swap, uint16_t initial_tone_hz);

#ifdef __cplusplus
}
#endif
