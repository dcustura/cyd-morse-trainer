#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "iambic_keyer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure the paddle/straight-key GPIOs, initialize the sidetone driver,
 * and spawn the task that reads them, drives the iambic keyer and Morse
 * decoder, and logs decoded characters via ESP_LOGI.
 */
void paddle_input_start(iambic_keyer_mode_t mode, uint16_t wpm, bool paddle_swap);

/* Live-apply setting changes (called from the Settings screen). */
void paddle_input_set_mode(iambic_keyer_mode_t mode);
void paddle_input_set_wpm(uint16_t wpm);
void paddle_input_set_swap(bool swap);

#ifdef __cplusplus
}
#endif
