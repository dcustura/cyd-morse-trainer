#include "touch_cal_store.h"

#include "esp_log.h"
#include "nvs.h"

#define NVS_NAMESPACE "touch_cal"
#define NVS_KEY_HORIZ_MIN "horiz_min"
#define NVS_KEY_HORIZ_MAX "horiz_max"
#define NVS_KEY_VERT_MIN "vert_min"
#define NVS_KEY_VERT_MAX "vert_max"

static const char *TAG = "touch_cal_store";

esp_err_t touch_cal_store_load(touch_calibration_t *out, bool *found)
{
    *found = false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "no stored touch calibration");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    touch_calibration_t loaded;
    bool complete = nvs_get_u16(handle, NVS_KEY_HORIZ_MIN, &loaded.horiz_min) == ESP_OK
                    && nvs_get_u16(handle, NVS_KEY_HORIZ_MAX, &loaded.horiz_max) == ESP_OK
                    && nvs_get_u16(handle, NVS_KEY_VERT_MIN, &loaded.vert_min) == ESP_OK
                    && nvs_get_u16(handle, NVS_KEY_VERT_MAX, &loaded.vert_max) == ESP_OK;

    nvs_close(handle);

    if (!complete) {
        ESP_LOGW(TAG, "stored touch calibration is incomplete, ignoring");
        return ESP_OK;
    }

    *out = loaded;
    *found = true;
    return ESP_OK;
}

esp_err_t touch_cal_store_save(const touch_calibration_t *in)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_u16(handle, NVS_KEY_HORIZ_MIN, in->horiz_min);
    nvs_set_u16(handle, NVS_KEY_HORIZ_MAX, in->horiz_max);
    nvs_set_u16(handle, NVS_KEY_VERT_MIN, in->vert_min);
    nvs_set_u16(handle, NVS_KEY_VERT_MAX, in->vert_max);

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
