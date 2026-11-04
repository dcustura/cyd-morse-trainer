#pragma once

#include "lvgl.h"
#include "iambic_keyer.h"
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the Settings screen and all of its submenus, starting from
 * *initial. The main screen is a 3x2 tile grid (Keyer, Sidetone, Display,
 * Bluetooth, System, Back to menu_screen); each tile opens a submenu of
 * further tiles. Every change is applied to the running trainer at once
 * and saved to NVS (debounced), with no OK/Cancel step. Calibrate (Display >
 * Touchscreen) opens calibration_screen, and Verify Calibration opens
 * touch_test_screen; both return to the Touchscreen submenu. See
 * docs/functional-spec.md ("Settings screen") for each tile's behavior.
 * Must be called while holding the LVGL lock (lvgl_port_lock).
 */
lv_obj_t *ui_settings_create(lv_obj_t *menu_screen, lv_obj_t *calibration_screen,
                              lv_obj_t *touch_test_screen, const settings_t *initial);

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
 * Copy out the current settings -- the single live copy, updated by every
 * Settings-screen change and by ui_settings_sync(). Must be called while
 * holding the LVGL lock (lvgl_port_lock).
 */
void ui_settings_get(settings_t *out);

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
