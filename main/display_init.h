#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
#include "touch_calibration.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bring up the ST7789 display and XPT2046 touch controller and initialize
 * LVGL on top of them (SPI buses, panel/touch drivers, backlight, LVGL port
 * display + touch input device). Touch reports are mapped to screen
 * coordinates using the factory-default calibration until
 * display_touch_apply_calibration() is called with a stored one.
 *
 * @return the LVGL display handle on success, NULL on failure (see logs).
 */
lv_display_t *display_init(void);

/** Replace the calibration used to map touch reports to screen coordinates. */
void display_touch_apply_calibration(const touch_calibration_t *cal);

/**
 * Enable or disable raw touch passthrough: while enabled, touch reports
 * bypass calibration entirely (only the fixed driver axis-swap is applied)
 * so the calibration screen can record true raw samples. Must be disabled
 * again before normal screen navigation resumes.
 */
void display_touch_set_raw_mode(bool enable);

/**
 * Poll the touch controller directly, bypassing LVGL's input device
 * pipeline. Intended for the calibration screen, which needs press/release
 * edges and (in raw mode) unscaled coordinates rather than LVGL's clipped,
 * calibrated pointer events.
 *
 * @return true if currently pressed, with x and y set; false if not pressed.
 */
bool display_touch_read_point(uint16_t *x, uint16_t *y);

/**
 * Map a raw sample (as returned by display_touch_read_point() while raw
 * mode is enabled) to an approximate screen coordinate, using whatever
 * calibration is currently applied - regardless of raw mode. Intended for
 * hit-testing fixed UI controls (e.g. a Back button) from within the
 * calibration screen's own raw-sample polling loop, since raw mode also
 * defeats LVGL's normal click detection for every on-screen control while
 * it's enabled.
 */
void display_touch_map_raw_to_screen(int32_t raw_horiz, int32_t raw_vert, uint16_t *x, uint16_t *y);

/**
 * Map a logical LVGL color to the raw color that must be fed to LVGL for it
 * to actually appear as `c` on screen, compensating for this panel's
 * rgb_ele_order(BGR) + invert_color config (see the comment above
 * LCD_RGB_ORDER in display_init.c). A no-op for black, white, and grays.
 */
lv_color_t display_compensate_color(lv_color_t c);

/**
 * Give a button a teal background instead of the default theme's stock
 * blue. Deliberately a local per-object style rather than overriding the
 * shared default theme at runtime - re-initializing that global singleton
 * after screens/objects already exist is fragile (confirmed on hardware: it
 * hung the main task inside LVGL's style-refresh walk).
 */
void display_style_button_teal(lv_obj_t *btn);

#ifdef __cplusplus
}
#endif
