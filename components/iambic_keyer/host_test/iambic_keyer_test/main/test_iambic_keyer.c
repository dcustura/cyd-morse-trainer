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

TEST(iambic_keyer, tap_while_holding_primary_paddle_inserts_opposite_element)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_A, TEST_WPM);

    /* Standard squeeze technique: hold dit throughout, briefly tap dah in
     * passing. The tap must be honored even though dit is never released --
     * this must not require full release like the mode-B memory test below,
     * and must work in Mode A too, unlike the dropped-tap test above. */
    bool out = iambic_keyer_service(&s_keyer, true, false, 0); /* start dit, element_end=60 */
    TEST_ASSERT_TRUE(out);
    for (uint32_t t = 1; t <= 29; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }
    iambic_keyer_service(&s_keyer, true, true, 30); /* dah tapped mid-element -> latched */
    for (uint32_t t = 31; t <= 119; ++t) {
        out = iambic_keyer_service(&s_keyer, true, false, t); /* dah released, dit stays held */
    }
    TEST_ASSERT_FALSE(out); /* still in the gap just before it ends */

    /* Gap ends at t=120 with dit still held and dah's tap latched: the dah
     * must be inserted now rather than repeating dit. */
    out = iambic_keyer_service(&s_keyer, true, false, 120);
    TEST_ASSERT_TRUE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_SEND_DAH, s_keyer.state);
    TEST_ASSERT_FALSE(s_keyer.sending_dit);

    for (uint32_t t = 121; t <= 299; ++t) {
        out = iambic_keyer_service(&s_keyer, false, false, t); /* dit released partway through */
        TEST_ASSERT_TRUE_MESSAGE(out, "dah element (180ms) must finish on its own timer");
    }

    /* Both paddles released and nothing new latched: the trailing gap ends cleanly. */
    for (uint32_t t = 300; t <= 359; ++t) {
        out = iambic_keyer_service(&s_keyer, false, false, t);
        TEST_ASSERT_FALSE(out);
    }
    out = iambic_keyer_service(&s_keyer, false, false, 360);
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

TEST(iambic_keyer, ultimatic_single_dit_hold_behaves_like_iambic)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_ULTIMATIC, TEST_WPM);

    for (uint32_t t = 0; t < 2 * (2u * U); ++t) {
        bool out = iambic_keyer_service(&s_keyer, true, false, t);
        uint32_t phase = t % (2u * U);
        bool expect = phase < U; /* element for the first U ms of each 2U-ms cycle, gap for the rest */
        TEST_ASSERT_EQUAL_MESSAGE(expect, out, "ultimatic single-dit-hold cycle mismatch");
    }
}

TEST(iambic_keyer, ultimatic_tap_while_holding_primary_paddle_inserts_opposite_element_once)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_ULTIMATIC, TEST_WPM);

    /* Hold dit throughout; briefly tap dah in passing (standard squeeze
     * technique) and release it well before dit's own gap ends. Ultimatic
     * must still insert the tapped dah once -- exactly like Iambic's
     * squeeze-insert -- even though it never alternates on a live squeeze. */
    iambic_keyer_service(&s_keyer, true, false, 0); /* start dit, element_end=60 */
    for (uint32_t t = 1; t <= 29; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }
    iambic_keyer_service(&s_keyer, true, true, 30); /* dah tapped mid-element -> latched */
    for (uint32_t t = 31; t <= 119; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t); /* dah released, dit stays held */
    }

    /* Gap ends at t=120 with dit still held and dah's tap latched: the dah
     * must be inserted now instead of just repeating dit. */
    bool out = iambic_keyer_service(&s_keyer, true, false, 120);
    TEST_ASSERT_TRUE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_SEND_DAH, s_keyer.state);
    TEST_ASSERT_FALSE(s_keyer.sending_dit);

    /* dit remains held throughout the inserted dah's element+gap: unlike
     * Iambic, Ultimatic must not treat dit's continued presence as reason
     * to alternate back and forth -- the dah's own 180ms timer runs to
     * completion regardless. */
    for (uint32_t t = 121; t <= 299; ++t) {
        out = iambic_keyer_service(&s_keyer, true, false, t);
        TEST_ASSERT_TRUE_MESSAGE(out, "dah element (180ms) must finish on its own timer");
    }
    out = iambic_keyer_service(&s_keyer, true, false, 300); /* dah's trailing gap starts, gap_end=360 */
    TEST_ASSERT_FALSE(out);
    for (uint32_t t = 301; t <= 359; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }

    /* Gap ends: dah's tap is long over and was never re-latched (dit was
     * merely held, never tapped), so this resumes plain dit -- once, not
     * as an alternation -- and it keeps repeating dit for as long as dit
     * alone remains held. */
    out = iambic_keyer_service(&s_keyer, true, false, 360);
    TEST_ASSERT_TRUE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_SEND_DIT, s_keyer.state);
    TEST_ASSERT_TRUE(s_keyer.sending_dit);

    for (uint32_t t = 361; t <= 419; ++t) {
        out = iambic_keyer_service(&s_keyer, true, false, t);
        TEST_ASSERT_TRUE(out);
    }
    out = iambic_keyer_service(&s_keyer, true, false, 420); /* dit's trailing gap */
    TEST_ASSERT_FALSE(out);
    for (uint32_t t = 421; t <= 479; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }
    out = iambic_keyer_service(&s_keyer, true, false, 480);
    TEST_ASSERT_TRUE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_SEND_DIT, s_keyer.state); /* still dit, no alternation */
}

TEST(iambic_keyer, ultimatic_squeeze_repeats_last_pressed_paddle_not_alternating)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_ULTIMATIC, TEST_WPM);

    /* Press dit first (t=0), then dah joins the squeeze at t=10 while dit
     * is still sending: dah is now the more-recently-pressed paddle, so it
     * must win at the next decision point and keep repeating -- unlike
     * Iambic, which would alternate dit/dah every element. */
    iambic_keyer_service(&s_keyer, true, false, 0); /* start dit, element_end=60 */
    for (uint32_t t = 1; t <= 9; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }
    for (uint32_t t = 10; t <= 59; ++t) {
        iambic_keyer_service(&s_keyer, true, true, t); /* dah joins mid-dit-element */
    }
    bool out = iambic_keyer_service(&s_keyer, true, true, 60); /* dit's trailing gap starts, gap_end=120 */
    TEST_ASSERT_FALSE(out);
    for (uint32_t t = 61; t <= 119; ++t) {
        iambic_keyer_service(&s_keyer, true, true, t);
    }

    /* Gap ends at t=120, squeeze still held: dah has priority (its rising
     * edge at t=10 came after dit's at t=0). */
    out = iambic_keyer_service(&s_keyer, true, true, 120);
    TEST_ASSERT_TRUE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_SEND_DAH, s_keyer.state);
    TEST_ASSERT_FALSE(s_keyer.sending_dit);

    for (uint32_t t = 121; t <= 299; ++t) {
        out = iambic_keyer_service(&s_keyer, true, true, t);
        TEST_ASSERT_TRUE(out);
    }
    out = iambic_keyer_service(&s_keyer, true, true, 300); /* dah's trailing gap */
    TEST_ASSERT_FALSE(out);
    for (uint32_t t = 301; t <= 359; ++t) {
        iambic_keyer_service(&s_keyer, true, true, t);
    }

    /* Gap ends again: still dah, no alternation, ever, while squeezed. */
    out = iambic_keyer_service(&s_keyer, true, true, 360);
    TEST_ASSERT_TRUE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_SEND_DAH, s_keyer.state);
    TEST_ASSERT_FALSE(s_keyer.sending_dit);
}

TEST(iambic_keyer, ultimatic_switches_to_still_held_paddle_when_priority_paddle_released)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_ULTIMATIC, TEST_WPM);

    /* Dah held alone first (element_end=180), dit joins at t=1 -- making
     * dit the more-recently-pressed, hence priority, paddle. */
    iambic_keyer_service(&s_keyer, false, true, 0);
    for (uint32_t t = 1; t <= 179; ++t) {
        iambic_keyer_service(&s_keyer, true, true, t);
    }
    bool out = iambic_keyer_service(&s_keyer, true, true, 180); /* gap starts, gap_end=240 */
    TEST_ASSERT_FALSE(out);

    /* Release dah mid-gap; dit stays held. */
    for (uint32_t t = 181; t <= 239; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }

    /* Gap ends: only dit is held now, so Ultimatic switches to dit. */
    out = iambic_keyer_service(&s_keyer, true, false, 240);
    TEST_ASSERT_TRUE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_SEND_DIT, s_keyer.state);
    TEST_ASSERT_TRUE(s_keyer.sending_dit);
}

TEST(iambic_keyer, ultimatic_drops_a_tap_released_before_gap_end_no_memory)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_ULTIMATIC, TEST_WPM);

    /* Same shape as mode_a_drops_a_tap_that_was_released_before_gap_end:
     * dah tapped mid-dit-element and released well before the gap ends,
     * both paddles released for the rest of the gap. Ultimatic has no
     * dot/dash memory at all (not even Mode B's one-shot), so this must
     * go idle, not send a delayed dah. */
    iambic_keyer_service(&s_keyer, true, false, 0); /* start dit, element_end=60 */
    for (uint32_t t = 1; t <= 29; ++t) {
        iambic_keyer_service(&s_keyer, true, false, t);
    }
    iambic_keyer_service(&s_keyer, true, true, 30); /* dah tapped mid-element */
    bool out = false;
    for (uint32_t t = 31; t <= 119; ++t) {
        out = iambic_keyer_service(&s_keyer, false, false, t); /* both released before gap ends at t=120 */
    }
    TEST_ASSERT_FALSE(out);

    out = iambic_keyer_service(&s_keyer, false, false, 120);
    TEST_ASSERT_FALSE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_IDLE, s_keyer.state);
}

TEST(iambic_keyer, ultimatic_simultaneous_first_press_ties_toward_dit)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_ULTIMATIC, TEST_WPM);

    /* Both paddles rise on the very same tick from IDLE with no prior
     * press: the deterministic tie-break must favor dit, matching every
     * other mode's dit-first convention. */
    bool out = iambic_keyer_service(&s_keyer, true, true, 0);
    TEST_ASSERT_TRUE(out);
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_SEND_DIT, s_keyer.state);
    TEST_ASSERT_TRUE(s_keyer.sending_dit);
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

/* Sends one dit from IDLE (press, 1-unit element, 1-unit trailing gap) and
 * returns the time of the key-up edge that ended it -- the reference point
 * Auto Space measures a subsequent pause from. */
static uint32_t send_one_dit_and_settle(iambic_keyer_t *k, uint32_t t)
{
    TEST_ASSERT_TRUE(iambic_keyer_service(k, true, false, t)); /* start_element */
    t += U;
    TEST_ASSERT_FALSE(iambic_keyer_service(k, false, false, t)); /* element ends -> GAP */
    uint32_t key_up_ms = t;
    t += U; /* GAP's own fixed 1-unit wait */
    TEST_ASSERT_FALSE(iambic_keyer_service(k, false, false, t)); /* GAP -> IDLE */
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_IDLE, k->state);
    return key_up_ms;
}

TEST(iambic_keyer, auto_space_disabled_by_default_starts_immediately_after_any_pause)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_B, TEST_WPM);
    uint32_t key_up = send_one_dit_and_settle(&s_keyer, 0);

    /* No call to iambic_keyer_set_auto_space_pct(): must behave exactly
     * like a keyer with no Auto Space concept at all, however long the
     * pause before the next touch. */
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, key_up + 50u * U));
}

TEST(iambic_keyer, auto_space_withholds_a_touch_inside_the_char_gap_window)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_B, TEST_WPM);
    iambic_keyer_set_auto_space_pct(&s_keyer, 70); /* char threshold=2u, standard=3u */
    uint32_t key_up = send_one_dit_and_settle(&s_keyer, 0);

    /* Touched at 2.5 units of silence: inside [2u, 3u), so withheld. */
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, true, false, key_up + 2u * U + U / 2));
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, true, false, key_up + 3u * U - 1));
    /* Exactly at the 3-unit standard char gap, it finally starts. */
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, key_up + 3u * U));
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_SEND_DIT, s_keyer.state);
}

TEST(iambic_keyer, auto_space_latches_a_touch_released_before_the_withhold_ends)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_B, TEST_WPM);
    iambic_keyer_set_auto_space_pct(&s_keyer, 70);
    uint32_t key_up = send_one_dit_and_settle(&s_keyer, 0);

    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, true, false, key_up + 2u * U + 5)); /* touched, withheld */
    /* Paddle fully released well before the target -- must still fire then. */
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, false, false, key_up + 2u * U + 10));
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, false, false, key_up + 3u * U - 1));
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, false, false, key_up + 3u * U));
}

TEST(iambic_keyer, auto_space_leaves_a_pause_below_the_threshold_alone)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_B, TEST_WPM);
    iambic_keyer_set_auto_space_pct(&s_keyer, 70); /* char threshold = 2 units */
    uint32_t key_up = send_one_dit_and_settle(&s_keyer, 0);

    /* 1.5 units of silence: below the 2-unit threshold, so no withhold. */
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, key_up + U + U / 2));
}

TEST(iambic_keyer, auto_space_leaves_a_pause_already_past_standard_alone)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_B, TEST_WPM);
    iambic_keyer_set_auto_space_pct(&s_keyer, 70); /* word threshold = 5 units */
    uint32_t key_up = send_one_dit_and_settle(&s_keyer, 0);

    /* 4 units: already past the 3-unit char standard and below the 5-unit
     * word threshold -- neither window applies, no withhold. */
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, key_up + 4u * U));
}

TEST(iambic_keyer, auto_space_withholds_a_touch_inside_the_word_gap_window)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_B, TEST_WPM);
    iambic_keyer_set_auto_space_pct(&s_keyer, 70); /* word threshold=5u, standard=7u */
    uint32_t key_up = send_one_dit_and_settle(&s_keyer, 0);

    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, true, false, key_up + 6u * U));
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, true, false, key_up + 7u * U - 1));
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, key_up + 7u * U));
}

TEST(iambic_keyer, auto_space_pct_persists_across_set_mode)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_A, TEST_WPM);
    iambic_keyer_set_auto_space_pct(&s_keyer, 70);
    iambic_keyer_set_mode(&s_keyer, IAMBIC_KEYER_MODE_B); /* must not reset pct back to disabled */

    uint32_t key_up = send_one_dit_and_settle(&s_keyer, 0);
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, true, false, key_up + 2u * U + 5));
}

TEST(iambic_keyer, auto_space_has_no_effect_in_straight_key_mode)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_STRAIGHT, TEST_WPM);
    iambic_keyer_set_auto_space_pct(&s_keyer, 50); /* even an aggressive setting */

    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, 0));
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, false, false, 1));
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, 2)); /* immediate, no withhold */
}

TEST(iambic_keyer, auto_space_ultimatic_withholds_like_iambic)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_ULTIMATIC, TEST_WPM);
    iambic_keyer_set_auto_space_pct(&s_keyer, 70);
    uint32_t key_up = send_one_dit_and_settle(&s_keyer, 0);

    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, true, false, key_up + 2u * U + U / 2));
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, key_up + 3u * U));
}

TEST(iambic_keyer, auto_space_reference_survives_a_wpm_change_during_the_trailing_gap)
{
    iambic_keyer_init(&s_keyer, IAMBIC_KEYER_MODE_B, TEST_WPM);
    iambic_keyer_set_auto_space_pct(&s_keyer, 70);

    /* Drive the dit and its trailing gap manually (not via
     * send_one_dit_and_settle(), which doesn't leave room to change WPM
     * mid-gap): element ends at t=60 (key-up edge), gap_end_ms=120. */
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, 0));
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, false, false, U));
    uint32_t key_up_ms = U;

    /* Change WPM mid-gap, well before gap_end_ms=120 -- unit_ms doubles to
     * 120, but gap_end_ms itself was already scheduled at the old unit_ms
     * and isn't retroactively recomputed (same as every other in-flight
     * timer in this keyer). */
    iambic_keyer_set_wpm(&s_keyer, TEST_WPM / 2);

    /* GAP still ends at its originally-scheduled t=120, transitioning to
     * IDLE there. idle_start_ms must be the real key-up edge (60), not a
     * value skewed by the now-doubled unit_ms. */
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, false, false, 2u * U));
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_STATE_IDLE, s_keyer.state);
    TEST_ASSERT_EQUAL_UINT32(key_up_ms, s_keyer.idle_start_ms);

    /* Classify a subsequent pause using the NEW (120ms) unit: 2.5 new-units
     * of silence from the real key-up edge is still inside the char-gap
     * withhold window, same as it would be at any fixed WPM. */
    uint32_t new_unit = 2u * U;
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, true, false, key_up_ms + 2u * new_unit + new_unit / 2));
    TEST_ASSERT_FALSE(iambic_keyer_service(&s_keyer, true, false, key_up_ms + 3u * new_unit - 1));
    TEST_ASSERT_TRUE(iambic_keyer_service(&s_keyer, true, false, key_up_ms + 3u * new_unit));
}

TEST_GROUP_RUNNER(iambic_keyer)
{
    RUN_TEST_CASE(iambic_keyer, pure_dit_hold_produces_continuous_dit_train);
    RUN_TEST_CASE(iambic_keyer, pure_dah_hold_produces_continuous_dah_train);
    RUN_TEST_CASE(iambic_keyer, squeeze_both_paddles_alternates_dit_dah_continuously);
    RUN_TEST_CASE(iambic_keyer, mode_a_drops_a_tap_that_was_released_before_gap_end);
    RUN_TEST_CASE(iambic_keyer, tap_while_holding_primary_paddle_inserts_opposite_element);
    RUN_TEST_CASE(iambic_keyer, mode_b_sends_exactly_one_extra_element_for_a_released_tap);
    RUN_TEST_CASE(iambic_keyer, ultimatic_single_dit_hold_behaves_like_iambic);
    RUN_TEST_CASE(iambic_keyer, ultimatic_tap_while_holding_primary_paddle_inserts_opposite_element_once);
    RUN_TEST_CASE(iambic_keyer, ultimatic_squeeze_repeats_last_pressed_paddle_not_alternating);
    RUN_TEST_CASE(iambic_keyer, ultimatic_switches_to_still_held_paddle_when_priority_paddle_released);
    RUN_TEST_CASE(iambic_keyer, ultimatic_drops_a_tap_released_before_gap_end_no_memory);
    RUN_TEST_CASE(iambic_keyer, ultimatic_simultaneous_first_press_ties_toward_dit);
    RUN_TEST_CASE(iambic_keyer, straight_key_mode_ignores_dah_and_tracks_dit_directly);
    RUN_TEST_CASE(iambic_keyer, set_wpm_mid_sequence_changes_the_next_elements_duration);
    RUN_TEST_CASE(iambic_keyer, auto_space_disabled_by_default_starts_immediately_after_any_pause);
    RUN_TEST_CASE(iambic_keyer, auto_space_withholds_a_touch_inside_the_char_gap_window);
    RUN_TEST_CASE(iambic_keyer, auto_space_latches_a_touch_released_before_the_withhold_ends);
    RUN_TEST_CASE(iambic_keyer, auto_space_leaves_a_pause_below_the_threshold_alone);
    RUN_TEST_CASE(iambic_keyer, auto_space_leaves_a_pause_already_past_standard_alone);
    RUN_TEST_CASE(iambic_keyer, auto_space_withholds_a_touch_inside_the_word_gap_window);
    RUN_TEST_CASE(iambic_keyer, auto_space_pct_persists_across_set_mode);
    RUN_TEST_CASE(iambic_keyer, auto_space_has_no_effect_in_straight_key_mode);
    RUN_TEST_CASE(iambic_keyer, auto_space_ultimatic_withholds_like_iambic);
    RUN_TEST_CASE(iambic_keyer, auto_space_reference_survives_a_wpm_change_during_the_trailing_gap);
}
