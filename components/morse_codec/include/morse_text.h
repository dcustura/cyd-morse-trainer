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
 * A prosign is sent as the one unbroken sequence the decoder reads it from.
 *
 * Message text may also hold variables written in braces, e.g. {MYCALL}:
 * a name of 1-MORSE_TEXT_VAR_NAME_MAX letters and digits, which
 * morse_text_expand() replaces with the variable's value before sending.
 */

/** Longest dit/dah pattern of any character or prosign (<HH>: 8 dits). */
#define MORSE_TEXT_MAX_PATTERN 8

/** Longest variable name, excluding the braces and terminator. */
#define MORSE_TEXT_VAR_NAME_MAX 6

/** What morse_text_normalize() accepts beyond plain characters and spaces. */
#define MORSE_TEXT_ALLOW_PROSIGNS  0x1u /* <AR> etc. */
#define MORSE_TEXT_ALLOW_VARIABLES 0x2u /* {NAME} */

typedef enum {
    MORSE_TEXT_OK = 0,
    MORSE_TEXT_ERR_TOO_LONG,         /* normalized text doesn't fit out_size */
    MORSE_TEXT_ERR_INVALID_CHAR,     /* a character with no Morse pattern */
    MORSE_TEXT_ERR_UNCLOSED_PROSIGN, /* '<' with no matching '>' */
    MORSE_TEXT_ERR_EMPTY_PROSIGN,    /* "<>" */
    MORSE_TEXT_ERR_UNKNOWN_PROSIGN,  /* bracketed text that isn't one of the eight */
    MORSE_TEXT_ERR_UNCLOSED_VARIABLE, /* '{' with no matching '}' */
    MORSE_TEXT_ERR_BAD_VARIABLE,      /* "{}", or a name that isn't 1-6 letters/digits */
    MORSE_TEXT_ERR_UNSET_VARIABLE,    /* morse_text_expand(): no value for a variable */
} morse_text_result_t;

/**
 * Validate and normalize message text: convert to uppercase (variable
 * names too), collapse runs of spaces to one, and drop leading/trailing
 * spaces.
 *
 * @param in             NUL-terminated input text.
 * @param flags          MORSE_TEXT_ALLOW_* bits. Without ALLOW_PROSIGNS,
 *                       '<' and '>' are rejected as
 *                       MORSE_TEXT_ERR_INVALID_CHAR; without
 *                       ALLOW_VARIABLES, '{' and '}' are. Memory names
 *                       allow neither, variable values only prosigns.
 * @param out            Receives the normalized, NUL-terminated text. Only
 *                       valid when MORSE_TEXT_OK is returned.
 * @param out_size       Size of out including the terminator; the
 *                       normalized text may be at most out_size - 1 chars.
 * @param err_pos        Optional; on error, receives the offset into in of
 *                       the offending character.
 */
morse_text_result_t morse_text_normalize(const char *in, unsigned flags, char *out, size_t out_size,
                                         size_t *err_pos);

/**
 * Validate a variable name (without braces) and write it uppercased into
 * out. False if it isn't 1-MORSE_TEXT_VAR_NAME_MAX letters and digits.
 */
bool morse_text_normalize_var_name(const char *in, char out[MORSE_TEXT_VAR_NAME_MAX + 1]);

/**
 * Looks up a variable by its (uppercase) name. Returns its value, which
 * must be normalized text without variables, or NULL / "" if it has none.
 */
typedef const char *(*morse_text_var_lookup_t)(const char *name, void *ctx);

/**
 * Replace every {NAME} in normalized text with lookup(NAME)'s value.
 * The result is normalized text without variables.
 *
 * @param missing  Optional; on MORSE_TEXT_ERR_UNSET_VARIABLE, receives the
 *                 name of the first variable with no value.
 * @return MORSE_TEXT_OK; MORSE_TEXT_ERR_UNSET_VARIABLE;
 *         MORSE_TEXT_ERR_TOO_LONG if the result doesn't fit out_size; or a
 *         variable syntax error if text wasn't normalized.
 */
morse_text_result_t morse_text_expand(const char *text, morse_text_var_lookup_t lookup, void *ctx, char *out,
                                      size_t out_size, char missing[MORSE_TEXT_VAR_NAME_MAX + 1]);

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
 *         text, or on anything morse_text_normalize() would have rejected,
 *         or on a variable (expand them first with morse_text_expand()).
 */
bool morse_text_next_token(const char *text, size_t *pos, morse_text_token_t *out);

#ifdef __cplusplus
}
#endif
