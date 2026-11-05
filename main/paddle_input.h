#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "iambic_keyer.h"
#include "message_player.h"
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure the paddle/straight-key GPIOs, initialize the sidetone driver,
 * and spawn the task that reads them, drives the iambic keyer and Morse
 * decoder, logs decoded characters via ESP_LOGI, and pushes each decoded
 * character (or ' ' for a word gap) into decoded_char_queue for the
 * Send screen to display. It doesn't touch the on-air state: main.c sets
 * that from settings->on_air with paddle_input_set_on_air() before the UI
 * comes up, so a quick toggle tapped before this runs isn't overwritten.
 */
void paddle_input_start(QueueHandle_t decoded_char_queue, const settings_t *settings);

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
 * Weight (dit/dah ratio, 50 = standard) for the keyer (Iambic A/B and
 * Ultimatic) and message playback. See components/iambic_keyer's
 * iambic_keyer_set_weight_pct().
 */
void paddle_input_set_weight_pct(uint8_t pct);

/**
 * Straight Key mode only: also read the paddle DAH GPIO (the "ring"
 * conductor on a TRS paddle/key jack) as part of the key signal, ORed with
 * the dit/"tip" GPIO -- for a key wired across both conductors. No effect
 * in any other key mode.
 */
void paddle_input_set_straight_ring_input(bool enabled);

/**
 * On-air: the external key output follows the key. Off-air: it's never
 * driven (sidetone, decoding and message playback still work). Takes
 * effect at once; going on-air mid-element waits for the next key-down
 * (see components/key_gate). Live state only: the saved boot value is
 * settings_t.on_air. Safe to call from any task.
 */
void paddle_input_set_on_air(bool on_air);
bool paddle_input_is_on_air(void);

/* Current keyer settings, kept live-updated by the setters above (used by
 * the Send screen to refresh its status label after a Settings save). */
iambic_keyer_mode_t paddle_input_get_mode(void);
uint16_t paddle_input_get_wpm(void);

/*
 * Message playback (message memories). While a message plays, it replaces
 * the keyer as the key source, so the sidetone, key output, keying
 * indicator and decoder all follow it. Touching either paddle or the key
 * stops it at once; that touch isn't keyed, and paddle input is ignored
 * until both paddles are released. Plays at the keyer's current WPM.
 * Safe to call from any task.
 */
message_player_enqueue_result_t paddle_input_message_enqueue(const message_player_message_t *msg);
void paddle_input_message_stop(void);
void paddle_input_message_set_repeat(bool repeat);
void paddle_input_message_set_repeat_params(uint8_t count, uint8_t pause_s);
bool paddle_input_message_is_active(void);

/**
 * Snapshot of playback progress. out->text points at text_buf, which
 * receives a copy of the current message's text.
 */
void paddle_input_message_get_status(message_player_status_t *out, char text_buf[MESSAGE_PLAYER_TEXT_MAX + 1]);

/* Clears in-progress decode state (called from the Send screen's Clear button). */
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
