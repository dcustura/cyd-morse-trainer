#include "sidetone.h"

#include "board_pins.h"
#include "driver/ledc.h"
#include "esp_err.h"

#define SIDETONE_TIMER      LEDC_TIMER_0
#define SIDETONE_CHANNEL    LEDC_CHANNEL_0
#define SIDETONE_MODE       LEDC_LOW_SPEED_MODE
#define SIDETONE_DUTY_RES   LEDC_TIMER_10_BIT
#define SIDETONE_DUTY_ON    (1u << 9) /* 50% duty at 10-bit resolution: a square wave */
#define SIDETONE_DUTY_OFF   0

void sidetone_init(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode = SIDETONE_MODE,
        .duty_resolution = SIDETONE_DUTY_RES,
        .timer_num = SIDETONE_TIMER,
        .freq_hz = SIDETONE_DEFAULT_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    const ledc_channel_config_t channel_cfg = {
        .gpio_num = BOARD_SPEAKER_GPIO,
        .speed_mode = SIDETONE_MODE,
        .channel = SIDETONE_CHANNEL,
        .timer_sel = SIDETONE_TIMER,
        .duty = SIDETONE_DUTY_OFF,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
}

void sidetone_set_freq(uint16_t hz)
{
    ledc_set_freq(SIDETONE_MODE, SIDETONE_TIMER, hz);
}

void sidetone_key(bool down)
{
    ledc_set_duty(SIDETONE_MODE, SIDETONE_CHANNEL, down ? SIDETONE_DUTY_ON : SIDETONE_DUTY_OFF);
    ledc_update_duty(SIDETONE_MODE, SIDETONE_CHANNEL);
}
