#include "settings_store.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NAMESPACE "morse_cfg"
#define NVS_KEY_WPM "wpm"
#define NVS_KEY_KEYMODE "keymode"
#define NVS_KEY_SWAP "swap"
#define NVS_KEY_TONE_HZ "tone_hz"
#define NVS_KEY_VOLUME_PCT "volume_pct"
#define NVS_KEY_ENVELOPE_MS "envelope_ms"
#define NVS_KEY_BRIGHTNESS_PCT "brightness_pct"

static const char *TAG = "settings_store";

static esp_err_t load_from_nvs(settings_t *out)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "no saved settings, using defaults");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t raw_wpm;
    if (nvs_get_u16(handle, NVS_KEY_WPM, &raw_wpm) == ESP_OK) {
        out->wpm = settings_clamp_wpm(raw_wpm);
    }

    uint8_t raw_keymode;
    if (nvs_get_u8(handle, NVS_KEY_KEYMODE, &raw_keymode) == ESP_OK) {
        out->keymode = settings_validate_keymode(raw_keymode);
    }

    uint8_t raw_swap;
    if (nvs_get_u8(handle, NVS_KEY_SWAP, &raw_swap) == ESP_OK) {
        out->paddle_swap = (raw_swap != 0);
    }

    uint16_t raw_tone_hz;
    if (nvs_get_u16(handle, NVS_KEY_TONE_HZ, &raw_tone_hz) == ESP_OK) {
        out->tone_hz = settings_clamp_tone_hz(raw_tone_hz);
    }

    uint8_t raw_volume_pct;
    if (nvs_get_u8(handle, NVS_KEY_VOLUME_PCT, &raw_volume_pct) == ESP_OK) {
        out->volume_pct = settings_clamp_volume_pct(raw_volume_pct);
    }

    uint16_t raw_envelope_ms;
    if (nvs_get_u16(handle, NVS_KEY_ENVELOPE_MS, &raw_envelope_ms) == ESP_OK) {
        out->envelope_ms = settings_clamp_envelope_ms(raw_envelope_ms);
    }

    uint8_t raw_brightness_pct;
    if (nvs_get_u8(handle, NVS_KEY_BRIGHTNESS_PCT, &raw_brightness_pct) == ESP_OK) {
        out->brightness_pct = settings_clamp_brightness_pct(raw_brightness_pct);
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t settings_store_init_and_load(settings_t *out)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    settings_set_defaults(out);
    return load_from_nvs(out);
}

esp_err_t settings_store_save(const settings_t *in)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_u16(handle, NVS_KEY_WPM, in->wpm);
    nvs_set_u8(handle, NVS_KEY_KEYMODE, (uint8_t)in->keymode);
    nvs_set_u8(handle, NVS_KEY_SWAP, in->paddle_swap ? 1 : 0);
    nvs_set_u16(handle, NVS_KEY_TONE_HZ, in->tone_hz);
    nvs_set_u8(handle, NVS_KEY_VOLUME_PCT, in->volume_pct);
    nvs_set_u16(handle, NVS_KEY_ENVELOPE_MS, in->envelope_ms);
    nvs_set_u8(handle, NVS_KEY_BRIGHTNESS_PCT, in->brightness_pct);

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t settings_store_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    nvs_erase_all(handle);
    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
