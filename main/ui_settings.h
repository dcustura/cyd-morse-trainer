#pragma once

#include "lvgl.h"
#include "iambic_keyer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the Settings screen: WPM slider, key-mode dropdown, paddle-swap
 * switch, sidetone-frequency slider, keying-envelope (attack/decay)
 * duration slider, sidetone-volume slider, a "Test tone" button (plays at
 * the currently-selected frequency, envelope, and volume), a "Calibrate
 * Touchscreen" button that navigates to calibration_screen (with its Back
 * button enabled, returning here), a "Verify Calibration" button that
 * navigates to touch_test_screen, a "Reset to Factory Defaults" button with
 * a confirmation popup, and OK/Cancel. Widget changes only take effect when
 * OK is pressed; leaving via Cancel (or any other navigation away from the
 * screen) discards edits, and widgets are reset to the last-saved values
 * the next time the screen is shown. Must be called while holding the LVGL
 * lock (lvgl_port_lock).
 */
lv_obj_t *ui_settings_create(lv_obj_t *menu_screen, lv_obj_t *calibration_screen,
                              lv_obj_t *touch_test_screen, iambic_keyer_mode_t initial_mode,
                              uint16_t initial_wpm, bool initial_swap, uint16_t initial_tone_hz,
                              uint8_t initial_volume_pct, uint16_t initial_envelope_ms);

#ifdef __cplusplus
}
#endif
