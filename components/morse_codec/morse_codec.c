#include "morse_codec.h"

#include <string.h>

typedef struct {
    const char *seq;
    char ch;
} morse_table_entry_t;

static const morse_table_entry_t MORSE_TABLE[] = {
    { ".-", 'A' },    { "-...", 'B' }, { "-.-.", 'C' }, { "-..", 'D' },
    { ".", 'E' },     { "..-.", 'F' }, { "--.", 'G' },  { "....", 'H' },
    { "..", 'I' },    { ".---", 'J' }, { "-.-", 'K' },  { ".-..", 'L' },
    { "--", 'M' },    { "-.", 'N' },   { "---", 'O' },  { ".--.", 'P' },
    { "--.-", 'Q' },  { ".-.", 'R' },  { "...", 'S' },  { "-", 'T' },
    { "..-", 'U' },   { "...-", 'V' }, { ".--", 'W' },  { "-..-", 'X' },
    { "-.--", 'Y' },  { "--..", 'Z' },
    { "-----", '0' }, { ".----", '1' }, { "..---", '2' }, { "...--", '3' },
    { "....-", '4' }, { ".....", '5' }, { "-....", '6' }, { "--...", '7' },
    { "---..", '8' }, { "----.", '9' },
};

static void reset_decode_state(morse_codec_t *codec)
{
    codec->key_down = false;
    codec->started = false;
    codec->space_emitted = true;
    codec->last_edge_ms = 0;
    codec->element_count = 0;
}

void morse_codec_init(morse_codec_t *codec, uint16_t wpm)
{
    codec->wpm = 0;
    reset_decode_state(codec);
    morse_codec_set_wpm(codec, wpm);
}

void morse_codec_reset(morse_codec_t *codec)
{
    reset_decode_state(codec);
}

void morse_codec_set_wpm(morse_codec_t *codec, uint16_t wpm)
{
    if (wpm == 0) {
        wpm = 1;
    }
    codec->wpm = wpm;
    codec->unit_ms = 1200u / wpm;
    if (codec->unit_ms == 0) {
        codec->unit_ms = 1;
    }
}

static char morse_lookup(const char *elements, uint8_t count)
{
    for (size_t i = 0; i < sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0]); ++i) {
        if (strncmp(MORSE_TABLE[i].seq, elements, count) == 0 && MORSE_TABLE[i].seq[count] == '\0') {
            return MORSE_TABLE[i].ch;
        }
    }
    return 0;
}

static morse_codec_event_t flush_char(morse_codec_t *codec, char *out_char)
{
    char decoded = morse_lookup(codec->elements, codec->element_count);
    codec->element_count = 0;
    codec->space_emitted = false; /* a character just happened; allow a SPACE once the next idle period is long enough */

    if (decoded != 0) {
        if (out_char) {
            *out_char = decoded;
        }
        return MORSE_CODEC_EVENT_CHAR;
    }

    if (out_char) {
        *out_char = '?';
    }
    return MORSE_CODEC_EVENT_UNKNOWN;
}

morse_codec_event_t morse_codec_key_event(morse_codec_t *codec, bool key_down, uint32_t timestamp_ms, char *out_char)
{
    morse_codec_event_t event = MORSE_CODEC_EVENT_NONE;

    if (!codec->started) {
        codec->started = true;
        codec->key_down = key_down;
        codec->last_edge_ms = timestamp_ms;
        return MORSE_CODEC_EVENT_NONE;
    }

    if (key_down == codec->key_down) {
        return MORSE_CODEC_EVENT_NONE;
    }

    uint32_t duration_ms = timestamp_ms - codec->last_edge_ms;

    if (key_down) {
        /* Rising edge: classify the gap that just ended, if a character is pending. */
        if (codec->element_count > 0 && duration_ms >= 2u * codec->unit_ms) {
            event = flush_char(codec, out_char);
        }
        codec->space_emitted = false;
    } else {
        /* Falling edge: classify the element that just ended as a dit or dah. */
        char element = (duration_ms <= 2u * codec->unit_ms) ? '.' : '-';
        if (codec->element_count < MORSE_CODEC_MAX_ELEMENTS) {
            codec->elements[codec->element_count++] = element;
        }
    }

    codec->key_down = key_down;
    codec->last_edge_ms = timestamp_ms;
    return event;
}

morse_codec_event_t morse_codec_tick(morse_codec_t *codec, uint32_t timestamp_ms, char *out_char)
{
    if (!codec->started || codec->key_down) {
        return MORSE_CODEC_EVENT_NONE;
    }

    uint32_t duration_ms = timestamp_ms - codec->last_edge_ms;

    if (codec->element_count > 0) {
        if (duration_ms >= 2u * codec->unit_ms) {
            return flush_char(codec, out_char);
        }
        return MORSE_CODEC_EVENT_NONE;
    }

    if (!codec->space_emitted && duration_ms >= 5u * codec->unit_ms) {
        codec->space_emitted = true;
        return MORSE_CODEC_EVENT_SPACE;
    }

    return MORSE_CODEC_EVENT_NONE;
}
