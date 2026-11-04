#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Message text for sending: validation/normalization and encoding into
 * per-character dit/dah patterns.
 *
 * Text syntax: A-Z, 0-9, space, the punctuation the decoder knows
 * (. , ? ' ! / ( ) & : ; = + - _ " $ @), and the eight prosigns the decoder
 * knows written in angle brackets: <AR> <SK> <KN> <BT> <HH> <VE> <CT> <AS>.
 * The letters of a prosign are sent as one unbroken sequence.
 */

/** Longest dit/dah pattern of any character or prosign (<HH>: 8 dits). */
#define MORSE_TEXT_MAX_PATTERN 8

typedef enum {
    MORSE_TEXT_OK = 0,
    MORSE_TEXT_ERR_TOO_LONG,         /* normalized text doesn't fit out_size */
    MORSE_TEXT_ERR_INVALID_CHAR,     /* a character with no Morse pattern */
    MORSE_TEXT_ERR_UNCLOSED_PROSIGN, /* '<' with no matching '>' */
    MORSE_TEXT_ERR_EMPTY_PROSIGN,    /* "<>" */
    MORSE_TEXT_ERR_UNKNOWN_PROSIGN,  /* bracketed text that isn't one of the eight */
} morse_text_result_t;

/**
 * Validate and normalize message text: convert to uppercase, collapse runs
 * of spaces to one, and drop leading/trailing spaces.
 *
 * @param in             NUL-terminated input text.
 * @param allow_prosigns false for memory names: '<' and '>' are then
 *                       rejected as MORSE_TEXT_ERR_INVALID_CHAR.
 * @param out            Receives the normalized, NUL-terminated text. Only
 *                       valid when MORSE_TEXT_OK is returned.
 * @param out_size       Size of out including the terminator; the
 *                       normalized text may be at most out_size - 1 chars.
 * @param err_pos        Optional; on error, receives the offset into in of
 *                       the offending character.
 */
morse_text_result_t morse_text_normalize(const char *in, bool allow_prosigns, char *out, size_t out_size,
                                         size_t *err_pos);

/** A short English description of a result, for CLI error messages. */
const char *morse_text_result_str(morse_text_result_t result);

typedef struct {
    char pattern[MORSE_TEXT_MAX_PATTERN + 1]; /* '.'/'-' string */
    size_t text_pos;      /* offset of this token in the text */
    size_t text_len;      /* 1 for a character, 4 for a prosign such as "<AR>" */
    bool word_gap_before; /* true if one or more spaces precede this token */
} morse_text_token_t;

/**
 * Read the next sendable token from normalized text (as produced by
 * morse_text_normalize()), starting at *pos, and advance *pos past it.
 * Start with *pos = 0.
 *
 * @return true and fill *out if a token was read; false at the end of the
 *         text, or on anything morse_text_normalize() would have rejected.
 */
bool morse_text_next_token(const char *text, size_t *pos, morse_text_token_t *out);

#ifdef __cplusplus
}
#endif
