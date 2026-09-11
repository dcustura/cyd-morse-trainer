#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "iambic_keyer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_WPM_MIN 5
#define SETTINGS_WPM_MAX 40
#define SETTINGS_TONE_HZ_MIN 300
#define SETTINGS_TONE_HZ_MAX 1200
#define SETTINGS_VOLUME_PCT_MIN 0
#define SETTINGS_VOLUME_PCT_MAX 100
#define SETTINGS_ENVELOPE_MS_MIN 2
#define SETTINGS_ENVELOPE_MS_MAX 100
#define SETTINGS_BRIGHTNESS_PCT_MIN 10
#define SETTINGS_BRIGHTNESS_PCT_MAX 100

#define SETTINGS_DEFAULT_WPM 15
#define SETTINGS_DEFAULT_KEYMODE IAMBIC_KEYER_MODE_B
#define SETTINGS_DEFAULT_PADDLE_SWAP false
#define SETTINGS_DEFAULT_TONE_HZ 600
#define SETTINGS_DEFAULT_VOLUME_PCT 50
#define SETTINGS_DEFAULT_ENVELOPE_MS 10
#define SETTINGS_DEFAULT_BRIGHTNESS_PCT 100

typedef struct {
    uint16_t wpm;
    iambic_keyer_mode_t keymode;
    bool paddle_swap;
    uint16_t tone_hz;
    uint8_t volume_pct;
    uint16_t envelope_ms;
    uint8_t brightness_pct;
} settings_t;

/** Clamp a WPM value to [SETTINGS_WPM_MIN, SETTINGS_WPM_MAX]. */
uint16_t settings_clamp_wpm(uint16_t wpm);

/** Clamp a sidetone frequency to [SETTINGS_TONE_HZ_MIN, SETTINGS_TONE_HZ_MAX]. */
uint16_t settings_clamp_tone_hz(uint16_t hz);

/** Clamp a sidetone volume percentage to [SETTINGS_VOLUME_PCT_MIN, SETTINGS_VOLUME_PCT_MAX]. */
uint8_t settings_clamp_volume_pct(uint8_t pct);

/** Clamp a keying envelope (attack/decay) duration to [SETTINGS_ENVELOPE_MS_MIN, SETTINGS_ENVELOPE_MS_MAX]. */
uint16_t settings_clamp_envelope_ms(uint16_t ms);

/** Clamp a screen brightness percentage to [SETTINGS_BRIGHTNESS_PCT_MIN, SETTINGS_BRIGHTNESS_PCT_MAX]. */
uint8_t settings_clamp_brightness_pct(uint8_t pct);

/** Validate a raw stored key-mode byte, falling back to the default key mode on an out-of-range value. */
iambic_keyer_mode_t settings_validate_keymode(uint8_t raw);

/** Populate *out with the compiled-in defaults. */
void settings_set_defaults(settings_t *out);

#ifdef __cplusplus
}
#endif
