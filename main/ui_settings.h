#pragma once

#include "lvgl.h"
#include "iambic_keyer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the Settings screen: a 3x2 grid of tappable tiles (Keyer, Sidetone,
 * Brightness, Touchscreen, Reset to Factory Defaults, and a final "< Back"
 * tile), filling the whole display with no scrolling and no separate title
 * bar. "Keyer" navigates to a submenu (created internally) with WPM, Key
 * Mode, and Paddle Swap tiles; "Sidetone" navigates to a submenu with Pitch,
 * Volume, and Smoothing tiles. Tapping a numeric tile (WPM, Pitch/Volume/
 * Smoothing, Brightness) opens a popup with -/+ buttons (supporting
 * press-and-hold repeat) and, for the sidetone fields (Pitch/Volume/
 * Smoothing), a "Test" button that plays the sidetone at its current
 * settings. The Brightness popup additionally has an "Auto"/"Manual"
 * toggle button: in Auto mode the backlight is driven by the onboard
 * ambient-light sensor and the tile shows "Auto" instead of a percentage;
 * pressing +/- always switches back to Manual immediately. Tapping Key Mode
 * or Paddle Swap opens a popup listing its
 * options as a row of equal-width, finger-sized buttons (Key Mode:
 * "Mode A"/"Mode B"/"Straight"; Paddle Swap: "Normal"/"Swapped"). Every
 * change is applied to the running trainer and
 * persisted to NVS immediately when made; there is no OK/Cancel or
 * discard-on-cancel step. "Touchscreen" navigates to a submenu screen
 * (created internally) with two tiles: "Calibrate" (navigates to
 * calibration_screen, with its Back button enabled, returning to the
 * submenu) and "Verify Calibration" (navigates to touch_test_screen, whose
 * swipe-Back also returns to the submenu). "Reset to Factory Defaults"
 * shows a confirmation popup before erasing settings and calibration and
 * restarting. The last tile, "< Back", returns to menu_screen. Must be
 * called while holding the LVGL lock (lvgl_port_lock).
 */
lv_obj_t *ui_settings_create(lv_obj_t *menu_screen, lv_obj_t *calibration_screen,
                              lv_obj_t *touch_test_screen, iambic_keyer_mode_t initial_mode,
                              uint16_t initial_wpm, bool initial_swap, uint16_t initial_tone_hz,
                              uint8_t initial_volume_pct, uint16_t initial_envelope_ms,
                              uint8_t initial_brightness_pct, bool initial_brightness_auto);

#ifdef __cplusplus
}
#endif
