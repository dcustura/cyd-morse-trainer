#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "morse_text.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The message memory library (32 memories) and the memories panel's six
 * tile assignments, cached in RAM and persisted to their own NVS namespace.
 * Every function is safe to call from any task.
 */

#define MEMORY_COUNT 32
#define MEMORY_TILE_COUNT 6
#define MEMORY_NAME_MAX 8  /* chars, excluding the terminator */
#define MEMORY_TEXT_MAX 80 /* chars, excluding the terminator */
#define MEMORY_TILE_NONE 0 /* tile assignment meaning "no memory" */
/* morse_text_normalize() flags for a memory's text: prosigns and {VAR}s.
 * Names allow neither (flags 0). */
#define MEMORY_TEXT_FLAGS (MORSE_TEXT_ALLOW_PROSIGNS | MORSE_TEXT_ALLOW_VARIABLES)

#define MEMORY_DEFAULT_PAUSE_S 10

typedef struct {
    char name[MEMORY_NAME_MAX + 1]; /* normalized (uppercase), may be empty */
    char text[MEMORY_TEXT_MAX + 1]; /* normalized message text, may hold {VAR}s, may be empty */
    bool repeat;
    uint8_t count;   /* 0-99; 0 = repeat until stopped */
    uint8_t pause_s; /* 1-99 */
} memory_t;

/** Load all memories and tile assignments. Call once at boot, after NVS init. */
esp_err_t memory_store_init(void);

/** The empty default memory. */
void memory_store_default(memory_t *out);

/** Copy memory n (1-based, 1..MEMORY_COUNT) into *out. */
void memory_store_get(uint8_t n, memory_t *out);

/**
 * Replace memory n (1-based) and persist it. The caller must pass a name
 * and text already normalized with morse_text_normalize(); count and pause
 * are clamped.
 */
esp_err_t memory_store_set(uint8_t n, const memory_t *in);

/** Memory assigned to tile t (1-based, 1..MEMORY_TILE_COUNT), or MEMORY_TILE_NONE. */
uint8_t memory_store_get_tile(uint8_t t);

/** Assign memory n (or MEMORY_TILE_NONE) to tile t (1-based) and persist it. */
esp_err_t memory_store_set_tile(uint8_t t, uint8_t n);

/**
 * Incremented on every change, so a screen showing memories can tell
 * cheaply whether it needs to refresh.
 */
uint32_t memory_store_generation(void);

/** Erase all stored memories and tile assignments and reload the defaults. */
esp_err_t memory_store_reset(void);

#ifdef __cplusplus
}
#endif
