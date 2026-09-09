#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MORSE_CODEC_MAX_ELEMENTS 8

typedef enum {
    MORSE_CODEC_EVENT_NONE = 0,
    MORSE_CODEC_EVENT_CHAR,    /* a character was decoded; *out_char is valid */
    MORSE_CODEC_EVENT_SPACE,   /* a word gap was decoded */
    MORSE_CODEC_EVENT_UNKNOWN, /* an element sequence didn't match any known character; *out_char is MORSE_CODEC_UNKNOWN_CHAR */
} morse_codec_event_t;

/**
 * Placeholder emitted as *out_char when MORSE_CODEC_EVENT_UNKNOWN fires.
 * Chosen to not collide with any decodable letter, digit, or punctuation
 * mark, so it can't be confused with a real decoded character.
 */
#define MORSE_CODEC_UNKNOWN_CHAR '*'

/**
 * Prosigns (procedural signals) are sent as one unbroken Morse sequence with
 * no gap between "letters." Ones whose sequence happens to match an existing
 * punctuation mark decode directly to that punctuation character, by
 * convention: BT -> '=', AR -> '+', KN -> '(', AS -> '&'.
 *
 * The remaining prosigns below have no punctuation equivalent, so *out_char
 * carries one of these sentinel values instead of a printable glyph. Each
 * has the top bit set, so a caller can distinguish a prosign from a plain
 * ASCII character with `(unsigned char)ch >= 0x80`; look up its display
 * abbreviation with morse_codec_prosign_name().
 */
#define MORSE_CODEC_PROSIGN_SK ((char)0x80) /* end of contact ("silent key") */
#define MORSE_CODEC_PROSIGN_HH ((char)0x81) /* error, keyed over */
#define MORSE_CODEC_PROSIGN_VE ((char)0x82) /* understood */
#define MORSE_CODEC_PROSIGN_CT ((char)0x83) /* commencing / start copying */

/**
 * Morse timing classifier and tree decoder.
 *
 * Pure logic: callers supply monotonically increasing millisecond
 * timestamps rather than the codec reading a clock itself, so it can be
 * driven either by real hardware (esp_timer) or by a synthetic timestamp
 * sequence in tests.
 */
typedef struct {
    uint16_t wpm;
    uint32_t unit_ms;
    bool key_down;
    bool started;       /* true once the first key transition has been seen */
    bool space_emitted; /* true once a word-gap SPACE has been emitted for the current idle period */
    uint32_t last_edge_ms;
    char elements[MORSE_CODEC_MAX_ELEMENTS];
    uint8_t element_count;
} morse_codec_t;

/** Initialize the codec at the given words-per-minute keying speed. */
void morse_codec_init(morse_codec_t *codec, uint16_t wpm);

/** Clear in-progress decode state (e.g. for a UI "Clear" action). Keeps wpm. */
void morse_codec_reset(morse_codec_t *codec);

/** Change keying speed; affects classification of subsequent events. */
void morse_codec_set_wpm(morse_codec_t *codec, uint16_t wpm);

/**
 * Feed one key transition (key_down flips relative to the codec's current
 * state; callers should only call this on real edges). timestamp_ms must be
 * monotonically increasing across calls.
 *
 * On key-down, classifies the just-finished key-up gap (may flush a pending
 * character if the gap was character-gap length or longer). On key-up,
 * classifies the just-finished key-down duration as a dit or dah and
 * appends it to the in-progress element sequence.
 */
morse_codec_event_t morse_codec_key_event(morse_codec_t *codec, bool key_down, uint32_t timestamp_ms, char *out_char);

/**
 * Call periodically (e.g. every 20-50ms) while idle so a trailing
 * character-gap or word-gap silence is classified even without a further
 * key-down edge. No-op while the key is currently down.
 */
morse_codec_event_t morse_codec_tick(morse_codec_t *codec, uint32_t timestamp_ms, char *out_char);

/**
 * Look up the display abbreviation for a prosign sentinel value (one of the
 * MORSE_CODEC_PROSIGN_* macros) received via *out_char.
 *
 * @return a short static string (e.g. "SK"), or NULL if ch is not one of the
 *         MORSE_CODEC_PROSIGN_* values.
 */
const char *morse_codec_prosign_name(char ch);

#ifdef __cplusplus
}
#endif
