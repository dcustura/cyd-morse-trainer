#include "paddle_input.h"

#include "board_pins.h"
#include "sidetone.h"
#include "example_component.h"
#include "morse_codec.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "paddle_input";

#define PADDLE_TASK_STACK 4096
#define PADDLE_TASK_PRIO 5
#define PADDLE_TICK_MS 1
#define PADDLE_DEBOUNCE_THRESHOLD 4 /* ~4ms of consistent contact before accepting an edge */
#define MORSE_TICK_INTERVAL_MS 30

static iambic_keyer_t s_keyer;
static morse_codec_t s_codec;
static volatile bool s_paddle_swap;

static void configure_input_gpio(int gpio)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

static void log_decode_event(morse_codec_event_t event, char out_char)
{
    switch (event) {
    case MORSE_CODEC_EVENT_CHAR:
        ESP_LOGI(TAG, "decoded: %c", out_char);
        break;
    case MORSE_CODEC_EVENT_UNKNOWN:
        ESP_LOGI(TAG, "decoded: ? (unknown sequence)");
        break;
    case MORSE_CODEC_EVENT_SPACE:
        ESP_LOGI(TAG, "decoded: <space>");
        break;
    default:
        break;
    }
}

static void paddle_task(void *arg)
{
    (void)arg;

    configure_input_gpio(BOARD_PADDLE_DIT_GPIO);
    configure_input_gpio(BOARD_PADDLE_DAH_GPIO);

    example_component_debounce_t dit_db;
    example_component_debounce_t dah_db;
    example_component_debounce_init(&dit_db, PADDLE_DEBOUNCE_THRESHOLD, false);
    example_component_debounce_init(&dah_db, PADDLE_DEBOUNCE_THRESHOLD, false);

    bool prev_key_down = false;
    uint32_t last_morse_tick_ms = 0;

    while (1) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

        bool raw_dit = !gpio_get_level(BOARD_PADDLE_DIT_GPIO); /* active-low: pressed = 0 */
        bool raw_dah = !gpio_get_level(BOARD_PADDLE_DAH_GPIO);
        bool swap = s_paddle_swap;

        bool dit_contact = example_component_debounce_feed(&dit_db, swap ? raw_dah : raw_dit);
        bool dah_contact = example_component_debounce_feed(&dah_db, swap ? raw_dit : raw_dah);

        bool key_down = iambic_keyer_service(&s_keyer, dit_contact, dah_contact, now_ms);

        if (key_down != prev_key_down) {
            sidetone_key(key_down);
            char out_char = 0;
            morse_codec_event_t event = morse_codec_key_event(&s_codec, key_down, now_ms, &out_char);
            log_decode_event(event, out_char);
            prev_key_down = key_down;
        }

        if (now_ms - last_morse_tick_ms >= MORSE_TICK_INTERVAL_MS) {
            last_morse_tick_ms = now_ms;
            char out_char = 0;
            morse_codec_event_t event = morse_codec_tick(&s_codec, now_ms, &out_char);
            log_decode_event(event, out_char);
        }

        vTaskDelay(pdMS_TO_TICKS(PADDLE_TICK_MS));
    }
}

void paddle_input_start(iambic_keyer_mode_t mode, uint16_t wpm, bool paddle_swap)
{
    iambic_keyer_init(&s_keyer, mode, wpm);
    morse_codec_init(&s_codec, wpm);
    s_paddle_swap = paddle_swap;

    sidetone_init();

    xTaskCreate(paddle_task, "paddle_input", PADDLE_TASK_STACK, NULL, PADDLE_TASK_PRIO, NULL);
}

void paddle_input_set_mode(iambic_keyer_mode_t mode)
{
    iambic_keyer_set_mode(&s_keyer, mode);
}

void paddle_input_set_wpm(uint16_t wpm)
{
    iambic_keyer_set_wpm(&s_keyer, wpm);
    morse_codec_set_wpm(&s_codec, wpm);
}

void paddle_input_set_swap(bool swap)
{
    s_paddle_swap = swap;
}
