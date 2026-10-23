#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "iambic_keyer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure the paddle/straight-key GPIOs, initialize the sidetone driver,
 * and spawn the task that reads them, drives the iambic keyer and Morse
 * decoder, logs decoded characters via ESP_LOGI, and pushes each decoded
 * character (or ' ' for a word gap) into decoded_char_queue for the
 * Practice screen to display.
 */
void paddle_input_start(QueueHandle_t decoded_char_queue, iambic_keyer_mode_t mode, uint16_t wpm,
                         bool paddle_swap, bool paddle_debounce, uint16_t tone_hz, uint8_t volume_pct,
                         uint16_t envelope_ms, bool straight_ring_input, uint8_t auto_space_pct);

/* Live-apply setting changes (called from the Settings screen). */
void paddle_input_set_mode(iambic_keyer_mode_t mode);
void paddle_input_set_wpm(uint16_t wpm);
void paddle_input_set_swap(bool swap);

/* Enables/disables debounce while in paddle (Mode A/B) keying; Straight Key
 * mode is always debounced regardless of this setting. */
void paddle_input_set_debounce(bool debounce);

/**
 * Auto Space: in Iambic Mode A/B and Ultimatic (no effect in Straight
 * Key), withholds starting a new element after too short a pause so the
 * actual generated signal is correctly spaced (100 = disabled, an element
 * always starts the instant a paddle is touched, however short the
 * preceding pause; lower percentages withhold on a correspondingly
 * shorter pause). See components/iambic_keyer's
 * iambic_keyer_set_auto_space_pct().
 */
void paddle_input_set_auto_space_pct(uint8_t pct);

/**
 * Straight Key mode only: also read the paddle DAH GPIO (the "ring"
 * conductor on a TRS paddle/key jack) as part of the key signal, ORed with
 * the dit/"tip" GPIO -- for a key wired across both conductors. No effect
 * in any other key mode.
 */
void paddle_input_set_straight_ring_input(bool enabled);

/* Current keyer settings, kept live-updated by the setters above (used by
 * the Practice screen to refresh its status label after a Settings save). */
iambic_keyer_mode_t paddle_input_get_mode(void);
uint16_t paddle_input_get_wpm(void);

/* Clears in-progress decode state (called from the Practice screen's Clear button). */
void paddle_input_reset_decoder(void);

/**
 * True while the key (straight key or either paddle, per the iambic keyer)
 * is down and the sidetone is sounding. Safe to poll from the UI task to
 * drive a visual keying indicator.
 */
bool paddle_input_is_keying(void);

#ifdef __cplusplus
}
#endif
