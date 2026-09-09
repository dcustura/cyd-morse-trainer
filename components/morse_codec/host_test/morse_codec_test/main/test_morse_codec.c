#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "morse_codec.h"

TEST_GROUP(morse_codec);

static morse_codec_t s_codec;

/* wpm=20 -> unit_ms = 1200/20 = 60ms. Thresholds: dit/dah boundary at
 * 2*unit=120ms, char-gap starts at 120ms, word-gap starts at 5*unit=300ms. */
#define TEST_WPM 20
#define TEST_UNIT_MS 60u

TEST_SETUP(morse_codec)
{
    memset(&s_codec, 0, sizeof(s_codec));
    morse_codec_init(&s_codec, TEST_WPM);
}

TEST_TEAR_DOWN(morse_codec)
{
}

TEST(morse_codec, element_duration_equal_to_two_units_is_dit)
{
    char out = 0;
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, true, 0, &out));
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, false, 2 * TEST_UNIT_MS, &out));
    /* Flush via a long gap on the next key-down: a lone dit decodes as 'E'. */
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_CHAR, morse_codec_key_event(&s_codec, true, 2 * TEST_UNIT_MS + 5 * TEST_UNIT_MS, &out));
    TEST_ASSERT_EQUAL('E', out);
}

TEST(morse_codec, element_duration_over_two_units_is_dah)
{
    char out = 0;
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, true, 0, &out));
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, false, 2 * TEST_UNIT_MS + 1, &out));
    /* Flush via tick: a lone dah decodes as 'T'. */
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_CHAR, morse_codec_tick(&s_codec, 2 * TEST_UNIT_MS + 1 + 2 * TEST_UNIT_MS, &out));
    TEST_ASSERT_EQUAL('T', out);
}

TEST(morse_codec, char_gap_boundary_flushes_pending_character)
{
    char out = 0;
    uint32_t t = 0;
    morse_codec_key_event(&s_codec, true, t, &out);
    t += TEST_UNIT_MS;
    morse_codec_key_event(&s_codec, false, t, &out); /* one dit recorded */

    /* Gap of exactly (2*unit - 1) ms: still intra-character, no flush. */
    t += (2 * TEST_UNIT_MS - 1);
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, true, t, &out));
    t += TEST_UNIT_MS;
    morse_codec_key_event(&s_codec, false, t, &out); /* second dit recorded, still same character */

    /* Gap of exactly 2*unit ms: character-gap length, flush now ('I' = ..). */
    t += 2 * TEST_UNIT_MS;
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_CHAR, morse_codec_key_event(&s_codec, true, t, &out));
    TEST_ASSERT_EQUAL('I', out);
}

TEST(morse_codec, tick_emits_space_after_five_units_of_silence)
{
    char out = 0;
    uint32_t t = 0;
    morse_codec_key_event(&s_codec, true, t, &out);
    t += TEST_UNIT_MS;
    morse_codec_key_event(&s_codec, false, t, &out); /* one dit */

    /* Flush the character first via tick at the char-gap boundary: a lone dit is 'E'. */
    t += 2 * TEST_UNIT_MS;
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_CHAR, morse_codec_tick(&s_codec, t, &out));
    TEST_ASSERT_EQUAL('E', out);

    /* Before 5 units of total silence (measured from the key-up edge), no SPACE. */
    uint32_t silence_start = TEST_UNIT_MS; /* the key-up timestamp */
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_tick(&s_codec, silence_start + 5 * TEST_UNIT_MS - 1, &out));

    /* At 5 units of silence, SPACE fires exactly once. */
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_SPACE, morse_codec_tick(&s_codec, silence_start + 5 * TEST_UNIT_MS, &out));
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_tick(&s_codec, silence_start + 6 * TEST_UNIT_MS, &out));
}

TEST(morse_codec, decodes_sos_over_a_full_paris_timed_sequence)
{
    char out = 0;
    uint32_t t = 0;

    /* S = ... : three dits, 1-unit intra-element gaps. */
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, true, t, &out));
        t += TEST_UNIT_MS;
        TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, false, t, &out));
        t += TEST_UNIT_MS; /* 1-unit gap before next element/character */
    }

    /* Character gap (3 units total silence) before O; already advanced 1 unit above. */
    t += 2 * TEST_UNIT_MS;
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_CHAR, morse_codec_key_event(&s_codec, true, t, &out));
    TEST_ASSERT_EQUAL('S', out);

    /* O = --- : three dahs. */
    for (int i = 0; i < 3; ++i) {
        if (i > 0) {
            TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, true, t, &out));
        }
        t += 3 * TEST_UNIT_MS;
        TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, false, t, &out));
        t += TEST_UNIT_MS;
    }

    t += 2 * TEST_UNIT_MS;
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_CHAR, morse_codec_key_event(&s_codec, true, t, &out));
    TEST_ASSERT_EQUAL('O', out);

    /* S = ... again. */
    for (int i = 0; i < 3; ++i) {
        if (i > 0) {
            TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, true, t, &out));
        }
        t += TEST_UNIT_MS;
        TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, false, t, &out));
        t += TEST_UNIT_MS;
    }

    /* No further key-down: flush the final S via tick. */
    t += 2 * TEST_UNIT_MS;
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_CHAR, morse_codec_tick(&s_codec, t, &out));
    TEST_ASSERT_EQUAL('S', out);
}

TEST(morse_codec, unknown_sequence_returns_unknown)
{
    char out = 0;
    uint32_t t = 0;

    /* Six dits doesn't match any table entry. */
    for (int i = 0; i < 6; ++i) {
        morse_codec_key_event(&s_codec, true, t, &out);
        t += TEST_UNIT_MS;
        morse_codec_key_event(&s_codec, false, t, &out);
        t += TEST_UNIT_MS;
    }

    t += 2 * TEST_UNIT_MS;
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_UNKNOWN, morse_codec_tick(&s_codec, t, &out));
    TEST_ASSERT_EQUAL(MORSE_CODEC_UNKNOWN_CHAR, out);
}

/* Feeds one already-classified dot/dash sequence (e.g. "-.-.--") through the
 * codec as key events with 1-unit intra-character gaps, then flushes via the
 * trailing character-gap silence. Mirrors decodes_sos_over_a_full_paris_timed_sequence
 * but generalized to any sequence, for exercising punctuation/prosign entries
 * without repeating the full timed dance per character. */
static char decode_sequence(const char *seq)
{
    char out = 0;
    uint32_t t = 1000;

    for (const char *p = seq; *p != '\0'; ++p) {
        TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, true, t, &out));
        t += (*p == '.') ? TEST_UNIT_MS : 3 * TEST_UNIT_MS;
        TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_NONE, morse_codec_key_event(&s_codec, false, t, &out));
        t += TEST_UNIT_MS; /* 1-unit intra-character gap */
    }

    t += 2 * TEST_UNIT_MS; /* extend the trailing gap to character-gap length */
    morse_codec_event_t event = morse_codec_tick(&s_codec, t, &out);
    TEST_ASSERT_TRUE(event == MORSE_CODEC_EVENT_CHAR || event == MORSE_CODEC_EVENT_UNKNOWN);
    return out;
}

TEST(morse_codec, decodes_punctuation)
{
    morse_codec_reset(&s_codec);
    TEST_ASSERT_EQUAL('.', decode_sequence(".-.-.-"));
    morse_codec_reset(&s_codec);
    TEST_ASSERT_EQUAL(',', decode_sequence("--..--"));
    morse_codec_reset(&s_codec);
    TEST_ASSERT_EQUAL('?', decode_sequence("..--.."));
    morse_codec_reset(&s_codec);
    TEST_ASSERT_EQUAL('/', decode_sequence("-..-."));
    morse_codec_reset(&s_codec);
    TEST_ASSERT_EQUAL('@', decode_sequence(".--.-."));
}

TEST(morse_codec, prosigns_sharing_a_punctuation_sequence_decode_to_that_punctuation)
{
    morse_codec_reset(&s_codec);
    TEST_ASSERT_EQUAL('=', decode_sequence("-...-"));   /* BT */
    morse_codec_reset(&s_codec);
    TEST_ASSERT_EQUAL('+', decode_sequence(".-.-."));   /* AR */
    morse_codec_reset(&s_codec);
    TEST_ASSERT_EQUAL('(', decode_sequence("-.--."));   /* KN */
    morse_codec_reset(&s_codec);
    TEST_ASSERT_EQUAL('&', decode_sequence(".-..."));   /* AS */
}

TEST(morse_codec, unique_prosigns_decode_to_a_named_sentinel)
{
    morse_codec_reset(&s_codec);
    char sk = decode_sequence("...-.-");
    TEST_ASSERT_EQUAL(MORSE_CODEC_PROSIGN_SK, sk);
    TEST_ASSERT_EQUAL_STRING("SK", morse_codec_prosign_name(sk));

    morse_codec_reset(&s_codec);
    char hh = decode_sequence("........");
    TEST_ASSERT_EQUAL(MORSE_CODEC_PROSIGN_HH, hh);
    TEST_ASSERT_EQUAL_STRING("HH", morse_codec_prosign_name(hh));

    morse_codec_reset(&s_codec);
    char ve = decode_sequence("...-.");
    TEST_ASSERT_EQUAL(MORSE_CODEC_PROSIGN_VE, ve);
    TEST_ASSERT_EQUAL_STRING("VE", morse_codec_prosign_name(ve));

    morse_codec_reset(&s_codec);
    char ct = decode_sequence("-.-.-");
    TEST_ASSERT_EQUAL(MORSE_CODEC_PROSIGN_CT, ct);
    TEST_ASSERT_EQUAL_STRING("CT", morse_codec_prosign_name(ct));

    TEST_ASSERT_NULL(morse_codec_prosign_name('A'));
    TEST_ASSERT_NULL(morse_codec_prosign_name(MORSE_CODEC_UNKNOWN_CHAR));
}

TEST(morse_codec, set_wpm_mid_stream_changes_subsequent_classification)
{
    char out = 0;
    uint32_t t = 0;

    /* At 20 WPM (unit=60ms), a 60ms element is a dit. */
    morse_codec_key_event(&s_codec, true, t, &out);
    t += TEST_UNIT_MS;
    morse_codec_key_event(&s_codec, false, t, &out);
    t += 2 * TEST_UNIT_MS;
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_CHAR, morse_codec_key_event(&s_codec, true, t, &out)); /* flush lone dit -> 'E' */
    TEST_ASSERT_EQUAL('E', out);

    /* Halve the speed to 10 WPM (unit=120ms): the same 60ms element is now a dit
     * (still <= 2*120=240ms), but a 150ms element that was a dah at 20 WPM
     * (150 > 120) is now a dit at 10 WPM (150 <= 240). */
    morse_codec_set_wpm(&s_codec, 10);
    t += 10;
    morse_codec_key_event(&s_codec, true, t, &out);
    t += 150;
    morse_codec_key_event(&s_codec, false, t, &out);
    t += 5 * 120; /* flush via a long enough gap at the new, larger unit size */
    TEST_ASSERT_EQUAL(MORSE_CODEC_EVENT_CHAR, morse_codec_tick(&s_codec, t, &out));
    TEST_ASSERT_EQUAL('E', out); /* the 150ms element classified as a dit at 10 WPM -> lone dit -> 'E' */
}

TEST_GROUP_RUNNER(morse_codec)
{
    RUN_TEST_CASE(morse_codec, element_duration_equal_to_two_units_is_dit);
    RUN_TEST_CASE(morse_codec, element_duration_over_two_units_is_dah);
    RUN_TEST_CASE(morse_codec, char_gap_boundary_flushes_pending_character);
    RUN_TEST_CASE(morse_codec, tick_emits_space_after_five_units_of_silence);
    RUN_TEST_CASE(morse_codec, decodes_sos_over_a_full_paris_timed_sequence);
    RUN_TEST_CASE(morse_codec, unknown_sequence_returns_unknown);
    RUN_TEST_CASE(morse_codec, set_wpm_mid_stream_changes_subsequent_classification);
    RUN_TEST_CASE(morse_codec, decodes_punctuation);
    RUN_TEST_CASE(morse_codec, prosigns_sharing_a_punctuation_sequence_decode_to_that_punctuation);
    RUN_TEST_CASE(morse_codec, unique_prosigns_decode_to_a_named_sentinel);
}
