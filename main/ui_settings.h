#pragma once

#include "lvgl.h"
#include "iambic_keyer.h"
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the Settings screen: a 3x2 grid of tappable tiles (Keyer, Sidetone,
 * Display, Bluetooth, Reset to Factory Defaults, and a final "< Back"
 * tile), filling the whole display with no scrolling and no separate title
 * bar. "Keyer" navigates to a submenu (created internally) with a 2x2 grid
 * of WPM, Key Mode, Paddle Swap, and Paddle Debounce tiles; "Sidetone"
 * navigates to a submenu with Pitch, Volume, and Smoothing tiles; "Display"
 * navigates to a submenu with Brightness, Touchscreen, and Text Size tiles;
 * "Bluetooth" navigates to a submenu with a single "BLE Keyboard" on/off
 * tile, which sends decoded Morse characters to a paired phone/PC as
 * keystrokes -- like every other toggle, "On"/"Off" is applied to
 * settings_t and persisted immediately, but (unlike the others) doesn't
 * take effect until the device is next restarted, since bringing the BLE
 * radio stack up or down live isn't supported; its popup's help text says
 * so.
 * Tapping a numeric tile (WPM, Pitch/Volume/Smoothing, Brightness) opens a
 * popup with -/+ buttons (supporting press-and-hold repeat) and, for the
 * sidetone fields (Pitch/Volume/Smoothing), a "Test" button that plays the
 * sidetone at its current settings. The Brightness popup additionally has an
 * "Auto"/"Manual" toggle button: in Auto mode the backlight is driven by the
 * onboard ambient-light sensor and the tile shows "Auto" instead of a percentage;
 * pressing +/- always switches back to Manual immediately. Tapping Key Mode
 * opens a popup listing its four options ("Mode A"/"Mode B"/"Straight"/
 * "Ultimatic") as a 3-column x 2-row grid of finger-sized buttons, with
 * "Close" filling the last cell. Tapping Paddle Swap or Paddle Debounce
 * opens a popup listing its two options as a row of equal-width,
 * finger-sized buttons (Paddle Swap: "Normal"/"Swapped"; Paddle Debounce:
 * "On"/"Off" -- this only affects Mode A/B/Ultimatic; Straight Key mode is
 * always debounced). Tapping Text Size opens a similar popup
 * ("Small"/"Large") that switches the Practice screen's decoded-text font
 * immediately between a small fixed-width font and a larger, anti-aliased
 * monospace font (see main/fonts.h), live, even while Practice isn't the
 * screen currently shown. Every change is applied to the running trainer
 * and persisted to NVS immediately when made; there is no OK/Cancel or
 * discard-on-cancel step. "Touchscreen" navigates to a submenu screen
 * (created internally) with two tiles: "Calibrate" (navigates to
 * calibration_screen, with its Back button enabled, returning to the
 * Display submenu) and "Verify Calibration" (navigates to touch_test_screen,
 * whose swipe-Back also returns to the Display submenu). "Reset to Factory
 * Defaults" shows a confirmation popup before erasing settings and
 * calibration and restarting. The last tile, "< Back", returns to
 * menu_screen. Must be called while holding the LVGL lock (lvgl_port_lock).
 */
lv_obj_t *ui_settings_create(lv_obj_t *menu_screen, lv_obj_t *calibration_screen,
                              lv_obj_t *touch_test_screen, iambic_keyer_mode_t initial_mode,
                              uint16_t initial_wpm, bool initial_swap, bool initial_debounce,
                              uint16_t initial_tone_hz, uint8_t initial_volume_pct,
                              uint16_t initial_envelope_ms, uint8_t initial_brightness_pct,
                              bool initial_brightness_auto, bool initial_large_text,
                              bool initial_ble_hid_enabled);

/**
 * Update the Settings screen's cached values and visible tile labels to
 * match *settings. For use by code outside this file (namely the CLI) that
 * changes a setting through its live-apply function directly rather than
 * through one of this screen's own popups, so the screen doesn't show a
 * stale value the next time it's opened. Must be called while holding the
 * LVGL lock (lvgl_port_lock).
 */
void ui_settings_sync(const settings_t *settings);

/**
 * The Keyer submenu screen created internally by ui_settings_create(), for
 * navigating to it directly from outside this file (e.g. from the Practice
 * screen). Must be called after ui_settings_create().
 */
lv_obj_t *ui_settings_get_keyer_submenu(void);

/**
 * Set where the Keyer submenu's "< Back" tile navigates to on its next load.
 * ui_settings_create()'s own "Keyer" tile resets this to the Settings screen
 * each time it navigates there, so an external caller (e.g. the Practice
 * screen, navigating here directly) must set it again immediately before
 * each lv_scr_load() to the Keyer submenu for Back to return to the right
 * place.
 */
void ui_settings_set_keyer_back_target(lv_obj_t *back_target_screen);

#ifdef __cplusplus
}
#endif
