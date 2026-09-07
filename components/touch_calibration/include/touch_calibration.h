#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Factory defaults: the empirically-measured raw touch range for the
 * ESP32-2432S028R found during hardware bring-up (see main/display_init.c).
 * Used until a real calibration is stored, and as the fallback if a stored
 * calibration is ever found to be invalid. */
#define TOUCH_CALIBRATION_DEFAULT_HORIZ_MIN 17
#define TOUCH_CALIBRATION_DEFAULT_HORIZ_MAX 217
#define TOUCH_CALIBRATION_DEFAULT_VERT_MIN 31
#define TOUCH_CALIBRATION_DEFAULT_VERT_MAX 283

/* Minimum acceptable raw span (per axis) between a corner pair for a
 * calibration to be accepted; guards against a degenerate calibration from
 * corners tapped too close together. */
#define TOUCH_CALIBRATION_MIN_RAW_SPAN 20

/* Maximum acceptable deviation (in screen pixels, either axis) between the
 * center tap mapped through the freshly computed calibration and the true
 * screen center, before a calibration is rejected as imprecise. */
#define TOUCH_CALIBRATION_MAX_CENTER_ERROR_PX 24

typedef struct {
    uint16_t horiz_min;
    uint16_t horiz_max;
    uint16_t vert_min;
    uint16_t vert_max;
} touch_calibration_t;

/* Raw (uncalibrated) touch sample: the driver's "horizontal"/"vertical"
 * readings after the fixed axis-swap already applied for this board (see
 * main/display_init.c), but before any min/max scaling. */
typedef struct {
    int32_t raw_horiz;
    int32_t raw_vert;
} touch_calibration_raw_point_t;

typedef enum {
    TOUCH_CAL_POINT_TOP_LEFT = 0,
    TOUCH_CAL_POINT_TOP_RIGHT,
    TOUCH_CAL_POINT_BOTTOM_LEFT,
    TOUCH_CAL_POINT_BOTTOM_RIGHT,
    TOUCH_CAL_POINT_CENTER,
    TOUCH_CAL_POINT_COUNT,
} touch_calibration_point_index_t;

/** Populate *out with the compiled-in factory defaults. */
void touch_calibration_set_defaults(touch_calibration_t *out);

/**
 * Compute a calibration from 5 raw taps (indexed by touch_calibration_point_index_t):
 * the 4 screen corners plus a center tap used purely as a precision check.
 *
 * The corner targets are assumed to sit target_margin screen pixels in from
 * the true screen edges on each side (a UI showing crosshairs flush against
 * the very edge would clip them and be unreliable to tap) - pass 0 if the
 * corner samples were taken exactly at the true edges instead. horiz_min/max
 * and vert_min/max are derived by averaging the matching pair of corner
 * samples and then extrapolating outward by target_margin, so the stored
 * calibration lines up with touch_calibration_apply()'s assumption that
 * horiz_min/vert_min map to the true screen origin and horiz_max/vert_max
 * map to the true far edge - not to the inset corner targets themselves.
 * Skipping this extrapolation (i.e. always passing 0 regardless of the
 * actual margin used) systematically overshoots away from screen center:
 * every touch away from the middle would map further out than intended,
 * roughly in proportion to its distance from center.
 *
 * The result is rejected (returns false, *out left unmodified) if either
 * axis's corner samples are too close together (TOUCH_CALIBRATION_MIN_RAW_SPAN)
 * or if mapping the center tap through the computed calibration lands more
 * than TOUCH_CALIBRATION_MAX_CENTER_ERROR_PX away from the true screen
 * center - both signal an imprecise or mistaken tap sequence that should be
 * retried.
 */
bool touch_calibration_compute(const touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT],
                                uint16_t lcd_h_res, uint16_t lcd_v_res, uint16_t target_margin,
                                touch_calibration_t *out);

/**
 * Map a raw touch sample to screen coordinates using *cal, clamped to
 * [0, lcd_h_res-1] / [0, lcd_v_res-1].
 */
void touch_calibration_apply(const touch_calibration_t *cal, int32_t raw_horiz, int32_t raw_vert,
                              uint16_t lcd_h_res, uint16_t lcd_v_res, uint16_t *out_x, uint16_t *out_y);

#ifdef __cplusplus
}
#endif
