#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bring up the ILI9341 display and XPT2046 touch controller and initialize
 * LVGL on top of them (SPI buses, panel/touch drivers, backlight, LVGL port
 * display + touch input device).
 *
 * @return the LVGL display handle on success, NULL on failure (see logs).
 */
lv_display_t *display_init(void);

#ifdef __cplusplus
}
#endif
