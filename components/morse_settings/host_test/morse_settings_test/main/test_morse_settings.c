#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "morse_settings.h"

TEST_GROUP(morse_settings);

TEST_SETUP(morse_settings)
{
}

TEST_TEAR_DOWN(morse_settings)
{
}

TEST(morse_settings, clamp_wpm_passes_in_range_values_through)
{
    TEST_ASSERT_EQUAL_UINT16(5, morse_settings_clamp_wpm(5));
    TEST_ASSERT_EQUAL_UINT16(20, morse_settings_clamp_wpm(20));
    TEST_ASSERT_EQUAL_UINT16(40, morse_settings_clamp_wpm(40));
}

TEST(morse_settings, clamp_wpm_clamps_out_of_range_values)
{
    TEST_ASSERT_EQUAL_UINT16(5, morse_settings_clamp_wpm(0));
    TEST_ASSERT_EQUAL_UINT16(5, morse_settings_clamp_wpm(4));
    TEST_ASSERT_EQUAL_UINT16(40, morse_settings_clamp_wpm(41));
    TEST_ASSERT_EQUAL_UINT16(40, morse_settings_clamp_wpm(65535));
}

TEST(morse_settings, clamp_tone_hz_passes_in_range_values_through)
{
    TEST_ASSERT_EQUAL_UINT16(300, morse_settings_clamp_tone_hz(300));
    TEST_ASSERT_EQUAL_UINT16(700, morse_settings_clamp_tone_hz(700));
    TEST_ASSERT_EQUAL_UINT16(1200, morse_settings_clamp_tone_hz(1200));
}

TEST(morse_settings, clamp_tone_hz_clamps_out_of_range_values)
{
    TEST_ASSERT_EQUAL_UINT16(300, morse_settings_clamp_tone_hz(0));
    TEST_ASSERT_EQUAL_UINT16(300, morse_settings_clamp_tone_hz(299));
    TEST_ASSERT_EQUAL_UINT16(1200, morse_settings_clamp_tone_hz(1201));
}

TEST(morse_settings, validate_keymode_passes_valid_values_through)
{
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_MODE_A, morse_settings_validate_keymode(IAMBIC_KEYER_MODE_A));
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_MODE_B, morse_settings_validate_keymode(IAMBIC_KEYER_MODE_B));
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_MODE_STRAIGHT, morse_settings_validate_keymode(IAMBIC_KEYER_MODE_STRAIGHT));
}

TEST(morse_settings, validate_keymode_falls_back_to_default_for_invalid_bytes)
{
    TEST_ASSERT_EQUAL(MORSE_SETTINGS_DEFAULT_KEYMODE, morse_settings_validate_keymode(3));
    TEST_ASSERT_EQUAL(MORSE_SETTINGS_DEFAULT_KEYMODE, morse_settings_validate_keymode(255));
}

TEST(morse_settings, set_defaults_populates_the_compiled_in_defaults)
{
    morse_settings_t settings;
    memset(&settings, 0xFF, sizeof(settings)); /* poison to catch any field left unset */

    morse_settings_set_defaults(&settings);

    TEST_ASSERT_EQUAL_UINT16(MORSE_SETTINGS_DEFAULT_WPM, settings.wpm);
    TEST_ASSERT_EQUAL(MORSE_SETTINGS_DEFAULT_KEYMODE, settings.keymode);
    TEST_ASSERT_EQUAL(MORSE_SETTINGS_DEFAULT_PADDLE_SWAP, settings.paddle_swap);
    TEST_ASSERT_EQUAL_UINT16(MORSE_SETTINGS_DEFAULT_TONE_HZ, settings.tone_hz);
}

TEST_GROUP_RUNNER(morse_settings)
{
    RUN_TEST_CASE(morse_settings, clamp_wpm_passes_in_range_values_through);
    RUN_TEST_CASE(morse_settings, clamp_wpm_clamps_out_of_range_values);
    RUN_TEST_CASE(morse_settings, clamp_tone_hz_passes_in_range_values_through);
    RUN_TEST_CASE(morse_settings, clamp_tone_hz_clamps_out_of_range_values);
    RUN_TEST_CASE(morse_settings, validate_keymode_passes_valid_values_through);
    RUN_TEST_CASE(morse_settings, validate_keymode_falls_back_to_default_for_invalid_bytes);
    RUN_TEST_CASE(morse_settings, set_defaults_populates_the_compiled_in_defaults);
}
