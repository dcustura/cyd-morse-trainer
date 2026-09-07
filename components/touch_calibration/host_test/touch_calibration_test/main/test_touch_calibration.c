#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "touch_calibration.h"

#define LCD_H_RES 320
#define LCD_V_RES 240

TEST_GROUP(touch_calibration);

TEST_SETUP(touch_calibration)
{
}

TEST_TEAR_DOWN(touch_calibration)
{
}

TEST(touch_calibration, set_defaults_populates_the_compiled_in_defaults)
{
    touch_calibration_t cal;
    memset(&cal, 0xFF, sizeof(cal)); /* poison to catch any field left unset */

    touch_calibration_set_defaults(&cal);

    TEST_ASSERT_EQUAL_UINT16(TOUCH_CALIBRATION_DEFAULT_HORIZ_MIN, cal.horiz_min);
    TEST_ASSERT_EQUAL_UINT16(TOUCH_CALIBRATION_DEFAULT_HORIZ_MAX, cal.horiz_max);
    TEST_ASSERT_EQUAL_UINT16(TOUCH_CALIBRATION_DEFAULT_VERT_MIN, cal.vert_min);
    TEST_ASSERT_EQUAL_UINT16(TOUCH_CALIBRATION_DEFAULT_VERT_MAX, cal.vert_max);
}

TEST(touch_calibration, apply_maps_calibrated_range_to_full_screen)
{
    touch_calibration_t cal = { .horiz_min = 20, .horiz_max = 220, .vert_min = 30, .vert_max = 280 };
    uint16_t x, y;

    touch_calibration_apply(&cal, 20, 30, LCD_H_RES, LCD_V_RES, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(0, x);
    TEST_ASSERT_EQUAL_UINT16(0, y);

    touch_calibration_apply(&cal, 220, 280, LCD_H_RES, LCD_V_RES, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(LCD_H_RES - 1, x);
    TEST_ASSERT_EQUAL_UINT16(LCD_V_RES - 1, y);
}

TEST(touch_calibration, apply_clamps_raw_values_outside_the_calibrated_range)
{
    touch_calibration_t cal = { .horiz_min = 20, .horiz_max = 220, .vert_min = 30, .vert_max = 280 };
    uint16_t x, y;

    touch_calibration_apply(&cal, 0, 0, LCD_H_RES, LCD_V_RES, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(0, x);
    TEST_ASSERT_EQUAL_UINT16(0, y);

    touch_calibration_apply(&cal, 4095, 4095, LCD_H_RES, LCD_V_RES, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(LCD_H_RES - 1, x);
    TEST_ASSERT_EQUAL_UINT16(LCD_V_RES - 1, y);
}

TEST(touch_calibration, compute_derives_min_max_from_clean_corner_taps)
{
    touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT] = {
        [TOUCH_CAL_POINT_TOP_LEFT] = { .raw_horiz = 20, .raw_vert = 30 },
        [TOUCH_CAL_POINT_TOP_RIGHT] = { .raw_horiz = 220, .raw_vert = 30 },
        [TOUCH_CAL_POINT_BOTTOM_LEFT] = { .raw_horiz = 20, .raw_vert = 280 },
        [TOUCH_CAL_POINT_BOTTOM_RIGHT] = { .raw_horiz = 220, .raw_vert = 280 },
        [TOUCH_CAL_POINT_CENTER] = { .raw_horiz = 120, .raw_vert = 155 },
    };
    touch_calibration_t cal;

    TEST_ASSERT_TRUE(touch_calibration_compute(points, LCD_H_RES, LCD_V_RES, 0, &cal));
    TEST_ASSERT_EQUAL_UINT16(20, cal.horiz_min);
    TEST_ASSERT_EQUAL_UINT16(220, cal.horiz_max);
    TEST_ASSERT_EQUAL_UINT16(30, cal.vert_min);
    TEST_ASSERT_EQUAL_UINT16(280, cal.vert_max);
}

TEST(touch_calibration, compute_averages_slightly_mismatched_corner_pairs)
{
    touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT] = {
        [TOUCH_CAL_POINT_TOP_LEFT] = { .raw_horiz = 18, .raw_vert = 28 },
        [TOUCH_CAL_POINT_TOP_RIGHT] = { .raw_horiz = 222, .raw_vert = 32 },
        [TOUCH_CAL_POINT_BOTTOM_LEFT] = { .raw_horiz = 22, .raw_vert = 282 },
        [TOUCH_CAL_POINT_BOTTOM_RIGHT] = { .raw_horiz = 218, .raw_vert = 278 },
        [TOUCH_CAL_POINT_CENTER] = { .raw_horiz = 120, .raw_vert = 155 },
    };
    touch_calibration_t cal;

    TEST_ASSERT_TRUE(touch_calibration_compute(points, LCD_H_RES, LCD_V_RES, 0, &cal));
    TEST_ASSERT_EQUAL_UINT16(20, cal.horiz_min);  /* (18+22)/2 */
    TEST_ASSERT_EQUAL_UINT16(220, cal.horiz_max); /* (222+218)/2 */
    TEST_ASSERT_EQUAL_UINT16(30, cal.vert_min);   /* (28+32)/2 */
    TEST_ASSERT_EQUAL_UINT16(280, cal.vert_max);  /* (282+278)/2 */
}

TEST(touch_calibration, compute_rejects_a_degenerate_horizontal_span)
{
    touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT] = {
        [TOUCH_CAL_POINT_TOP_LEFT] = { .raw_horiz = 100, .raw_vert = 30 },
        [TOUCH_CAL_POINT_TOP_RIGHT] = { .raw_horiz = 110, .raw_vert = 30 },
        [TOUCH_CAL_POINT_BOTTOM_LEFT] = { .raw_horiz = 105, .raw_vert = 280 },
        [TOUCH_CAL_POINT_BOTTOM_RIGHT] = { .raw_horiz = 112, .raw_vert = 280 },
        [TOUCH_CAL_POINT_CENTER] = { .raw_horiz = 107, .raw_vert = 155 },
    };
    touch_calibration_t cal;
    memset(&cal, 0xFF, sizeof(cal));

    TEST_ASSERT_FALSE(touch_calibration_compute(points, LCD_H_RES, LCD_V_RES, 0, &cal));
}

TEST(touch_calibration, compute_rejects_a_degenerate_vertical_span)
{
    touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT] = {
        [TOUCH_CAL_POINT_TOP_LEFT] = { .raw_horiz = 20, .raw_vert = 150 },
        [TOUCH_CAL_POINT_TOP_RIGHT] = { .raw_horiz = 220, .raw_vert = 152 },
        [TOUCH_CAL_POINT_BOTTOM_LEFT] = { .raw_horiz = 20, .raw_vert = 158 },
        [TOUCH_CAL_POINT_BOTTOM_RIGHT] = { .raw_horiz = 220, .raw_vert = 160 },
        [TOUCH_CAL_POINT_CENTER] = { .raw_horiz = 120, .raw_vert = 155 },
    };
    touch_calibration_t cal;

    TEST_ASSERT_FALSE(touch_calibration_compute(points, LCD_H_RES, LCD_V_RES, 0, &cal));
}

TEST(touch_calibration, compute_rejects_an_imprecise_center_tap)
{
    touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT] = {
        [TOUCH_CAL_POINT_TOP_LEFT] = { .raw_horiz = 20, .raw_vert = 30 },
        [TOUCH_CAL_POINT_TOP_RIGHT] = { .raw_horiz = 220, .raw_vert = 30 },
        [TOUCH_CAL_POINT_BOTTOM_LEFT] = { .raw_horiz = 20, .raw_vert = 280 },
        [TOUCH_CAL_POINT_BOTTOM_RIGHT] = { .raw_horiz = 220, .raw_vert = 280 },
        /* Center tap lands near the left edge instead of the middle. */
        [TOUCH_CAL_POINT_CENTER] = { .raw_horiz = 20, .raw_vert = 155 },
    };
    touch_calibration_t cal;

    TEST_ASSERT_FALSE(touch_calibration_compute(points, LCD_H_RES, LCD_V_RES, 0, &cal));
}

/* Regression coverage for a real bug: touch_calibration_apply() assumes
 * horiz_min/vert_min map to the true screen origin and horiz_max/vert_max
 * map to the true far edge. The calibration screen's corner targets sit 24px
 * in from the edges (crosshairs flush against the very edge would clip and
 * be unreliable to tap), so without the target_margin extrapolation below,
 * compute() stored the raw values measured AT those inset targets directly
 * as horiz_min/max - i.e. it treated the inset targets as if they were the
 * true edges. That systematically overshot away from center: on real
 * hardware, touches near the left edge landed further left than tapped,
 * touches near the right edge landed further right, and touches near center
 * were fine, exactly as this synthetic fixture reproduces. */
TEST(touch_calibration, compute_extrapolates_raw_min_max_past_inset_corner_targets)
{
    /* Synthetic raw = k*screen + c per axis (raw_horiz = 2*x + 10, raw_vert
     * = 3*y + 5), sampled at corner targets inset 24px from the 320x240
     * screen's edges, i.e. at (24,24)/(295,24)/(24,215)/(295,215). */
    touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT] = {
        [TOUCH_CAL_POINT_TOP_LEFT] = { .raw_horiz = 58, .raw_vert = 77 },
        [TOUCH_CAL_POINT_TOP_RIGHT] = { .raw_horiz = 600, .raw_vert = 77 },
        [TOUCH_CAL_POINT_BOTTOM_LEFT] = { .raw_horiz = 58, .raw_vert = 650 },
        [TOUCH_CAL_POINT_BOTTOM_RIGHT] = { .raw_horiz = 600, .raw_vert = 650 },
        [TOUCH_CAL_POINT_CENTER] = { .raw_horiz = 330, .raw_vert = 365 },
    };
    touch_calibration_t cal;

    TEST_ASSERT_TRUE(touch_calibration_compute(points, LCD_H_RES, LCD_V_RES, 24, &cal));
    /* Raw values the true edges (screen 0 and LCD_*_RES-1) would produce
     * under the synthetic formula above, not the raw values actually
     * measured at the inset targets (58/600/77/650). */
    TEST_ASSERT_EQUAL_UINT16(10, cal.horiz_min);
    TEST_ASSERT_EQUAL_UINT16(648, cal.horiz_max);
    TEST_ASSERT_EQUAL_UINT16(5, cal.vert_min);
    TEST_ASSERT_EQUAL_UINT16(722, cal.vert_max);
}

TEST(touch_calibration, compute_with_margin_maps_corner_taps_back_to_their_own_screen_position)
{
    touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT] = {
        [TOUCH_CAL_POINT_TOP_LEFT] = { .raw_horiz = 58, .raw_vert = 77 },
        [TOUCH_CAL_POINT_TOP_RIGHT] = { .raw_horiz = 600, .raw_vert = 77 },
        [TOUCH_CAL_POINT_BOTTOM_LEFT] = { .raw_horiz = 58, .raw_vert = 650 },
        [TOUCH_CAL_POINT_BOTTOM_RIGHT] = { .raw_horiz = 600, .raw_vert = 650 },
        [TOUCH_CAL_POINT_CENTER] = { .raw_horiz = 330, .raw_vert = 365 },
    };
    touch_calibration_t cal;
    TEST_ASSERT_TRUE(touch_calibration_compute(points, LCD_H_RES, LCD_V_RES, 24, &cal));

    uint16_t x, y;

    /* Tapping the top-left target (true screen position (24,24)) must map
     * back to (24,24) - not (0,0), which is what the pre-fix code produced. */
    touch_calibration_apply(&cal, 58, 77, LCD_H_RES, LCD_V_RES, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(24, x);
    TEST_ASSERT_EQUAL_UINT16(24, y);

    /* Same check for the opposite corner: true screen position
     * (LCD_H_RES-1-24, LCD_V_RES-1-24) = (295,215), not (319,239). */
    touch_calibration_apply(&cal, 600, 650, LCD_H_RES, LCD_V_RES, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(295, x);
    TEST_ASSERT_EQUAL_UINT16(215, y);
}

TEST(touch_calibration, compute_rejects_a_margin_too_large_for_the_screen)
{
    touch_calibration_raw_point_t points[TOUCH_CAL_POINT_COUNT] = {
        [TOUCH_CAL_POINT_TOP_LEFT] = { .raw_horiz = 20, .raw_vert = 30 },
        [TOUCH_CAL_POINT_TOP_RIGHT] = { .raw_horiz = 220, .raw_vert = 30 },
        [TOUCH_CAL_POINT_BOTTOM_LEFT] = { .raw_horiz = 20, .raw_vert = 280 },
        [TOUCH_CAL_POINT_BOTTOM_RIGHT] = { .raw_horiz = 220, .raw_vert = 280 },
        [TOUCH_CAL_POINT_CENTER] = { .raw_horiz = 120, .raw_vert = 155 },
    };
    touch_calibration_t cal;

    /* A margin of 160px on each side leaves no usable width on a 320px screen. */
    TEST_ASSERT_FALSE(touch_calibration_compute(points, LCD_H_RES, LCD_V_RES, 160, &cal));
}

TEST_GROUP_RUNNER(touch_calibration)
{
    RUN_TEST_CASE(touch_calibration, set_defaults_populates_the_compiled_in_defaults);
    RUN_TEST_CASE(touch_calibration, apply_maps_calibrated_range_to_full_screen);
    RUN_TEST_CASE(touch_calibration, apply_clamps_raw_values_outside_the_calibrated_range);
    RUN_TEST_CASE(touch_calibration, compute_derives_min_max_from_clean_corner_taps);
    RUN_TEST_CASE(touch_calibration, compute_averages_slightly_mismatched_corner_pairs);
    RUN_TEST_CASE(touch_calibration, compute_rejects_a_degenerate_horizontal_span);
    RUN_TEST_CASE(touch_calibration, compute_rejects_a_degenerate_vertical_span);
    RUN_TEST_CASE(touch_calibration, compute_rejects_an_imprecise_center_tap);
    RUN_TEST_CASE(touch_calibration, compute_extrapolates_raw_min_max_past_inset_corner_targets);
    RUN_TEST_CASE(touch_calibration, compute_with_margin_maps_corner_taps_back_to_their_own_screen_position);
    RUN_TEST_CASE(touch_calibration, compute_rejects_a_margin_too_large_for_the_screen);
}
