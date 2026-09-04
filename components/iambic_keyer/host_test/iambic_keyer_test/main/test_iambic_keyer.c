#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "iambic_keyer.h"

TEST_GROUP(iambic_keyer);

static iambic_keyer_t s_keyer;

/* wpm=20 -> unit_ms = 1200/20 = 60ms. dit=60ms, dah=180ms, inter-element gap=60ms. */
#define TEST_WPM 20
#define U 60u

TEST_SETUP(iambic_keyer)
{
    memset(&s_keyer, 0, sizeof(s_keyer));
}

TEST_TEAR_DOWN(iambic_keyer)
{
}

TEST(iambic_keyer, pure_dit_hold_produces_continuous_dit_train)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_A, TEST_WPM);

    for (uint32_t t = 0; t < 2 * (2u * U); ++t) {
        bool out = iambic_keyer_service(&s_keyer, true, false, t);
        uint32_t phase = t % (2u * U);
        bool expect = phase < U; /* element for the first U ms of each 2U-ms cycle, gap for the rest */
        TEST_ASSERT_EQUAL_MESSAGE(expect, out, "dit-hold cycle mismatch");
    }
}

TEST(iambic_keyer, pure_dah_hold_produces_continuous_dah_train)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_A, TEST_WPM);
    uint32_t cycle = 3u * U + U; /* dah element (3U) + gap (U) */

    for (uint32_t t = 0; t < 2 * cycle; ++t) {
        bool out = iambic_keyer_service(&s_keyer, false, true, t);
        uint32_t phase = t % cycle;
        bool expect = phase < 3u * U;
        TEST_ASSERT_EQUAL_MESSAGE(expect, out, "dah-hold cycle mismatch");
    }
}

TEST(iambic_keyer, squeeze_both_paddles_alternates_dit_dah_continuously)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_A, TEST_WPM);

    bool out;
    /* dit (0..59 true), gap (60..119 false) */
    for (uint32_t t = 0; t <= 59; ++t) {
        out = iambic_keyer_service(&s_keyer, true, true, t);
        TEST_ASSERT_TRUE(out);
    }
    for (uint32_t t = 60; t <= 119; ++t) {
        out = iambic_keyer_service(&s_keyer, true, true, t);
        TEST_ASSERT_FALSE(out);
    }
    /* alternated to dah (120..299 true, 3U=180ms), gap (300..359 false) */
    for (uint32_t t = 120; t <= 299; ++t) {
        out = iambic_keyer_service(&s_keyer, true, true, t);
        TEST_ASSERT_TRUE(out);
    }
    for (uint32_t t = 300; t <= 359; ++t) {
        out = iambic_keyer_service(&s_keyer, true, true, t);
        TEST_ASSERT_FALSE(out);
    }
    /* alternated back to dit (360..419 true) */
    for (uint32_t t = 360; t <= 419; ++t) {
        out = iambic_keyer_service(&s_keyer, true, true, t);
        TEST_ASSERT_TRUE(out);
    }
    out = iambic_keyer_service(&s_keyer, true, true, 420);
    TEST_ASSERT_FALSE(out); /* back to a gap: alternation continues, never stalls */
}

TEST(iambic_keyer, mode_a_drops_a_tap_that_was_released_before_gap_end)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_A, TEST_WPM);

    /* Manually drive dit=true only at t=0 (edge into SEND_DIT), then both
     * paddles false for the rest — dit_contact must stay true for the
     * duration of the dit element for the keyer to have started at all, so
     * drive it explicitly instead of reusing the one-shot helper above. */
    bool out = iambic_keyer_service(&s_keyer, true, false, 0); /* start dit, element_end=60 */
    TEST_ASSERT_TRUE(out);
    for (uint32_t t = 1; t <= 29; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }
    iambic_keyer_service(&s_keyer, true, true, 30); /* dah tapped mid-element -> latched */
    for (uint32_t t = 31; t <= 119; ++t) {
        out = iambic_keyer_service(&s_keyer, false, false, t); /* both released well before gap ends at t=120 */
    }
    TEST_ASSERT_FALSE(out);

    /* Gap ends at t=120 with both paddles released: Mode A must not send the
     * latched dah — it goes straight to idle. */
    out = iambic_keyer_service(&s_keyer, false, false, 120);
    TEST_ASSERT_FALSE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_IDLE, s_keyer.state);

    /* Stays idle indefinitely with no paddles held. */
    out = iambic_keyer_service(&s_keyer, false, false, 200);
    TEST_ASSERT_FALSE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_IDLE, s_keyer.state);
}

TEST(iambic_keyer, mode_b_sends_exactly_one_extra_element_for_a_released_tap)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_B, TEST_WPM);

    bool out = iambic_keyer_service(&s_keyer, true, false, 0); /* start dit, element_end=60 */
    TEST_ASSERT_TRUE(out);
    for (uint32_t t = 1; t <= 29; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }
    iambic_keyer_service(&s_keyer, true, true, 30); /* dah tapped mid-element -> latched */
    for (uint32_t t = 31; t <= 119; ++t) {
        iambic_keyer_service(&s_keyer, false, false, t); /* both released before gap ends at t=120 */
    }

    /* Gap ends at t=120: Mode B sends the one latched extra dah (3U=180ms). */
    out = iambic_keyer_service(&s_keyer, false, false, 120);
    TEST_ASSERT_TRUE(out);
    for (uint32_t t = 121; t <= 299; ++t) {
        out = iambic_keyer_service(&s_keyer, false, false, t);
        TEST_ASSERT_TRUE(out);
    }
    /* Its trailing gap, then idle — no further/second extra. */
    for (uint32_t t = 300; t <= 359; ++t) {
        out = iambic_keyer_service(&s_keyer, false, false, t);
        TEST_ASSERT_FALSE(out);
    }
    out = iambic_keyer_service(&s_keyer, false, false, 360);
    TEST_ASSERT_FALSE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_IDLE, s_keyer.state);
}

TEST(iambic_keyer, straight_key_mode_ignores_dah_and_tracks_dit_directly)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_STRAIGHT, TEST_WPM);

    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, false, false, 0));
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, 1));
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, true, 2)); /* dah paddle asserted too: no effect */
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, false, true, 3)); /* dit released: output follows dit only */
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, false, false, 4));
}

TEST(iambic_keyer, set_wpm_mid_sequence_changes_the_next_elements_duration)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_A, TEST_WPM);

    /* First dit element + gap at the original 60ms unit (see pure_dit_hold test). */
    for (uint32_t t = 0; t <= 119; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }

    /* Halve the speed right at the element/gap boundary (t=120, unit becomes 120ms). */
    iambic_keyer_set_wpm(&s_keyer, 10);

    bool out = iambic_keyer_service(&s_keyer, true, false, 120); /* starts a new dit with the new, larger unit */
    TEST_ASSERT_TRUE(out);
    for (uint32_t t = 121; t <= 239; ++t) {
        out = iambic_keyer_service(&s_keyer, true, false, t);
        TEST_ASSERT_TRUE_MESSAGE(out, "dit element should now last 120ms at 10 WPM");
    }
    out = iambic_keyer_service(&s_keyer, true, false, 240);
    TEST_ASSERT_FALSE(out); /* element ended exactly at the new 120ms unit boundary */
}

TEST_GROUP_RUNNER(iambic_keyer)
{
    RUN_TEST_CASE(iambic_keyer, pure_dit_hold_produces_continuous_dit_train);
    RUN_TEST_CASE(iambic_keyer, pure_dah_hold_produces_continuous_dah_train);
    RUN_TEST_CASE(iambic_keyer, squeeze_both_paddles_alternates_dit_dah_continuously);
    RUN_TEST_CASE(iambic_keyer, mode_a_drops_a_tap_that_was_released_before_gap_end);
    RUN_TEST_CASE(iambic_keyer, mode_b_sends_exactly_one_extra_element_for_a_released_tap);
    RUN_TEST_CASE(iambic_keyer, straight_key_mode_ignores_dah_and_tracks_dit_directly);
    RUN_TEST_CASE(iambic_keyer, set_wpm_mid_sequence_changes_the_next_elements_duration);
}
