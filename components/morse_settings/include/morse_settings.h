#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "iambic_keyer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MORSE_SETTINGS_WPM_MIN 5
#define MORSE_SETTINGS_WPM_MAX 40
#define MORSE_SETTINGS_TONE_HZ_MIN 300
#define MORSE_SETTINGS_TONE_HZ_MAX 1200
#define MORSE_SETTINGS_VOLUME_PCT_MIN 0
#define MORSE_SETTINGS_VOLUME_PCT_MAX 100
#define MORSE_SETTINGS_ENVELOPE_MS_MIN 2
#define MORSE_SETTINGS_ENVELOPE_MS_MAX 100

#define MORSE_SETTINGS_DEFAULT_WPM 15
#define MORSE_SETTINGS_DEFAULT_KEYMODE IAMBIC_KEYER_MODE_B
#define MORSE_SETTINGS_DEFAULT_PADDLE_SWAP false
#define MORSE_SETTINGS_DEFAULT_TONE_HZ 600
#define MORSE_SETTINGS_DEFAULT_VOLUME_PCT 50
#define MORSE_SETTINGS_DEFAULT_ENVELOPE_MS 10

typedef struct {
    uint16_t wpm;
    iambic_keyer_mode_t keymode;
    bool paddle_swap;
    uint16_t tone_hz;
    uint8_t volume_pct;
    uint16_t envelope_ms;
} morse_settings_t;

/** Clamp a WPM value to [MORSE_SETTINGS_WPM_MIN, MORSE_SETTINGS_WPM_MAX]. */
uint16_t morse_settings_clamp_wpm(uint16_t wpm);

/** Clamp a sidetone frequency to [MORSE_SETTINGS_TONE_HZ_MIN, MORSE_SETTINGS_TONE_HZ_MAX]. */
uint16_t morse_settings_clamp_tone_hz(uint16_t hz);

/** Clamp a sidetone volume percentage to [MORSE_SETTINGS_VOLUME_PCT_MIN, MORSE_SETTINGS_VOLUME_PCT_MAX]. */
uint8_t morse_settings_clamp_volume_pct(uint8_t pct);

/** Clamp a keying envelope (attack/decay) duration to [MORSE_SETTINGS_ENVELOPE_MS_MIN, MORSE_SETTINGS_ENVELOPE_MS_MAX]. */
uint16_t morse_settings_clamp_envelope_ms(uint16_t ms);

/** Validate a raw stored key-mode byte, falling back to the default key mode on an out-of-range value. */
iambic_keyer_mode_t morse_settings_validate_keymode(uint8_t raw);

/** Populate *out with the compiled-in defaults. */
void morse_settings_set_defaults(morse_settings_t *out);

#ifdef __cplusplus
}
#endif
