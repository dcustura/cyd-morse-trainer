#include "paddle_input.h"

#include "board_pins.h"
#include "sidetone.h"
#include "debouncer.h"
#include "key_gate.h"
#include "morse_codec.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdatomic.h>
#include <string.h>

static const char *TAG = "paddle_input";

#define PADDLE_TASK_STACK 4096
#define PADDLE_TASK_PRIO 5
#define PADDLE_TICK_MS 1
#define PADDLE_DEBOUNCE_THRESHOLD 4 /* ~4ms of consistent contact before accepting an edge */
#define MORSE_TICK_INTERVAL_MS 30

static iambic_keyer_t s_keyer;
static morse_codec_t s_codec;
static volatile bool s_paddle_swap;
static volatile bool s_paddle_debounce;
static volatile bool s_straight_ring_input;
static QueueHandle_t s_decoded_char_queue;
static _Atomic bool s_key_down_state;
static _Atomic bool s_on_air;

/* Shared with the UI and CLI tasks, so every access holds s_player_lock. */
static message_player_t s_player;
static SemaphoreHandle_t s_player_lock;

static void player_lock(void)
{
    xSemaphoreTake(s_player_lock, portMAX_DELAY);
}

static void player_unlock(void)
{
    xSemaphoreGive(s_player_lock);
}

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void configure_input_gpio(int gpio)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

#define KEY_OUTPUT_ON_LEVEL  0 /* active-low: matches the CYD's common-anode onboard RGB LED */
#define KEY_OUTPUT_OFF_LEVEL 1

static void configure_output_gpio(int gpio, int initial_level)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(gpio, initial_level);
}

static void handle_decode_event(morse_codec_event_t event, char out_char)
{
    char queued_char;

    switch (event) {
    case MORSE_CODEC_EVENT_CHAR:
        ESP_LOGI(TAG, "decoded: %c", out_char);
        queued_char = out_char;
        break;
    case MORSE_CODEC_EVENT_UNKNOWN:
        ESP_LOGI(TAG, "decoded: %c (unknown sequence)", MORSE_CODEC_UNKNOWN_CHAR);
        queued_char = MORSE_CODEC_UNKNOWN_CHAR;
        break;
    case MORSE_CODEC_EVENT_SPACE:
        ESP_LOGI(TAG, "decoded: <space>");
        queued_char = ' ';
        break;
    default:
        return;
    }

    if (s_decoded_char_queue != NULL) {
        xQueueSend(s_decoded_char_queue, &queued_char, 0);
    }
}

static void paddle_task(void *arg)
{
    (void)arg;

    configure_input_gpio(BOARD_PADDLE_DIT_GPIO);
    configure_input_gpio(BOARD_PADDLE_DAH_GPIO);
    configure_output_gpio(BOARD_KEY_OUTPUT_GPIO, KEY_OUTPUT_OFF_LEVEL);

    debouncer_t dit_db;
    debouncer_t dah_db;
    debouncer_init(&dit_db, PADDLE_DEBOUNCE_THRESHOLD, false);
    debouncer_init(&dah_db, PADDLE_DEBOUNCE_THRESHOLD, false);

    bool prev_key_down = false;
    key_gate_t key_gate;
    key_gate_init(&key_gate);
    bool prev_output = false;
    bool ignore_until_released = false;
    uint32_t last_morse_tick_ms = 0;

    while (1) {
        uint32_t now = now_ms();

        bool raw_dit = !gpio_get_level(BOARD_PADDLE_DIT_GPIO); /* active-low: pressed = 0 */
        bool raw_dah = !gpio_get_level(BOARD_PADDLE_DAH_GPIO);
        bool swap = s_paddle_swap;
        bool straight = s_keyer.mode == IAMBIC_KEYER_MODE_STRAIGHT;

        /* Paddle Swap has no effect in Straight Key mode -- its tile isn't
         * even shown there (see ui_settings.c) -- so the key signal always
         * starts from the real dit/"tip" GPIO, unswapped, regardless of
         * whatever Paddle Swap was left set to in another key mode. A key
         * wired to the other ("ring") terminal is handled by Ring Input's
         * OR-fold below instead, not by swapping here. */
        bool swapped_dit = (!straight && swap) ? raw_dah : raw_dit;
        bool swapped_dah = (!straight && swap) ? raw_dit : raw_dah;

        /* Straight key contact always bounces mechanically, so it's always
         * debounced regardless of the setting; paddle debounce is optional
         * since clean paddle contacts don't usually need it. */
        bool debounce = straight || s_paddle_debounce;

        bool dit_contact, dah_contact;
        if (debounce) {
            dit_contact = debouncer_feed(&dit_db, swapped_dit);
            dah_contact = debouncer_feed(&dah_db, swapped_dah);
        } else {
            dit_contact = swapped_dit;
            dah_contact = swapped_dah;
        }

        /* Straight Key mode only: fold in the "ring" (dah) GPIO as part of
         * the key signal too, for a key wired across both conductors of a
         * TRS jack. No effect in any other key mode -- dah_contact is
         * otherwise unused by the keyer in Straight Key mode. */
        if (straight && s_straight_ring_input) {
            dit_contact = dit_contact || dah_contact;
        }

        /* A message being played owns the key. Any contact stops it; that
         * contact isn't keyed, and paddle input stays ignored until both
         * paddles are released, so the stop never turns into a stray
         * element. */
        bool any_contact = dit_contact || dah_contact;
        player_lock();
        bool playing = message_player_is_active(&s_player);
        if (playing && any_contact) {
            message_player_stop(&s_player);
            playing = false;
            ignore_until_released = true;
        }
        bool message_key_down = playing && message_player_service(&s_player, now);

        if (ignore_until_released) {
            if (any_contact) {
                dit_contact = false;
                dah_contact = false;
            } else {
                ignore_until_released = false;
            }
        }

        /* Still under the player lock (the keyer service is quick), so the
         * key-down is recorded before any enqueue can see this tick. */
        bool keyer_key_down = iambic_keyer_service(&s_keyer, dit_contact && !playing, dah_contact && !playing, now);
        if (!playing) {
            /* So a message started now waits a word gap after a key-down. */
            message_player_note_key(&s_player, keyer_key_down, now);
        }
        player_unlock();

        bool key_down = playing ? message_key_down : keyer_key_down;

        if (key_down != prev_key_down) {
            sidetone_set_source(SIDETONE_SOURCE_KEY, key_down);
            atomic_store_explicit(&s_key_down_state, key_down, memory_order_relaxed);
            char out_char = 0;
            morse_codec_event_t event = morse_codec_key_event(&s_codec, key_down, now, &out_char);
            handle_decode_event(event, out_char);
            prev_key_down = key_down;
        }

        /* Checked every tick, not just on key edges, so going off-air
         * mid-element releases the transmitter at once. */
        bool on_air = atomic_load_explicit(&s_on_air, memory_order_relaxed);
        bool output = key_gate_update(&key_gate, key_down, on_air);
        if (output != prev_output) {
            gpio_set_level(BOARD_KEY_OUTPUT_GPIO, output ? KEY_OUTPUT_ON_LEVEL : KEY_OUTPUT_OFF_LEVEL);
            prev_output = output;
        }

        if (now - last_morse_tick_ms >= MORSE_TICK_INTERVAL_MS) {
            last_morse_tick_ms = now;
            char out_char = 0;
            morse_codec_event_t event = morse_codec_tick(&s_codec, now, &out_char);
            handle_decode_event(event, out_char);
        }

        vTaskDelay(pdMS_TO_TICKS(PADDLE_TICK_MS));
    }
}

void paddle_input_start(QueueHandle_t decoded_char_queue, const settings_t *settings)
{
    iambic_keyer_init(&s_keyer, settings->keymode, settings->wpm);
    iambic_keyer_set_auto_space_pct(&s_keyer, settings->auto_space_pct);
    iambic_keyer_set_weight_pct(&s_keyer, settings->weight_pct);
    morse_codec_init(&s_codec, settings->wpm);
    message_player_init(&s_player, settings->wpm);
    message_player_set_weight_pct(&s_player, settings->weight_pct);
    s_player_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(s_player_lock != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    s_paddle_swap = settings->paddle_swap;
    s_paddle_debounce = settings->paddle_debounce;
    s_straight_ring_input = settings->straight_ring_input;
    s_decoded_char_queue = decoded_char_queue;

    sidetone_init();
    sidetone_set_freq(settings->tone_hz);
    sidetone_set_volume(settings->volume_pct);
    sidetone_set_envelope_ms(settings->envelope_ms);

    xTaskCreate(paddle_task, "paddle_input", PADDLE_TASK_STACK, NULL, PADDLE_TASK_PRIO, NULL);
}

void paddle_input_reset_decoder(void)
{
    morse_codec_reset(&s_codec);
}

bool paddle_input_is_keying(void)
{
    return atomic_load_explicit(&s_key_down_state, memory_order_relaxed);
}

void paddle_input_set_mode(iambic_keyer_mode_t mode)
{
    iambic_keyer_set_mode(&s_keyer, mode);
}

void paddle_input_set_wpm(uint16_t wpm)
{
    iambic_keyer_set_wpm(&s_keyer, wpm);
    morse_codec_set_wpm(&s_codec, wpm);
    player_lock();
    message_player_set_wpm(&s_player, wpm);
    player_unlock();
}

message_player_enqueue_result_t paddle_input_message_enqueue(const message_player_message_t *msg)
{
    player_lock();
    message_player_enqueue_result_t result = message_player_enqueue(&s_player, msg, now_ms());
    player_unlock();
    return result;
}

void paddle_input_message_stop(void)
{
    player_lock();
    message_player_stop(&s_player);
    player_unlock();
}

void paddle_input_message_set_repeat(bool repeat)
{
    player_lock();
    message_player_set_repeat(&s_player, repeat);
    player_unlock();
}

void paddle_input_message_set_repeat_params(uint8_t count, uint8_t pause_s)
{
    player_lock();
    message_player_set_repeat_params(&s_player, count, pause_s, now_ms());
    player_unlock();
}

bool paddle_input_message_is_active(void)
{
    player_lock();
    bool active = message_player_is_active(&s_player);
    player_unlock();
    return active;
}

void paddle_input_message_get_status(message_player_status_t *out, char text_buf[MESSAGE_PLAYER_TEXT_MAX + 1])
{
    player_lock();
    message_player_get_status(&s_player, now_ms(), out);
    text_buf[0] = '\0';
    if (out->text != NULL) {
        strlcpy(text_buf, out->text, MESSAGE_PLAYER_TEXT_MAX + 1);
    }
    out->text = text_buf;
    player_unlock();
}

void paddle_input_set_swap(bool swap)
{
    s_paddle_swap = swap;
}

void paddle_input_set_debounce(bool debounce)
{
    s_paddle_debounce = debounce;
}

void paddle_input_set_straight_ring_input(bool enabled)
{
    s_straight_ring_input = enabled;
}

void paddle_input_set_on_air(bool on_air)
{
    atomic_store(&s_on_air, on_air);
    ESP_LOGI(TAG, "%s", on_air ? "on-air: key output live" : "off-air: key output disabled");
}

bool paddle_input_is_on_air(void)
{
    return atomic_load(&s_on_air);
}

void paddle_input_set_auto_space_pct(uint8_t pct)
{
    iambic_keyer_set_auto_space_pct(&s_keyer, pct);
}

void paddle_input_set_weight_pct(uint8_t pct)
{
    iambic_keyer_set_weight_pct(&s_keyer, pct);
    player_lock();
    message_player_set_weight_pct(&s_player, pct);
    player_unlock();
}

iambic_keyer_mode_t paddle_input_get_mode(void)
{
    return s_keyer.mode;
}

uint16_t paddle_input_get_wpm(void)
{
    return s_keyer.wpm;
}
