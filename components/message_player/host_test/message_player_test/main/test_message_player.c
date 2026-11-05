#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "message_player.h"
#include "morse_codec.h"

TEST_GROUP(message_player);

/* 20 wpm: unit 60 ms, dah 180, char gap 180, word gap 420. */
#define U 60u

static message_player_t s_p;

/* Key edges seen by run(): alternating down/up times, starting with down. */
#define MAX_EDGES 64
static uint32_t s_edges[MAX_EDGES];
static size_t s_edge_count;
static bool s_key;

TEST_SETUP(message_player)
{
    message_player_init(&s_p, 20);
    s_edge_count = 0;
    s_key = false;
}

TEST_TEAR_DOWN(message_player)
{
}

/* Service every 1 ms over [from_ms, to_ms), recording key edges. */
static void run(uint32_t from_ms, uint32_t to_ms)
{
    for (uint32_t t = from_ms; t < to_ms; ++t) {
        bool key = message_player_service(&s_p, t);
        if (key != s_key) {
            TEST_ASSERT_TRUE_MESSAGE(s_edge_count < MAX_EDGES, "too many edges");
            s_edges[s_edge_count++] = t;
            s_key = key;
        }
    }
}

static message_player_message_t msg(const char *text, int tag)
{
    message_player_message_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.text, text, MESSAGE_PLAYER_TEXT_MAX);
    m.pause_s = 1;
    m.tag = tag;
    return m;
}

static message_player_message_t repeating(const char *text, int tag, uint8_t count, uint8_t pause_s)
{
    message_player_message_t m = msg(text, tag);
    m.repeat = true;
    m.count = count;
    m.pause_s = pause_s;
    return m;
}

static void enqueue_ok(message_player_message_t m, uint32_t now_ms, message_player_enqueue_result_t expected)
{
    TEST_ASSERT_EQUAL(expected, message_player_enqueue(&s_p, &m, now_ms));
}

static void assert_edges(const uint32_t *expected, size_t n)
{
    TEST_ASSERT_EQUAL_UINT(n, s_edge_count);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, s_edges, n);
}

TEST(message_player, sends_elements_and_gaps_with_paris_timing)
{
    /* A = .-  then char gap, N = -.  then word gap, E = . */
    enqueue_ok(msg("AN E", 1), 0, MESSAGE_PLAYER_STARTED);
    run(0, 3000);
    const uint32_t expected[] = {
        0,   U,               /* A: dit */
        2 * U, 5 * U,         /* A: dah after element gap */
        8 * U, 11 * U,        /* N: dah after char gap */
        12 * U, 13 * U,       /* N: dit */
        20 * U, 21 * U,       /* E after word gap */
    };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
    TEST_ASSERT_FALSE(message_player_is_active(&s_p));
}

TEST(message_player, prosign_letters_run_together)
{
    enqueue_ok(msg("<AR>", 1), 0, MESSAGE_PLAYER_STARTED); /* .-.-. */
    run(0, 2000);
    const uint32_t expected[] = { 0, U, 2 * U, 5 * U, 6 * U, 7 * U, 8 * U, 11 * U, 12 * U, 13 * U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, wpm_change_applies_from_the_next_element)
{
    enqueue_ok(msg("EE", 1), 0, MESSAGE_PLAYER_STARTED);
    run(0, 30);
    message_player_set_wpm(&s_p, 10); /* unit 120 */
    run(30, 2000);
    /* First dit keeps its 60 ms; the char gap and second dit use 120 ms units. */
    const uint32_t expected[] = { 0, 60, 60 + 360, 60 + 360 + 120 };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, repeat_count_includes_the_first_send_and_pauses_between)
{
    enqueue_ok(repeating("E", 1, 3, 2), 0, MESSAGE_PLAYER_STARTED);
    run(0, 10000);
    const uint32_t expected[] = { 0, U, U + 2000, 2 * U + 2000, 2 * U + 4000, 3 * U + 4000 };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
    TEST_ASSERT_FALSE(message_player_is_active(&s_p));
}

TEST(message_player, count_zero_repeats_until_stopped)
{
    enqueue_ok(repeating("E", 1, 0, 1), 0, MESSAGE_PLAYER_STARTED);
    run(0, 10500);
    TEST_ASSERT_TRUE(message_player_is_active(&s_p));
    TEST_ASSERT_EQUAL_UINT(2 * 10, s_edge_count); /* sends at 0, 1060, ..., 9540 */
    message_player_stop(&s_p);
    TEST_ASSERT_FALSE(message_player_is_active(&s_p));
}

TEST(message_player, stop_cuts_an_element_short_and_clears_the_queue)
{
    enqueue_ok(msg("T", 1), 0, MESSAGE_PLAYER_STARTED);
    enqueue_ok(msg("E", 2), 1, MESSAGE_PLAYER_QUEUED);
    run(0, 50);
    TEST_ASSERT_TRUE(s_key);
    message_player_stop(&s_p);
    run(50, 3000);
    const uint32_t expected[] = { 0, 50 }; /* key up at once, nothing after */
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
    message_player_status_t st;
    message_player_get_status(&s_p, 3000, &st);
    TEST_ASSERT_FALSE(st.active);
    TEST_ASSERT_EQUAL_UINT8(0, st.queue_len);
}

TEST(message_player, queued_message_follows_after_a_word_gap)
{
    enqueue_ok(msg("E", 1), 0, MESSAGE_PLAYER_STARTED);
    enqueue_ok(msg("T", 2), 10, MESSAGE_PLAYER_QUEUED);
    run(0, 3000);
    const uint32_t expected[] = { 0, U, U + 7 * U, U + 7 * U + 3 * U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, queueing_ends_a_repeating_message_after_its_current_send)
{
    enqueue_ok(repeating("E", 1, 0, 5), 0, MESSAGE_PLAYER_STARTED);
    enqueue_ok(msg("T", 2), 10, MESSAGE_PLAYER_QUEUED);
    run(0, 10000);
    const uint32_t expected[] = { 0, U, 8 * U, 11 * U }; /* no 5 s pause, no repeat */
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
    TEST_ASSERT_FALSE(message_player_is_active(&s_p));
}

TEST(message_player, queueing_during_a_pause_starts_the_next_message_promptly)
{
    enqueue_ok(repeating("E", 1, 0, 5), 0, MESSAGE_PLAYER_STARTED);
    run(0, 2000);
    /* The word gap after the last send has long passed: start right away. */
    enqueue_ok(msg("T", 2), 2000, MESSAGE_PLAYER_STARTED);
    run(2000, 6000);
    const uint32_t expected[] = { 0, U, 2000, 2000 + 3 * U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, queueing_early_in_a_pause_still_waits_a_word_gap)
{
    enqueue_ok(repeating("E", 1, 0, 5), 0, MESSAGE_PLAYER_STARTED);
    run(0, 100);
    enqueue_ok(msg("T", 2), 100, MESSAGE_PLAYER_STARTED);
    run(100, 3000);
    const uint32_t expected[] = { 0, U, 8 * U, 11 * U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, enqueue_ignores_duplicates_full_queue_and_empty_text)
{
    enqueue_ok(msg("", 9), 0, MESSAGE_PLAYER_INVALID);
    enqueue_ok(msg("   ", 9), 0, MESSAGE_PLAYER_INVALID);
    enqueue_ok(msg("E", 1), 0, MESSAGE_PLAYER_STARTED);
    enqueue_ok(msg("E", 1), 0, MESSAGE_PLAYER_IGNORED_PLAYING);
    for (int tag = 2; tag < 2 + MESSAGE_PLAYER_QUEUE_MAX; ++tag) {
        enqueue_ok(msg("E", tag), 0, MESSAGE_PLAYER_QUEUED);
    }
    enqueue_ok(msg("E", 2), 0, MESSAGE_PLAYER_IGNORED_QUEUED);
    enqueue_ok(msg("E", 99), 0, MESSAGE_PLAYER_QUEUE_FULL);

    message_player_status_t st;
    message_player_get_status(&s_p, 0, &st);
    TEST_ASSERT_EQUAL_UINT8(MESSAGE_PLAYER_QUEUE_MAX, st.queue_len);
    TEST_ASSERT_EQUAL_INT(2, st.queue_tags[0]);
    TEST_ASSERT_EQUAL_INT(7, st.queue_tags[5]);
}

TEST(message_player, repeat_on_during_a_single_send_makes_it_repeat)
{
    enqueue_ok(msg("E", 1), 0, MESSAGE_PLAYER_STARTED);
    message_player_set_repeat_params(&s_p, 2, 1, 0);
    message_player_set_repeat(&s_p, true);
    run(0, 5000);
    const uint32_t expected[] = { 0, U, U + 1000, 2 * U + 1000 };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, repeat_off_during_a_send_lets_it_finish)
{
    enqueue_ok(repeating("T", 1, 0, 1), 0, MESSAGE_PLAYER_STARTED);
    run(0, 100);
    message_player_set_repeat(&s_p, false);
    run(100, 5000);
    const uint32_t expected[] = { 0, 3 * U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, repeat_off_during_a_pause_stops_at_once)
{
    enqueue_ok(repeating("E", 1, 0, 5), 0, MESSAGE_PLAYER_STARTED);
    run(0, 1000);
    TEST_ASSERT_TRUE(message_player_is_active(&s_p));
    message_player_set_repeat(&s_p, false);
    TEST_ASSERT_FALSE(message_player_is_active(&s_p));
}

TEST(message_player, repeat_params_during_a_pause_apply_from_the_pause_start)
{
    enqueue_ok(repeating("E", 1, 0, 5), 0, MESSAGE_PLAYER_STARTED);
    run(0, 1000);
    message_player_set_repeat_params(&s_p, 0, 2, 1000); /* pause now ends at 60 + 2000 */
    run(1000, 2200);
    const uint32_t expected[] = { 0, U, U + 2000, 2 * U + 2000 };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));

    /* Two sends done; lowering the count to 2 during the next pause stops. */
    run(2200, 2500);
    message_player_set_repeat_params(&s_p, 2, 2, 2500);
    TEST_ASSERT_FALSE(message_player_is_active(&s_p));
}

TEST(message_player, shortening_a_pause_that_already_ran_out_sends_next_at_once)
{
    enqueue_ok(repeating("E", 1, 0, 30), 0, MESSAGE_PLAYER_STARTED);
    run(0, 10000);
    /* 5 s from the pause start (60) is already past at 10000: the next
     * send starts now and is actually keyed, not skipped. */
    message_player_set_repeat_params(&s_p, 0, 5, 10000);
    run(10000, 10200);
    const uint32_t expected[] = { 0, U, 10000, 10000 + U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, enqueue_reads_an_unterminated_text_only_up_to_its_limit)
{
    /* All spaces with no NUL in the array: nothing to send, and the
     * tokenizer must stop at the array's end, not run into later fields. */
    message_player_message_t m = repeating("", 1, 0, 1);
    memset(m.text, ' ', sizeof(m.text));
    enqueue_ok(m, 0, MESSAGE_PLAYER_INVALID);

    /* Full of letters with no NUL: sends the first MESSAGE_PLAYER_TEXT_MAX. */
    m = msg("", 2);
    memset(m.text, 'E', sizeof(m.text));
    enqueue_ok(m, 0, MESSAGE_PLAYER_STARTED);
    message_player_status_t st;
    message_player_get_status(&s_p, 0, &st);
    TEST_ASSERT_EQUAL_size_t(MESSAGE_PLAYER_TEXT_MAX, strlen(st.text));
}

TEST(message_player, start_waits_a_word_gap_after_the_operators_key)
{
    /* Operator's key down until 1000 (mid-element): the message keys up
     * at once and starts a word gap after the operator's last key-down. */
    message_player_note_key(&s_p, true, 999);
    enqueue_ok(msg("E", 1), 1000, MESSAGE_PLAYER_STARTED);
    TEST_ASSERT_TRUE(message_player_is_active(&s_p));
    run(1000, 2000);
    const uint32_t expected[] = { 999 + 7 * U, 999 + 8 * U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, start_counts_the_word_gap_from_the_operators_last_key_down)
{
    /* Released 5 units ago: only the rest of the word gap is waited. */
    message_player_note_key(&s_p, true, 1000);
    enqueue_ok(msg("E", 1), 1000 + 5 * U, MESSAGE_PLAYER_STARTED);
    run(1000 + 5 * U, 2000);
    const uint32_t expected[] = { 1000 + 7 * U, 1000 + 8 * U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, start_is_immediate_once_the_operators_key_has_been_idle_a_word_gap)
{
    message_player_note_key(&s_p, true, 1000);
    enqueue_ok(msg("E", 1), 1000 + 7 * U, MESSAGE_PLAYER_STARTED);
    run(1000 + 7 * U, 2000);
    const uint32_t expected[] = { 1000 + 7 * U, 1000 + 8 * U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, a_key_down_a_word_gap_old_is_forgotten)
{
    message_player_note_key(&s_p, true, 1000);
    message_player_note_key(&s_p, false, 1000 + 7 * U); /* forgotten here */
    /* 2^32 ms later the counter reads 1000 again: remembered, that key-down
     * would look 0 ms old and delay the start by a word gap. */
    uint32_t wrapped = 1000;
    enqueue_ok(msg("E", 1), wrapped, MESSAGE_PLAYER_STARTED);
    run(wrapped, wrapped + 2 * U);
    const uint32_t expected[] = { wrapped, wrapped + U };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, weight_lengthens_elements_and_shortens_the_gaps_after_them)
{
    message_player_set_weight_pct(&s_p, 75); /* +U/2 on, -U/2 off */
    enqueue_ok(msg("EE T", 1), 0, MESSAGE_PLAYER_STARTED);
    run(0, 3000);
    const uint32_t on = U + U / 2;
    const uint32_t expected[] = {
        0, on,                                        /* E */
        on + 3 * U - U / 2, 2 * on + 3 * U - U / 2,   /* E after a weighted char gap */
        2 * on + 10 * U - U, 2 * on + 10 * U - U + 3 * U + U / 2, /* T after a weighted word gap */
    };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, weight_below_standard_shortens_elements)
{
    message_player_set_weight_pct(&s_p, 25); /* -U/2 on, +U/2 off */
    enqueue_ok(msg("A", 1), 0, MESSAGE_PLAYER_STARTED);
    run(0, 1000);
    const uint32_t expected[] = { 0, U / 2, 2 * U, 2 * U + 3 * U - U / 2 };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, weight_change_mid_element_keeps_that_elements_total)
{
    message_player_set_weight_pct(&s_p, 75);
    enqueue_ok(msg("I", 1), 0, MESSAGE_PLAYER_STARTED);
    run(0, 10);                               /* first dit keyed: 1.5u */
    message_player_set_weight_pct(&s_p, 25);  /* before its key-up */
    run(10, 1000);
    /* The gap gives back that dit's 0.5u; the next dit is lighter (0.5u). */
    const uint32_t expected[] = { 0, U + U / 2, 2 * U, 2 * U + U / 2 };
    assert_edges(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(message_player, weight_is_clamped)
{
    message_player_set_weight_pct(&s_p, 0);
    TEST_ASSERT_EQUAL_UINT8(25, s_p.weight_pct);
    message_player_set_weight_pct(&s_p, 100);
    TEST_ASSERT_EQUAL_UINT8(75, s_p.weight_pct);
}

/* Plays text at 20 WPM with the given weight through the real decoder,
 * returning what it decoded (word gaps as spaces). */
static void decode_played(uint8_t weight_pct, const char *text, char *out, size_t out_size)
{
    morse_codec_t codec;
    morse_codec_init(&codec, 20);
    message_player_set_weight_pct(&s_p, weight_pct);
    enqueue_ok(msg(text, 1), 0, MESSAGE_PLAYER_STARTED);

    size_t n = 0;
    bool key = false;
    for (uint32_t t = 0; t < 30000 && n + 1 < out_size; ++t) {
        bool now_key = message_player_service(&s_p, t);
        char ch = 0;
        morse_codec_event_t ev = (now_key != key) ? morse_codec_key_event(&codec, now_key, t, &ch)
                                                  : morse_codec_tick(&codec, t, &ch);
        key = now_key;
        if (ev == MORSE_CODEC_EVENT_CHAR || ev == MORSE_CODEC_EVENT_UNKNOWN) {
            out[n++] = ch;
        } else if (ev == MORSE_CODEC_EVENT_SPACE) {
            out[n++] = ' ';
        }
    }
    out[n] = '\0';
}

TEST(message_player, weighted_playback_still_decodes_at_both_limits)
{
    static const char text[] = "PARIS 73 BK";
    char decoded[32];

    decode_played(25, text, decoded, sizeof(decoded));
    TEST_ASSERT_EQUAL_STRING("PARIS 73 BK ", decoded);

    message_player_init(&s_p, 20);
    decode_played(75, text, decoded, sizeof(decoded));
    TEST_ASSERT_EQUAL_STRING("PARIS 73 BK ", decoded);
}

TEST(message_player, enqueue_clamps_count_and_pause)
{
    enqueue_ok(repeating("E", 1, 200, 0), 0, MESSAGE_PLAYER_STARTED);
    message_player_status_t st;
    message_player_get_status(&s_p, 0, &st);
    TEST_ASSERT_EQUAL_UINT8(MESSAGE_PLAYER_COUNT_MAX, st.count);
    run(0, 3000);
    TEST_ASSERT_EQUAL_UINT32(U + 1000, s_edges[2]); /* pause clamped to 1 s */
}

TEST(message_player, status_reports_progress_for_the_sending_strip)
{
    message_player_status_t st;
    message_player_get_status(&s_p, 0, &st);
    TEST_ASSERT_FALSE(st.active);

    enqueue_ok(repeating("E <AR>", 4, 3, 2), 0, MESSAGE_PLAYER_STARTED);
    message_player_get_status(&s_p, 0, &st);
    TEST_ASSERT_TRUE(st.active);
    TEST_ASSERT_FALSE(st.paused);
    TEST_ASSERT_EQUAL_INT(4, st.tag);
    TEST_ASSERT_EQUAL_STRING("E <AR>", st.text);
    TEST_ASSERT_EQUAL_UINT(0, st.char_pos);
    TEST_ASSERT_EQUAL_UINT(1, st.char_len);
    TEST_ASSERT_EQUAL_UINT32(1, st.send_number);
    TEST_ASSERT_TRUE(st.repeat);
    TEST_ASSERT_EQUAL_UINT8(3, st.count);
    TEST_ASSERT_EQUAL_UINT8(2, st.pause_s);

    /* During the word gap the highlight stays on E; then moves to <AR>. */
    run(0, U + 10);
    message_player_get_status(&s_p, U + 10, &st);
    TEST_ASSERT_EQUAL_UINT(0, st.char_pos);
    run(U + 10, 8 * U + 10);
    message_player_get_status(&s_p, 8 * U + 10, &st);
    TEST_ASSERT_EQUAL_UINT(2, st.char_pos);
    TEST_ASSERT_EQUAL_UINT(4, st.char_len);

    /* <AR> ends at 8U + 13U = 21U; then a 2 s pause before send 2. */
    run(8 * U + 10, 21 * U + 500);
    message_player_get_status(&s_p, 21 * U + 500, &st);
    TEST_ASSERT_TRUE(st.paused);
    TEST_ASSERT_EQUAL_UINT(0, st.char_len);
    TEST_ASSERT_EQUAL_UINT32(2, st.send_number);
    TEST_ASSERT_EQUAL_UINT32(1500, st.pause_remaining_ms);
}

TEST(message_player, late_service_calls_do_not_shift_later_elements)
{
    enqueue_ok(msg("EE", 1), 0, MESSAGE_PLAYER_STARTED);
    TEST_ASSERT_TRUE(message_player_service(&s_p, 0));
    /* Called late, 5 ms after the dit should have ended. */
    TEST_ASSERT_FALSE(message_player_service(&s_p, U + 5));
    /* The next dit still starts 3 units after the first one's true end. */
    TEST_ASSERT_FALSE(message_player_service(&s_p, 4 * U - 1));
    TEST_ASSERT_TRUE(message_player_service(&s_p, 4 * U));
}

TEST_GROUP_RUNNER(message_player)
{
    RUN_TEST_CASE(message_player, sends_elements_and_gaps_with_paris_timing);
    RUN_TEST_CASE(message_player, prosign_letters_run_together);
    RUN_TEST_CASE(message_player, wpm_change_applies_from_the_next_element);
    RUN_TEST_CASE(message_player, repeat_count_includes_the_first_send_and_pauses_between);
    RUN_TEST_CASE(message_player, count_zero_repeats_until_stopped);
    RUN_TEST_CASE(message_player, stop_cuts_an_element_short_and_clears_the_queue);
    RUN_TEST_CASE(message_player, queued_message_follows_after_a_word_gap);
    RUN_TEST_CASE(message_player, queueing_ends_a_repeating_message_after_its_current_send);
    RUN_TEST_CASE(message_player, queueing_during_a_pause_starts_the_next_message_promptly);
    RUN_TEST_CASE(message_player, queueing_early_in_a_pause_still_waits_a_word_gap);
    RUN_TEST_CASE(message_player, enqueue_ignores_duplicates_full_queue_and_empty_text);
    RUN_TEST_CASE(message_player, repeat_on_during_a_single_send_makes_it_repeat);
    RUN_TEST_CASE(message_player, repeat_off_during_a_send_lets_it_finish);
    RUN_TEST_CASE(message_player, repeat_off_during_a_pause_stops_at_once);
    RUN_TEST_CASE(message_player, repeat_params_during_a_pause_apply_from_the_pause_start);
    RUN_TEST_CASE(message_player, shortening_a_pause_that_already_ran_out_sends_next_at_once);
    RUN_TEST_CASE(message_player, enqueue_reads_an_unterminated_text_only_up_to_its_limit);
    RUN_TEST_CASE(message_player, start_waits_a_word_gap_after_the_operators_key);
    RUN_TEST_CASE(message_player, start_counts_the_word_gap_from_the_operators_last_key_down);
    RUN_TEST_CASE(message_player, start_is_immediate_once_the_operators_key_has_been_idle_a_word_gap);
    RUN_TEST_CASE(message_player, a_key_down_a_word_gap_old_is_forgotten);
    RUN_TEST_CASE(message_player, weight_lengthens_elements_and_shortens_the_gaps_after_them);
    RUN_TEST_CASE(message_player, weight_below_standard_shortens_elements);
    RUN_TEST_CASE(message_player, weight_change_mid_element_keeps_that_elements_total);
    RUN_TEST_CASE(message_player, weight_is_clamped);
    RUN_TEST_CASE(message_player, weighted_playback_still_decodes_at_both_limits);
    RUN_TEST_CASE(message_player, enqueue_clamps_count_and_pause);
    RUN_TEST_CASE(message_player, status_reports_progress_for_the_sending_strip);
    RUN_TEST_CASE(message_player, late_service_calls_do_not_shift_later_elements);
}
