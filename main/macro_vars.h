#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "morse_text.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * User-defined message variables: up to MACRO_VARS_MAX name/value pairs,
 * saved to flash, that {NAME} placeholders in memories and CLI `send` text
 * are replaced with when a message starts (e.g. MYCALL = DL1ABC/P for
 * "CQ DE {MYCALL} K").
 *
 * Every function is safe to call from any task.
 */

#define MACRO_VARS_MAX 8
#define MACRO_VAR_VALUE_MAX 16 /* chars, excluding the terminator */
/* morse_text_normalize() flags for a value: prosigns, but no {VAR}s. */
#define MACRO_VAR_VALUE_FLAGS MORSE_TEXT_ALLOW_PROSIGNS

typedef struct {
    char name[MORSE_TEXT_VAR_NAME_MAX + 1];  /* normalized (uppercase) */
    char value[MACRO_VAR_VALUE_MAX + 1];     /* normalized, never empty */
} macro_var_t;

/** Load the variables. Call once at boot, after NVS init. */
esp_err_t macro_vars_init(void);

/** Copy the defined variables, sorted by name, into out; returns how many. */
size_t macro_vars_list(macro_var_t out[MACRO_VARS_MAX]);

/**
 * Define or replace a variable and persist it. name must be normalized
 * with morse_text_normalize_var_name() and value with morse_text_normalize()
 * (MACRO_VAR_VALUE_FLAGS), non-empty.
 *
 * @return ESP_OK; ESP_ERR_INVALID_ARG for a bad name or value;
 *         ESP_ERR_NO_MEM if name is new and all MACRO_VARS_MAX are in use;
 *         or an NVS error (nothing changed).
 */
esp_err_t macro_vars_set(const char *name, const char *value);

/**
 * Delete a variable and persist the change.
 *
 * @return ESP_OK; ESP_ERR_NOT_FOUND if it isn't defined; or an NVS error.
 */
esp_err_t macro_vars_clear(const char *name);

/** Delete every variable (Settings > Reset to Defaults). */
esp_err_t macro_vars_reset(void);

/**
 * morse_text_expand() against the current variables: fill normalized
 * message text's {NAME}s into out. See morse_text_expand() for the results.
 */
morse_text_result_t macro_vars_expand(const char *text, char *out, size_t out_size,
                                      char missing[MORSE_TEXT_VAR_NAME_MAX + 1]);

#ifdef __cplusplus
}
#endif
