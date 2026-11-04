#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "morse_text.h"

TEST_GROUP(morse_text);

static char s_out[81];

TEST_SETUP(morse_text)
{
    memset(s_out, 0x55, sizeof(s_out));
}

TEST_TEAR_DOWN(morse_text)
{
}

static morse_text_result_t normalize(const char *in)
{
    return morse_text_normalize(in, true, s_out, sizeof(s_out), NULL);
}

TEST(morse_text, normalize_uppercases_collapses_and_trims_spaces)
{
    TEST_ASSERT_EQUAL(MORSE_TEXT_OK, normalize("  cq  cq de  n0call  "));
    TEST_ASSERT_EQUAL_STRING("CQ CQ DE N0CALL", s_out);
}

TEST(morse_text, normalize_accepts_all_decoder_punctuation)
{
    TEST_ASSERT_EQUAL(MORSE_TEXT_OK, normalize(".,?'!/()&:;=+-_\"$@"));
    TEST_ASSERT_EQUAL_STRING(".,?'!/()&:;=+-_\"$@", s_out);
}

TEST(morse_text, normalize_accepts_the_eight_prosigns_case_insensitively)
{
    TEST_ASSERT_EQUAL(MORSE_TEXT_OK, normalize("<ar><SK><kN><BT> <hh><VE><CT><As>"));
    TEST_ASSERT_EQUAL_STRING("<AR><SK><KN><BT> <HH><VE><CT><AS>", s_out);
}

TEST(morse_text, normalize_empty_and_all_space_input_give_empty_text)
{
    TEST_ASSERT_EQUAL(MORSE_TEXT_OK, normalize(""));
    TEST_ASSERT_EQUAL_STRING("", s_out);
    TEST_ASSERT_EQUAL(MORSE_TEXT_OK, normalize("    "));
    TEST_ASSERT_EQUAL_STRING("", s_out);
}

TEST(morse_text, normalize_rejects_bad_characters_and_reports_their_position)
{
    size_t pos = 99;
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_INVALID_CHAR, morse_text_normalize("CQ#", true, s_out, sizeof(s_out), &pos));
    TEST_ASSERT_EQUAL_UINT(2, pos);
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_INVALID_CHAR, normalize("A\tB"));
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_INVALID_CHAR, normalize("A\\B"));
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_INVALID_CHAR, normalize("A>B"));
    /* UTF-8 bytes must not alias the decoder's prosign sentinels (0x80..). */
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_INVALID_CHAR, normalize("\xc3\xa9"));
}

TEST(morse_text, normalize_rejects_malformed_and_unknown_prosigns)
{
    size_t pos = 99;
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_UNCLOSED_PROSIGN,
                      morse_text_normalize("TU <SK", true, s_out, sizeof(s_out), &pos));
    TEST_ASSERT_EQUAL_UINT(3, pos);
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_EMPTY_PROSIGN, normalize("A<>B"));
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_UNKNOWN_PROSIGN, normalize("<SOS>"));
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_UNKNOWN_PROSIGN, normalize("<QQ>"));
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_UNKNOWN_PROSIGN, normalize("<A R>"));
}

TEST(morse_text, names_reject_prosign_brackets)
{
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_INVALID_CHAR, morse_text_normalize("<AR>", false, s_out, 9, NULL));
    TEST_ASSERT_EQUAL(MORSE_TEXT_OK, morse_text_normalize("cq dx", false, s_out, 9, NULL));
    TEST_ASSERT_EQUAL_STRING("CQ DX", s_out);
}

TEST(morse_text, normalize_length_limit_applies_to_normalized_text)
{
    /* 8-char limit (buffer of 9): extra spaces don't count, a prosign is 4. */
    TEST_ASSERT_EQUAL(MORSE_TEXT_OK, morse_text_normalize("  ABC    DEFG  ", true, s_out, 9, NULL));
    TEST_ASSERT_EQUAL_STRING("ABC DEFG", s_out);
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_TOO_LONG, morse_text_normalize("ABCDEFGHI", true, s_out, 9, NULL));
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_TOO_LONG, morse_text_normalize("ABCDE<AR>", true, s_out, 9, NULL));
    TEST_ASSERT_EQUAL(MORSE_TEXT_OK, morse_text_normalize("ABCD<AR>", true, s_out, 9, NULL));
    TEST_ASSERT_EQUAL(MORSE_TEXT_ERR_TOO_LONG, morse_text_normalize("", true, s_out, 0, NULL));
}

TEST(morse_text, next_token_walks_characters_prosigns_and_word_gaps)
{
    const char *text = "CQ DE <AR>";
    size_t pos = 0;
    morse_text_token_t tok;

    TEST_ASSERT_TRUE(morse_text_next_token(text, &pos, &tok));
    TEST_ASSERT_EQUAL_STRING("-.-.", tok.pattern);
    TEST_ASSERT_EQUAL_UINT(0, tok.text_pos);
    TEST_ASSERT_EQUAL_UINT(1, tok.text_len);
    TEST_ASSERT_FALSE(tok.word_gap_before);

    TEST_ASSERT_TRUE(morse_text_next_token(text, &pos, &tok));
    TEST_ASSERT_EQUAL_STRING("--.-", tok.pattern);
    TEST_ASSERT_FALSE(tok.word_gap_before);

    TEST_ASSERT_TRUE(morse_text_next_token(text, &pos, &tok));
    TEST_ASSERT_EQUAL_STRING("-..", tok.pattern);
    TEST_ASSERT_EQUAL_UINT(3, tok.text_pos);
    TEST_ASSERT_TRUE(tok.word_gap_before);

    TEST_ASSERT_TRUE(morse_text_next_token(text, &pos, &tok));
    TEST_ASSERT_EQUAL_STRING(".", tok.pattern);

    TEST_ASSERT_TRUE(morse_text_next_token(text, &pos, &tok));
    TEST_ASSERT_EQUAL_STRING(".-.-.", tok.pattern);
    TEST_ASSERT_EQUAL_UINT(6, tok.text_pos);
    TEST_ASSERT_EQUAL_UINT(4, tok.text_len);
    TEST_ASSERT_TRUE(tok.word_gap_before);

    TEST_ASSERT_FALSE(morse_text_next_token(text, &pos, &tok));
    pos = 0;
    TEST_ASSERT_FALSE(morse_text_next_token("", &pos, &tok));
}

TEST(morse_text, prosign_patterns_match_the_decoder)
{
    static const struct {
        const char *text;
        const char *pattern;
    } cases[] = {
        { "<AR>", ".-.-." }, { "<SK>", "...-.-" }, { "<KN>", "-.--." },   { "<BT>", "-...-" },
        { "<HH>", "........" }, { "<VE>", "...-." }, { "<CT>", "-.-.-" }, { "<AS>", ".-..." },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        size_t pos = 0;
        morse_text_token_t tok;
        TEST_ASSERT_TRUE(morse_text_next_token(cases[i].text, &pos, &tok));
        TEST_ASSERT_EQUAL_STRING(cases[i].pattern, tok.pattern);
    }
}

TEST(morse_text, next_token_stops_on_text_normalize_would_reject)
{
    size_t pos = 0;
    morse_text_token_t tok;
    TEST_ASSERT_FALSE(morse_text_next_token("#", &pos, &tok));
    pos = 0;
    TEST_ASSERT_FALSE(morse_text_next_token("<SOS>", &pos, &tok));
    pos = 0;
    TEST_ASSERT_TRUE(morse_text_next_token("  E", &pos, &tok));
    TEST_ASSERT_FALSE(tok.word_gap_before); /* leading spaces aren't a gap */
}

TEST(morse_text, result_str_describes_each_error)
{
    TEST_ASSERT_EQUAL_STRING("unknown prosign", morse_text_result_str(MORSE_TEXT_ERR_UNKNOWN_PROSIGN));
    TEST_ASSERT_EQUAL_STRING("too long", morse_text_result_str(MORSE_TEXT_ERR_TOO_LONG));
}

TEST_GROUP_RUNNER(morse_text)
{
    RUN_TEST_CASE(morse_text, normalize_uppercases_collapses_and_trims_spaces);
    RUN_TEST_CASE(morse_text, normalize_accepts_all_decoder_punctuation);
    RUN_TEST_CASE(morse_text, normalize_accepts_the_eight_prosigns_case_insensitively);
    RUN_TEST_CASE(morse_text, normalize_empty_and_all_space_input_give_empty_text);
    RUN_TEST_CASE(morse_text, normalize_rejects_bad_characters_and_reports_their_position);
    RUN_TEST_CASE(morse_text, normalize_rejects_malformed_and_unknown_prosigns);
    RUN_TEST_CASE(morse_text, names_reject_prosign_brackets);
    RUN_TEST_CASE(morse_text, normalize_length_limit_applies_to_normalized_text);
    RUN_TEST_CASE(morse_text, next_token_walks_characters_prosigns_and_word_gaps);
    RUN_TEST_CASE(morse_text, prosign_patterns_match_the_decoder);
    RUN_TEST_CASE(morse_text, next_token_stops_on_text_normalize_would_reject);
    RUN_TEST_CASE(morse_text, result_str_describes_each_error);
}
