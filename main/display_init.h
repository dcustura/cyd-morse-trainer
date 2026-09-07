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

#ifdef __cplusplus
}
#endif
