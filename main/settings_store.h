#pragma once

#include "esp_err.h"
#include "morse_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize NVS flash (erasing and retrying once on a version-mismatch/
 * out-of-space error, the standard ESP-IDF pattern) and load saved
 * settings, falling back to compiled-in defaults for any missing or
 * out-of-range stored value. Must be called once at boot, before applying
 * settings to the keyer/decoder/sidetone or any UI widget.
 */
esp_err_t settings_store_init_and_load(morse_settings_t *out);

/** Persist settings to NVS so they survive a power cycle. */
esp_err_t settings_store_save(const morse_settings_t *in);

/** Erase any stored settings so the next load falls back to compiled-in defaults. */
esp_err_t settings_store_reset(void);

#ifdef __cplusplus
}
#endif
