#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "touch_calibration.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load a stored touch calibration from NVS. *found is set to true only if a
 * complete calibration was present; on any other outcome (nothing stored
 * yet, or an error) *out is left untouched and *found is set to false, so
 * the caller can fall back to touch_calibration_set_defaults() or trigger
 * the first-run calibration flow.
 */
esp_err_t touch_cal_store_load(touch_calibration_t *out, bool *found);

/** Persist a calibration to NVS so it survives a power cycle. */
esp_err_t touch_cal_store_save(const touch_calibration_t *in);

#ifdef __cplusplus
}
#endif
