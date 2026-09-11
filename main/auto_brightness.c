#include "auto_brightness.h"

#include "board_pins.h"
#include "display_init.h"
#include "settings.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "auto_brightness";

/*
 * Confirmed on real hardware: on a stock ESP32-2432S028R, this reads a flat
 * raw=0 regardless of light, at every ADC attenuation setting - this is not
 * a firmware/wiring-assumption bug. The board's LDR divider (R15 pull-up to
 * 3.3V, R19 parallel to ground, both ~1M ohm on the original board
 * revision) is high-impedance enough that GPIO34 stays pinned near 0V. Only
 * a physical rework fixes it: remove R19 and replace R15 with ~100k, or add
 * a resistor in parallel with R15 (see
 * https://rntlab.com/question/ldr-on-esp32-2432s028r-is-not-working/).
 * Newer board batches (sticker date 2435/2442+) reportedly ship different
 * resistor values that may already work.
 */

/* GPIO34 is ADC1 channel 6 on the ESP32; keep in sync with BOARD_LDR_GPIO
 * (see the help text on MORSE_LDR_GPIO in Kconfig.projbuild). */
#define LDR_ADC_UNIT ADC_UNIT_1
#define LDR_ADC_CHANNEL ADC_CHANNEL_6
#define LDR_ADC_ATTEN ADC_ATTEN_DB_12

#define SAMPLE_PERIOD_US (1000 * 1000)
#define ADC_RAW_MAX 4095
/* Only push a new duty to the backlight when the mapped percentage moves by
 * at least this much, so ADC noise doesn't cause visible micro-flicker. */
#define CHANGE_THRESHOLD_PCT 3

static adc_oneshot_unit_handle_t s_adc_handle;
static esp_timer_handle_t s_sample_timer;
static uint8_t s_last_applied_pct;

/* Raw ADC value -> brightness percentage. Assumes more ambient light yields
 * a higher raw reading (brighter room -> brighter screen); flip this if a
 * given unit's LDR divider is wired the other way around. */
static uint8_t raw_to_brightness_pct(int raw)
{
    if (raw < 0) {
        raw = 0;
    }
    if (raw > ADC_RAW_MAX) {
        raw = ADC_RAW_MAX;
    }
    int span = SETTINGS_BRIGHTNESS_PCT_MAX - SETTINGS_BRIGHTNESS_PCT_MIN;
    int pct = SETTINGS_BRIGHTNESS_PCT_MIN + (raw * span) / ADC_RAW_MAX;
    return settings_clamp_brightness_pct((uint8_t)pct);
}

static void sample_timer_cb(void *arg)
{
    (void)arg;

    int raw = 0;
    esp_err_t err = adc_oneshot_read(s_adc_handle, LDR_ADC_CHANNEL, &raw);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_read failed: %s", esp_err_to_name(err));
        return;
    }

    uint8_t pct = raw_to_brightness_pct(raw);
    int delta = (int)pct - (int)s_last_applied_pct;
    if (delta < 0) {
        delta = -delta;
    }
    if (delta < CHANGE_THRESHOLD_PCT) {
        return;
    }

    ESP_LOGI(TAG, "raw=%d -> brightness=%u%%", raw, pct);
    display_set_brightness(pct);
    s_last_applied_pct = pct;
}

void auto_brightness_init(void)
{
    const adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = LDR_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc_handle));

    const adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = LDR_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, LDR_ADC_CHANNEL, &chan_cfg));

    const esp_timer_create_args_t timer_args = {
        .callback = sample_timer_cb,
        .name = "auto_brightness",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_sample_timer));
}

void auto_brightness_set_enabled(bool enabled)
{
    if (enabled) {
        s_last_applied_pct = 0; /* force the first sample to always apply */
        sample_timer_cb(NULL);
        ESP_ERROR_CHECK(esp_timer_start_periodic(s_sample_timer, SAMPLE_PERIOD_US));
    } else if (esp_timer_is_active(s_sample_timer)) {
        ESP_ERROR_CHECK(esp_timer_stop(s_sample_timer));
    }
}
