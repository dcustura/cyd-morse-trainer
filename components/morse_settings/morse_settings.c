#include "morse_settings.h"

uint16_t morse_settings_clamp_wpm(uint16_t wpm)
{
    if (wpm < MORSE_SETTINGS_WPM_MIN) {
        return MORSE_SETTINGS_WPM_MIN;
    }
    if (wpm > MORSE_SETTINGS_WPM_MAX) {
        return MORSE_SETTINGS_WPM_MAX;
    }
    return wpm;
}

uint16_t morse_settings_clamp_tone_hz(uint16_t hz)
{
    if (hz < MORSE_SETTINGS_TONE_HZ_MIN) {
        return MORSE_SETTINGS_TONE_HZ_MIN;
    }
    if (hz > MORSE_SETTINGS_TONE_HZ_MAX) {
        return MORSE_SETTINGS_TONE_HZ_MAX;
    }
    return hz;
}

uint8_t morse_settings_clamp_volume_pct(uint8_t pct)
{
    if (pct > MORSE_SETTINGS_VOLUME_PCT_MAX) {
        return MORSE_SETTINGS_VOLUME_PCT_MAX;
    }
    return pct;
}

uint16_t morse_settings_clamp_envelope_ms(uint16_t ms)
{
    if (ms < MORSE_SETTINGS_ENVELOPE_MS_MIN) {
        return MORSE_SETTINGS_ENVELOPE_MS_MIN;
    }
    if (ms > MORSE_SETTINGS_ENVELOPE_MS_MAX) {
        return MORSE_SETTINGS_ENVELOPE_MS_MAX;
    }
    return ms;
}

uint8_t morse_settings_clamp_brightness_pct(uint8_t pct)
{
    if (pct < MORSE_SETTINGS_BRIGHTNESS_PCT_MIN) {
        return MORSE_SETTINGS_BRIGHTNESS_PCT_MIN;
    }
    if (pct > MORSE_SETTINGS_BRIGHTNESS_PCT_MAX) {
        return MORSE_SETTINGS_BRIGHTNESS_PCT_MAX;
    }
    return pct;
}

iambic_keyer_mode_t morse_settings_validate_keymode(uint8_t raw)
{
    switch (raw) {
    case IAMBIC_KEYER_MODE_A:
    case IAMBIC_KEYER_MODE_B:
    case IAMBIC_KEYER_MODE_STRAIGHT:
        return (iambic_keyer_mode_t)raw;
    default:
        return MORSE_SETTINGS_DEFAULT_KEYMODE;
    }
}

void morse_settings_set_defaults(morse_settings_t *out)
{
    out->wpm = MORSE_SETTINGS_DEFAULT_WPM;
    out->keymode = MORSE_SETTINGS_DEFAULT_KEYMODE;
    out->paddle_swap = MORSE_SETTINGS_DEFAULT_PADDLE_SWAP;
    out->tone_hz = MORSE_SETTINGS_DEFAULT_TONE_HZ;
    out->volume_pct = MORSE_SETTINGS_DEFAULT_VOLUME_PCT;
    out->envelope_ms = MORSE_SETTINGS_DEFAULT_ENVELOPE_MS;
    out->brightness_pct = MORSE_SETTINGS_DEFAULT_BRIGHTNESS_PCT;
}
