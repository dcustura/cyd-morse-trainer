#include "sidetone.h"

#include "board_pins.h"
#include "driver/dac_continuous.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdatomic.h>

/*
 * The speaker is wired to one of the ESP32's two built-in 8-bit DACs
 * (GPIO25 = DAC_CHAN_0, GPIO26 = DAC_CHAN_1), not a generic PWM-capable pin,
 * so the DAC channel below must track BOARD_SPEAKER_GPIO if that Kconfig
 * value ever changes.
 */
#define SIDETONE_DAC_CHANNEL_MASK  DAC_CHANNEL_MASK_CH1 /* GPIO26 = DAC_CHAN_1 */

_Static_assert(BOARD_SPEAKER_GPIO == 26,
               "sidetone.c drives the ESP32 DAC directly (GPIO25/DAC_CHAN_0 or GPIO26/DAC_CHAN_1 only); "
               "update SIDETONE_DAC_CHANNEL_MASK to match the new BOARD_SPEAKER_GPIO.");

/* Sample rate must exceed the ~19.6kHz minimum for DAC_DIGI_CLK_SRC_DEFAULT continuous mode. */
#define SIDETONE_SAMPLE_RATE_HZ    32000u
/* ~4ms per chunk: bounds how long a sidetone_key()/sidetone_set_freq() change takes to be heard. */
#define SIDETONE_CHUNK_SAMPLES     128u

#define SIDETONE_SINE_TABLE_BITS   8u
#define SIDETONE_SINE_TABLE_LEN    (1u << SIDETONE_SINE_TABLE_BITS)

/*
 * Raised-cosine keying envelope (avoids the key clicks a hard on/off would
 * produce). Runtime-adjustable via sidetone_set_envelope_ms(); MS_MIN/MAX
 * here just bound the fixed-size table below and should stay in sync with
 * MORSE_SETTINGS_ENVELOPE_MS_MIN/MAX in morse_settings.h, which own the
 * user-facing range.
 */
#define SIDETONE_ENVELOPE_MS_MIN    2u
#define SIDETONE_ENVELOPE_MS_MAX    100u
#define SIDETONE_ENVELOPE_MS_DEFAULT 10u
#define SIDETONE_ENVELOPE_SAMPLES_MAX  ((SIDETONE_SAMPLE_RATE_HZ * SIDETONE_ENVELOPE_MS_MAX) / 1000u)

typedef enum {
    ENV_IDLE,
    ENV_ATTACK,
    ENV_SUSTAIN,
    ENV_RELEASE,
} envelope_state_t;

static dac_continuous_handle_t s_dac_handle;
static int8_t s_sine_table[SIDETONE_SINE_TABLE_LEN];
static float s_envelope_ramp[SIDETONE_ENVELOPE_SAMPLES_MAX]; /* monotonic 0 -> 1, first s_envelope_len entries valid */

static _Atomic uint32_t s_phase_incr;
static _Atomic uint32_t s_volume_q16; /* volume fraction in Q16 fixed point, 0..65536 */
static _Atomic uint32_t s_envelope_len; /* number of valid entries in s_envelope_ramp */
static _Atomic bool s_key_down;
static TaskHandle_t s_audio_task_handle;

/* Only ever touched by audio_task(), so it needs no synchronization. */
static envelope_state_t s_env_state = ENV_IDLE;
static uint32_t s_env_idx;

static uint32_t freq_to_phase_incr(uint16_t hz)
{
    return (uint32_t)(((uint64_t)hz << 32) / SIDETONE_SAMPLE_RATE_HZ);
}

static void generate_sine_table(void)
{
    for (uint32_t i = 0; i < SIDETONE_SINE_TABLE_LEN; i++) {
        float rad = 2.0f * (float)M_PI * (float)i / (float)SIDETONE_SINE_TABLE_LEN;
        s_sine_table[i] = (int8_t)lrintf(127.0f * sinf(rad));
    }
}

/* Regenerates s_envelope_ramp for a new duration and atomically publishes its length last. */
static void generate_envelope_ramp(uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        s_envelope_ramp[i] = 0.5f * (1.0f - cosf((float)M_PI * (float)(i + 1) / (float)len));
    }
    atomic_store_explicit(&s_envelope_len, len, memory_order_relaxed);
}

/* Advances the envelope by one sample and returns its current value (0..1). */
static float next_envelope_value(void)
{
    bool key_down = atomic_load_explicit(&s_key_down, memory_order_relaxed);
    /*
     * Captured once per sample so a concurrent sidetone_set_envelope_ms()
     * can't tear mid-calculation; s_envelope_ramp always has at least `len`
     * valid entries for whatever `len` was current when read, so this stays
     * in-bounds even if the length changes between samples.
     */
    uint32_t len = atomic_load_explicit(&s_envelope_len, memory_order_relaxed);
    /*
     * s_env_idx persists across calls, but len can shrink between calls (a
     * concurrent sidetone_set_envelope_ms()). Clamp before using it as an
     * index or in a (len - 1) - idx subtraction, otherwise a stale idx from
     * a longer envelope would underflow that subtraction into a huge
     * out-of-bounds index.
     */
    if (s_env_idx >= len) {
        s_env_idx = len - 1;
    }

    switch (s_env_state) {
    case ENV_IDLE:
        if (key_down) {
            s_env_state = ENV_ATTACK;
            s_env_idx = 0;
        }
        break;
    case ENV_ATTACK:
        if (!key_down) {
            /* Continue smoothly from the current level instead of jumping. */
            s_env_state = ENV_RELEASE;
            s_env_idx = (len - 1) - s_env_idx;
        }
        break;
    case ENV_SUSTAIN:
        if (!key_down) {
            s_env_state = ENV_RELEASE;
            s_env_idx = 0;
        }
        break;
    case ENV_RELEASE:
        if (key_down) {
            s_env_state = ENV_ATTACK;
            s_env_idx = (len - 1) - s_env_idx;
        }
        break;
    }

    float value;
    switch (s_env_state) {
    case ENV_ATTACK:
        value = s_envelope_ramp[s_env_idx];
        s_env_idx++;
        if (s_env_idx >= len) {
            s_env_state = ENV_SUSTAIN;
        }
        break;
    case ENV_SUSTAIN:
        value = 1.0f;
        break;
    case ENV_RELEASE:
        value = s_envelope_ramp[(len - 1) - s_env_idx];
        s_env_idx++;
        if (s_env_idx >= len) {
            s_env_state = ENV_IDLE;
        }
        break;
    case ENV_IDLE:
    default:
        value = 0.0f;
        break;
    }
    return value;
}

static void audio_task(void *arg)
{
    (void)arg;
    uint32_t phase = 0;
    uint8_t chunk[SIDETONE_CHUNK_SAMPLES];

    while (1) {
        if (s_env_state == ENV_IDLE && !atomic_load_explicit(&s_key_down, memory_order_relaxed)) {
            /*
             * Fully silent: power the DAC channel down instead of leaving it
             * actively driving a static voltage. Even a constant DAC output
             * picks up audible noise from the chip's own digital activity
             * (SPI to the display, CPU switching, etc.) via poor analog
             * isolation; disabling it between tones avoids that.
             * sidetone_key(true) re-enables it and wakes this back up.
             */
            dac_continuous_disable(s_dac_handle);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            dac_continuous_enable(s_dac_handle);
            continue;
        }

        for (uint32_t i = 0; i < SIDETONE_CHUNK_SAMPLES; i++) {
            float env = next_envelope_value();
            uint32_t incr = atomic_load_explicit(&s_phase_incr, memory_order_relaxed);
            phase += incr;
            int8_t sample = s_sine_table[phase >> (32 - SIDETONE_SINE_TABLE_BITS)];
            uint32_t vol_q16 = atomic_load_explicit(&s_volume_q16, memory_order_relaxed);
            float scaled = (float)sample * env * ((float)vol_q16 / 65536.0f);
            chunk[i] = (uint8_t)(128 + (int)lrintf(scaled));
        }
        dac_continuous_write(s_dac_handle, chunk, sizeof(chunk), NULL, portMAX_DELAY);
    }
}

void sidetone_init(void)
{
    generate_sine_table();
    generate_envelope_ramp((SIDETONE_SAMPLE_RATE_HZ * SIDETONE_ENVELOPE_MS_DEFAULT) / 1000u);

    atomic_store_explicit(&s_phase_incr, freq_to_phase_incr(SIDETONE_DEFAULT_HZ), memory_order_relaxed);
    atomic_store_explicit(&s_volume_q16, 65536u, memory_order_relaxed); /* full volume until sidetone_set_volume() runs */
    atomic_store_explicit(&s_key_down, false, memory_order_relaxed);

    const dac_continuous_config_t cfg = {
        .chan_mask = SIDETONE_DAC_CHANNEL_MASK,
        .desc_num = 8,
        .buf_size = 512,
        .freq_hz = SIDETONE_SAMPLE_RATE_HZ,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };
    ESP_ERROR_CHECK(dac_continuous_new_channels(&cfg, &s_dac_handle));
    ESP_ERROR_CHECK(dac_continuous_enable(s_dac_handle));

    xTaskCreate(audio_task, "sidetone_dac", 4096, NULL, 5, &s_audio_task_handle);
}

void sidetone_set_freq(uint16_t hz)
{
    atomic_store_explicit(&s_phase_incr, freq_to_phase_incr(hz), memory_order_relaxed);
}

void sidetone_set_envelope_ms(uint16_t ms)
{
    if (ms < SIDETONE_ENVELOPE_MS_MIN) {
        ms = SIDETONE_ENVELOPE_MS_MIN;
    } else if (ms > SIDETONE_ENVELOPE_MS_MAX) {
        ms = SIDETONE_ENVELOPE_MS_MAX;
    }
    generate_envelope_ramp((SIDETONE_SAMPLE_RATE_HZ * ms) / 1000u);
}

void sidetone_set_volume(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    atomic_store_explicit(&s_volume_q16, (uint32_t)percent * 65536u / 100u, memory_order_relaxed);
}

void sidetone_key(bool down)
{
    atomic_store_explicit(&s_key_down, down, memory_order_relaxed);
    if (down && s_audio_task_handle != NULL) {
        xTaskNotifyGive(s_audio_task_handle); /* wake audio_task() if it's blocked waiting out silence */
    }
}
