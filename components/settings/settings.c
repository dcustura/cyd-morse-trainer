#include "settings.h"

uint16_t settings_clamp_wpm(uint16_t wpm)
{
    if (wpm < SETTINGS_WPM_MIN) {
        return SETTINGS_WPM_MIN;
    }
    if (wpm > SETTINGS_WPM_MAX) {
        return SETTINGS_WPM_MAX;
    }
    return wpm;
}

uint16_t settings_clamp_tone_hz(uint16_t hz)
{
    if (hz < SETTINGS_TONE_HZ_MIN) {
        return SETTINGS_TONE_HZ_MIN;
    }
    if (hz > SETTINGS_TONE_HZ_MAX) {
        return SETTINGS_TONE_HZ_MAX;
    }
    return hz;
}

uint8_t settings_clamp_volume_pct(uint8_t pct)
{
    if (pct > SETTINGS_VOLUME_PCT_MAX) {
        return SETTINGS_VOLUME_PCT_MAX;
    }
    return pct;
}

uint16_t settings_clamp_envelope_ms(uint16_t ms)
{
    if (ms < SETTINGS_ENVELOPE_MS_MIN) {
        return SETTINGS_ENVELOPE_MS_MIN;
    }
    if (ms > SETTINGS_ENVELOPE_MS_MAX) {
        return SETTINGS_ENVELOPE_MS_MAX;
    }
    return ms;
}

uint8_t settings_clamp_brightness_pct(uint8_t pct)
{
    if (pct < SETTINGS_BRIGHTNESS_PCT_MIN) {
        return SETTINGS_BRIGHTNESS_PCT_MIN;
    }
    if (pct > SETTINGS_BRIGHTNESS_PCT_MAX) {
        return SETTINGS_BRIGHTNESS_PCT_MAX;
    }
    return pct;
}

iambic_keyer_mode_t settings_validate_keymode(uint8_t raw)
{
    switch (raw) {
    case IAMBIC_KEYER_MODE_A:
    case IAMBIC_KEYER_MODE_B:
    case IAMBIC_KEYER_MODE_STRAIGHT:
        return (iambic_keyer_mode_t)raw;
    default:
        return SETTINGS_DEFAULT_KEYMODE;
    }
}

void settings_set_defaults(settings_t *out)
{
    out->wpm = SETTINGS_DEFAULT_WPM;
    out->keymode = SETTINGS_DEFAULT_KEYMODE;
    out->paddle_swap = SETTINGS_DEFAULT_PADDLE_SWAP;
    out->tone_hz = SETTINGS_DEFAULT_TONE_HZ;
    out->volume_pct = SETTINGS_DEFAULT_VOLUME_PCT;
    out->envelope_ms = SETTINGS_DEFAULT_ENVELOPE_MS;
    out->brightness_pct = SETTINGS_DEFAULT_BRIGHTNESS_PCT;
    out->brightness_auto = SETTINGS_DEFAULT_BRIGHTNESS_AUTO;
}
