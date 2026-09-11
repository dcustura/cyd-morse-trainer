#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure the onboard LDR's ADC input. Call once at boot, after
 * display_init() (auto mode, once enabled, drives the backlight via
 * display_set_brightness()).
 */
void auto_brightness_init(void);

/**
 * Enable or disable automatic backlight brightness from the ambient light
 * sensor. When enabled, a periodic timer samples the LDR and calls
 * display_set_brightness() directly, overriding whatever brightness was set
 * before. When disabled, the timer stops; the caller is responsible for
 * restoring the desired manual brightness via display_set_brightness().
 */
void auto_brightness_set_enabled(bool enabled);

#ifdef __cplusplus
}
#endif
