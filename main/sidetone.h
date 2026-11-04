#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIDETONE_DEFAULT_HZ 600

/**
 * Configure the ESP32 DAC continuous channel driving the speaker GPIO and
 * start the background task that synthesizes the sidetone (sine wave,
 * raised-cosine keying envelope, volume scaling). Must be called once at
 * boot, before sidetone_set_source().
 */
void sidetone_init(void);

/** Change the sidetone pitch. Takes effect within one audio chunk (a few ms). */
void sidetone_set_freq(uint16_t hz);

/** Set the sidetone volume as a percentage (0-100) of full-scale DAC amplitude. */
void sidetone_set_volume(uint8_t percent);

/**
 * Set the raised-cosine keying envelope's attack/decay duration in
 * milliseconds (clamped to the driver's supported range). Takes effect on
 * the next key transition; a change made mid-tone does not disturb a ramp
 * already in progress.
 */
void sidetone_set_envelope_ms(uint16_t ms);

/* Things that can sound the sidetone. Each turns only its own bit on and
 * off, so one can never silence (or get stuck sounding over) another. */
typedef enum {
    SIDETONE_SOURCE_KEY = 1u << 0,       /* keyer, straight key or message playback */
    SIDETONE_SOURCE_REFERENCE = 1u << 1, /* Morse Reference screen playback */
    SIDETONE_SOURCE_TEST = 1u << 2,      /* Settings > Sidetone test tone */
} sidetone_source_t;

/**
 * Turn source's request for the tone on or off; the tone sounds while any
 * source requests it. Transitions are shaped by a raised-cosine
 * attack/decay envelope (see sidetone_set_envelope_ms()) rather than
 * switching instantly, to avoid audible keying clicks. Safe from any task.
 */
void sidetone_set_source(sidetone_source_t source, bool on);

#ifdef __cplusplus
}
#endif
