#include "touch_calibration.h"

void touch_calibration_set_defaults(touch_calibration_t *out)
{
    out->horiz_min = TOUCH_CALIBRATION_DEFAULT_HORIZ_MIN;
    out->horiz_max = TOUCH_CALIBRATION_DEFAULT_HORIZ_MAX;
    out->vert_min = TOUCH_CALIBRATION_DEFAULT_VERT_MIN;
    out->vert_max = TOUCH_CALIBRATION_DEFAULT_VERT_MAX;
}

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void touch_calibration_apply(const touch_calibration_t *cal, int32_t raw_horiz, int32_t raw_vert,
                              uint16_t lcd_h_res, uint16_t lcd_v_res, uint16_t *out_x, uint16_t *out_y)
{
    int32_t x = (raw_horiz - (int32_t)cal->horiz_min) * (lcd_h_res - 1)
                / ((int32_t)cal->horiz_max - (int32_t)cal->horiz_min);
    int32_t y = (raw_vert - (int32_t)cal->vert_min) * (lcd_v_res - 1)
                / ((int32_t)cal->vert_max - (int32_t)cal->vert_min);

    *out_x = (uint16_t)clamp_i32(x, 0, lcd_h_res - 1);
    *out_y = (uint16_t)clamp_i32(y, 0, lcd_v_res - 1);
}

bool touch_calibration_compute(const touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT],
                                uint16_t lcd_h_res, uint16_t lcd_v_res, touch_calibration_t *out)
{
    /* horiz_min/max and vert_min/max are each the average of the matching
     * pair of corner taps; the center tap isn't used for the mapping
     * itself, only as the precision check below. This assumes the same
     * raw-value polarity (increasing raw = increasing screen position) that
     * was empirically confirmed for this board's touch driver quirk - see
     * main/display_init.c. */
    int32_t horiz_min = (points[TOUCH_CAL_POINT_TOP_LEFT].raw_horiz
                          + points[TOUCH_CAL_POINT_BOTTOM_LEFT].raw_horiz) / 2;
    int32_t horiz_max = (points[TOUCH_CAL_POINT_TOP_RIGHT].raw_horiz
                          + points[TOUCH_CAL_POINT_BOTTOM_RIGHT].raw_horiz) / 2;
    int32_t vert_min = (points[TOUCH_CAL_POINT_TOP_LEFT].raw_vert
                         + points[TOUCH_CAL_POINT_TOP_RIGHT].raw_vert) / 2;
    int32_t vert_max = (points[TOUCH_CAL_POINT_BOTTOM_LEFT].raw_vert
                         + points[TOUCH_CAL_POINT_BOTTOM_RIGHT].raw_vert) / 2;

    if (horiz_max - horiz_min < TOUCH_CALIBRATION_MIN_RAW_SPAN
        || vert_max - vert_min < TOUCH_CALIBRATION_MIN_RAW_SPAN) {
        return false;
    }

    touch_calibration_t candidate = {
        .horiz_min = (uint16_t)horiz_min,
        .horiz_max = (uint16_t)horiz_max,
        .vert_min = (uint16_t)vert_min,
        .vert_max = (uint16_t)vert_max,
    };

    uint16_t center_x, center_y;
    touch_calibration_apply(&candidate, points[TOUCH_CAL_POINT_CENTER].raw_horiz,
                             points[TOUCH_CAL_POINT_CENTER].raw_vert, lcd_h_res, lcd_v_res,
                             &center_x, &center_y);

    int32_t expected_x = (lcd_h_res - 1) / 2;
    int32_t expected_y = (lcd_v_res - 1) / 2;
    int32_t error_x = (center_x > expected_x) ? (center_x - expected_x) : (expected_x - center_x);
    int32_t error_y = (center_y > expected_y) ? (center_y - expected_y) : (expected_y - center_y);

    if (error_x > TOUCH_CALIBRATION_MAX_CENTER_ERROR_PX || error_y > TOUCH_CALIBRATION_MAX_CENTER_ERROR_PX) {
        return false;
    }

    *out = candidate;
    return true;
}
