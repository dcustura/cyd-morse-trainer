#include "settings.h"

static int32_t clamp(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

uint16_t settings_clamp_wpm(uint16_t wpm)
{
    return (uint16_t)clamp(wpm, SETTINGS_WPM_MIN, SETTINGS_WPM_MAX);
}

uint16_t settings_clamp_tone_hz(uint16_t hz)
{
    return (uint16_t)clamp(hz, SETTINGS_TONE_HZ_MIN, SETTINGS_TONE_HZ_MAX);
}

uint8_t settings_clamp_volume_pct(uint8_t pct)
{
    return (uint8_t)clamp(pct, SETTINGS_VOLUME_PCT_MIN, SETTINGS_VOLUME_PCT_MAX);
}

uint16_t settings_clamp_envelope_ms(uint16_t ms)
{
    return (uint16_t)clamp(ms, SETTINGS_ENVELOPE_MS_MIN, SETTINGS_ENVELOPE_MS_MAX);
}

uint8_t settings_clamp_brightness_pct(uint8_t pct)
{
    return (uint8_t)clamp(pct, SETTINGS_BRIGHTNESS_PCT_MIN, SETTINGS_BRIGHTNESS_PCT_MAX);
}

uint8_t settings_clamp_auto_space_pct(uint8_t pct)
{
    return (uint8_t)clamp(pct, SETTINGS_AUTO_SPACE_PCT_MIN, SETTINGS_AUTO_SPACE_PCT_MAX);
}

iambic_keyer_mode_t settings_validate_keymode(uint8_t raw)
{
    switch (raw) {
    case IAMBIC_KEYER_MODE_A:
    case IAMBIC_KEYER_MODE_B:
    case IAMBIC_KEYER_MODE_STRAIGHT:
    case IAMBIC_KEYER_MODE_ULTIMATIC:
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
    out->paddle_debounce = SETTINGS_DEFAULT_PADDLE_DEBOUNCE;
    out->auto_space_pct = SETTINGS_DEFAULT_AUTO_SPACE_PCT;
    out->tone_hz = SETTINGS_DEFAULT_TONE_HZ;
    out->volume_pct = SETTINGS_DEFAULT_VOLUME_PCT;
    out->envelope_ms = SETTINGS_DEFAULT_ENVELOPE_MS;
    out->brightness_pct = SETTINGS_DEFAULT_BRIGHTNESS_PCT;
    out->brightness_auto = SETTINGS_DEFAULT_BRIGHTNESS_AUTO;
    out->practice_large_text = SETTINGS_DEFAULT_PRACTICE_LARGE_TEXT;
    out->ble_hid_enabled = SETTINGS_DEFAULT_BLE_HID_ENABLED;
    out->straight_ring_input = SETTINGS_DEFAULT_STRAIGHT_RING_INPUT;
}
