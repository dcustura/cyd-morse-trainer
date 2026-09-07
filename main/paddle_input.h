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
                         bool paddle_swap, uint16_t tone_hz, uint8_t volume_pct, uint16_t envelope_ms);

/* Live-apply setting changes (called from the Settings screen). */
void paddle_input_set_mode(iambic_keyer_mode_t mode);
void paddle_input_set_wpm(uint16_t wpm);
void paddle_input_set_swap(bool swap);

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
