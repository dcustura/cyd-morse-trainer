#include "morse_text.h"

#include <ctype.h>
#include <string.h>

#include "morse_codec.h"

#define PROSIGN_TEXT_LEN 4 /* "<XX>" */

static char upper(char ch)
{
    return (char)toupper((unsigned char)ch);
}

/* Pattern for a plain (non-space, non-bracket) character, or NULL. Bytes
 * >= 0x80 are rejected so stray UTF-8 can't alias a decoder prosign
 * sentinel. */
static const char *char_pattern(char ch)
{
    if ((unsigned char)ch >= 0x80 || ch == ' ') {
        return NULL;
    }
    return morse_codec_pattern_for_char(upper(ch));
}

/* Parse a bracketed prosign starting at text[0] == '<'. On success, writes
 * its uppercase two-letter name into name[3] and the character it decodes
 * to into *ch, and returns MORSE_TEXT_OK. */
static morse_text_result_t parse_prosign(const char *text, char name[3], char *ch)
{
    const char *close = strchr(text + 1, '>');
    if (close == NULL) {
        return MORSE_TEXT_ERR_UNCLOSED_PROSIGN;
    }
    size_t len = (size_t)(close - (text + 1));
    if (len == 0) {
        return MORSE_TEXT_ERR_EMPTY_PROSIGN;
    }
    if (len != 2) {
        return MORSE_TEXT_ERR_UNKNOWN_PROSIGN;
    }
    name[0] = upper(text[1]);
    name[1] = upper(text[2]);
    name[2] = '\0';
    *ch = morse_codec_prosign_char(name);
    return (*ch != 0) ? MORSE_TEXT_OK : MORSE_TEXT_ERR_UNKNOWN_PROSIGN;
}

morse_text_result_t morse_text_normalize(const char *in, bool allow_prosigns, char *out, size_t out_size,
                                         size_t *err_pos)
{
    size_t n = 0;
    bool pending_space = false;
    size_t i = 0;
    morse_text_result_t result = MORSE_TEXT_OK;

    if (out_size == 0) {
        if (err_pos != NULL) {
            *err_pos = 0;
        }
        return MORSE_TEXT_ERR_TOO_LONG;
    }

    while (in[i] != '\0') {
        if (in[i] == ' ') {
            pending_space = (n > 0);
            ++i;
            continue;
        }

        char token[PROSIGN_TEXT_LEN];
        size_t token_len;
        if (in[i] == '<' && allow_prosigns) {
            char name[3];
            char prosign_ch;
            result = parse_prosign(&in[i], name, &prosign_ch);
            if (result != MORSE_TEXT_OK) {
                break;
            }
            token[0] = '<';
            token[1] = name[0];
            token[2] = name[1];
            token[3] = '>';
            token_len = PROSIGN_TEXT_LEN;
        } else if (char_pattern(in[i]) != NULL) {
            token[0] = upper(in[i]);
            token_len = 1;
        } else {
            result = MORSE_TEXT_ERR_INVALID_CHAR;
            break;
        }

        size_t needed = token_len + (pending_space ? 1 : 0);
        if (n + needed + 1 > out_size) {
            result = MORSE_TEXT_ERR_TOO_LONG;
            break;
        }
        if (pending_space) {
            out[n++] = ' ';
            pending_space = false;
        }
        memcpy(&out[n], token, token_len);
        n += token_len;
        i += token_len;
    }

    if (result != MORSE_TEXT_OK) {
        if (err_pos != NULL) {
            *err_pos = i;
        }
        return result;
    }
    out[n] = '\0';
    return MORSE_TEXT_OK;
}

const char *morse_text_result_str(morse_text_result_t result)
{
    switch (result) {
    case MORSE_TEXT_OK:
        return "ok";
    case MORSE_TEXT_ERR_TOO_LONG:
        return "too long";
    case MORSE_TEXT_ERR_INVALID_CHAR:
        return "invalid character";
    case MORSE_TEXT_ERR_UNCLOSED_PROSIGN:
        return "unclosed <";
    case MORSE_TEXT_ERR_EMPTY_PROSIGN:
        return "empty <>";
    case MORSE_TEXT_ERR_UNKNOWN_PROSIGN:
        return "unknown prosign";
    }
    return "unknown error";
}

bool morse_text_next_token(const char *text, size_t *pos, morse_text_token_t *out)
{
    size_t i = *pos;
    bool gap = false;
    while (text[i] == ' ') {
        gap = true;
        ++i;
    }
    if (text[i] == '\0') {
        return false;
    }

    out->text_pos = i;
    out->word_gap_before = gap && *pos > 0; /* leading spaces aren't a gap */

    if (text[i] == '<') {
        char name[3];
        char prosign_ch;
        if (parse_prosign(&text[i], name, &prosign_ch) != MORSE_TEXT_OK) {
            return false;
        }
        strcpy(out->pattern, morse_codec_pattern_for_char(prosign_ch));
        out->text_len = PROSIGN_TEXT_LEN;
    } else {
        const char *pattern = char_pattern(text[i]);
        if (pattern == NULL) {
            return false;
        }
        strcpy(out->pattern, pattern);
        out->text_len = 1;
    }

    *pos = i + out->text_len;
    return true;
}
