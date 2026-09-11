#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "settings.h"

TEST_GROUP(settings);

TEST_SETUP(settings)
{
}

TEST_TEAR_DOWN(settings)
{
}

TEST(settings, clamp_wpm_passes_in_range_values_through)
{
    TEST_ASSERT_EQUAL_UINT16(5, settings_clamp_wpm(5));
    TEST_ASSERT_EQUAL_UINT16(20, settings_clamp_wpm(20));
    TEST_ASSERT_EQUAL_UINT16(40, settings_clamp_wpm(40));
}

TEST(settings, clamp_wpm_clamps_out_of_range_values)
{
    TEST_ASSERT_EQUAL_UINT16(5, settings_clamp_wpm(0));
    TEST_ASSERT_EQUAL_UINT16(5, settings_clamp_wpm(4));
    TEST_ASSERT_EQUAL_UINT16(40, settings_clamp_wpm(41));
    TEST_ASSERT_EQUAL_UINT16(40, settings_clamp_wpm(65535));
}

TEST(settings, clamp_tone_hz_passes_in_range_values_through)
{
    TEST_ASSERT_EQUAL_UINT16(300, settings_clamp_tone_hz(300));
    TEST_ASSERT_EQUAL_UINT16(700, settings_clamp_tone_hz(700));
    TEST_ASSERT_EQUAL_UINT16(1200, settings_clamp_tone_hz(1200));
}

TEST(settings, clamp_tone_hz_clamps_out_of_range_values)
{
    TEST_ASSERT_EQUAL_UINT16(300, settings_clamp_tone_hz(0));
    TEST_ASSERT_EQUAL_UINT16(300, settings_clamp_tone_hz(299));
    TEST_ASSERT_EQUAL_UINT16(1200, settings_clamp_tone_hz(1201));
}

TEST(settings, clamp_volume_pct_passes_in_range_values_through)
{
    TEST_ASSERT_EQUAL_UINT8(0, settings_clamp_volume_pct(0));
    TEST_ASSERT_EQUAL_UINT8(50, settings_clamp_volume_pct(50));
    TEST_ASSERT_EQUAL_UINT8(100, settings_clamp_volume_pct(100));
}

TEST(settings, clamp_volume_pct_clamps_out_of_range_values)
{
    TEST_ASSERT_EQUAL_UINT8(100, settings_clamp_volume_pct(101));
    TEST_ASSERT_EQUAL_UINT8(100, settings_clamp_volume_pct(255));
}

TEST(settings, clamp_envelope_ms_passes_in_range_values_through)
{
    TEST_ASSERT_EQUAL_UINT16(2, settings_clamp_envelope_ms(2));
    TEST_ASSERT_EQUAL_UINT16(10, settings_clamp_envelope_ms(10));
    TEST_ASSERT_EQUAL_UINT16(100, settings_clamp_envelope_ms(100));
}

TEST(settings, clamp_envelope_ms_clamps_out_of_range_values)
{
    TEST_ASSERT_EQUAL_UINT16(2, settings_clamp_envelope_ms(0));
    TEST_ASSERT_EQUAL_UINT16(2, settings_clamp_envelope_ms(1));
    TEST_ASSERT_EQUAL_UINT16(100, settings_clamp_envelope_ms(101));
    TEST_ASSERT_EQUAL_UINT16(100, settings_clamp_envelope_ms(65535));
}

TEST(settings, validate_keymode_passes_valid_values_through)
{
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_MODE_A, settings_validate_keymode(IAMBIC_KEYER_MODE_A));
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_MODE_B, settings_validate_keymode(IAMBIC_KEYER_MODE_B));
    TEST_ASSERT_EQUAL(IAMBIC_KEYER_MODE_STRAIGHT, settings_validate_keymode(IAMBIC_KEYER_MODE_STRAIGHT));
}

TEST(settings, validate_keymode_falls_back_to_default_for_invalid_bytes)
{
    TEST_ASSERT_EQUAL(SETTINGS_DEFAULT_KEYMODE, settings_validate_keymode(3));
    TEST_ASSERT_EQUAL(SETTINGS_DEFAULT_KEYMODE, settings_validate_keymode(255));
}

TEST(settings, set_defaults_populates_the_compiled_in_defaults)
{
    settings_t settings;
    memset(&settings, 0xFF, sizeof(settings)); /* poison to catch any field left unset */

    settings_set_defaults(&settings);

    TEST_ASSERT_EQUAL_UINT16(SETTINGS_DEFAULT_WPM, settings.wpm);
    TEST_ASSERT_EQUAL(SETTINGS_DEFAULT_KEYMODE, settings.keymode);
    TEST_ASSERT_EQUAL(SETTINGS_DEFAULT_PADDLE_SWAP, settings.paddle_swap);
    TEST_ASSERT_EQUAL_UINT16(SETTINGS_DEFAULT_TONE_HZ, settings.tone_hz);
    TEST_ASSERT_EQUAL_UINT8(SETTINGS_DEFAULT_VOLUME_PCT, settings.volume_pct);
    TEST_ASSERT_EQUAL_UINT16(SETTINGS_DEFAULT_ENVELOPE_MS, settings.envelope_ms);
}

TEST_GROUP_RUNNER(settings)
{
    RUN_TEST_CASE(settings, clamp_wpm_passes_in_range_values_through);
    RUN_TEST_CASE(settings, clamp_wpm_clamps_out_of_range_values);
    RUN_TEST_CASE(settings, clamp_tone_hz_passes_in_range_values_through);
    RUN_TEST_CASE(settings, clamp_tone_hz_clamps_out_of_range_values);
    RUN_TEST_CASE(settings, clamp_volume_pct_passes_in_range_values_through);
    RUN_TEST_CASE(settings, clamp_volume_pct_clamps_out_of_range_values);
    RUN_TEST_CASE(settings, clamp_envelope_ms_passes_in_range_values_through);
    RUN_TEST_CASE(settings, clamp_envelope_ms_clamps_out_of_range_values);
    RUN_TEST_CASE(settings, validate_keymode_passes_valid_values_through);
    RUN_TEST_CASE(settings, validate_keymode_falls_back_to_default_for_invalid_bytes);
    RUN_TEST_CASE(settings, set_defaults_populates_the_compiled_in_defaults);
}
