#include "macro_vars.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define NVS_NAMESPACE "morse_vars"
#define NVS_KEY_VARS "vars"
/* Stored as one blob: a format version, then each variable's name and
 * value, NUL-terminated, in name order. No variables: no key. */
#define BLOB_VERSION 1
#define BLOB_MAX (1 + MACRO_VARS_MAX * (MORSE_TEXT_VAR_NAME_MAX + 1 + MACRO_VAR_VALUE_MAX + 1))

static const char *TAG = "macro_vars";

static macro_var_t s_vars[MACRO_VARS_MAX]; /* sorted by name */
static size_t s_count;
static SemaphoreHandle_t s_lock;

static void lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void)
{
    xSemaphoreGive(s_lock);
}

/* True if name and value are what macro_vars_set() accepts, unchanged by
 * normalizing. */
static bool valid_pair(const char *name, const char *value)
{
    char norm_name[MORSE_TEXT_VAR_NAME_MAX + 1];
    char norm_value[MACRO_VAR_VALUE_MAX + 1];
    return morse_text_normalize_var_name(name, norm_name) && strcmp(norm_name, name) == 0
           && morse_text_normalize(value, MACRO_VAR_VALUE_FLAGS, norm_value, sizeof(norm_value), NULL)
                  == MORSE_TEXT_OK
           && norm_value[0] != '\0' && strcmp(norm_value, value) == 0;
}

/* Index of name in vars[0..count), or where it would be inserted. */
static size_t find(const macro_var_t *vars, size_t count, const char *name, bool *found)
{
    size_t i = 0;
    while (i < count && strcmp(vars[i].name, name) < 0) {
        ++i;
    }
    *found = (i < count && strcmp(vars[i].name, name) == 0);
    return i;
}

static size_t encode_blob(const macro_var_t *vars, size_t count, uint8_t blob[BLOB_MAX])
{
    size_t len = 0;
    blob[len++] = BLOB_VERSION;
    for (size_t i = 0; i < count; ++i) {
        size_t n = strlen(vars[i].name) + 1;
        memcpy(&blob[len], vars[i].name, n);
        len += n;
        n = strlen(vars[i].value) + 1;
        memcpy(&blob[len], vars[i].value, n);
        len += n;
    }
    return len;
}

/* Parse a stored blob into s_vars. Entries that are malformed, no longer
 * valid, duplicated or out of order are dropped. */
static void decode_blob(const uint8_t *blob, size_t len)
{
    s_count = 0;
    if (len < 1 || blob[0] != BLOB_VERSION) {
        return;
    }
    size_t pos = 1;
    while (pos < len && s_count < MACRO_VARS_MAX) {
        const char *name = (const char *)&blob[pos];
        size_t name_len = strnlen(name, len - pos);
        if (pos + name_len + 1 >= len) {
            return; /* name unterminated, or no value after it */
        }
        const char *value = name + name_len + 1;
        size_t value_room = len - (pos + name_len + 1);
        size_t value_len = strnlen(value, value_room);
        if (value_len == value_room) {
            return; /* value unterminated */
        }
        pos += name_len + 1 + value_len + 1;

        bool in_order = (s_count == 0 || strcmp(s_vars[s_count - 1].name, name) < 0);
        if (in_order && valid_pair(name, value)) {
            strcpy(s_vars[s_count].name, name);
            strcpy(s_vars[s_count].value, value);
            s_count++;
        }
    }
}

esp_err_t macro_vars_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        }
        return ESP_OK;
    }
    uint8_t blob[BLOB_MAX];
    size_t len = sizeof(blob);
    if (nvs_get_blob(handle, NVS_KEY_VARS, blob, &len) == ESP_OK) {
        decode_blob(blob, len);
    }
    nvs_close(handle);
    return ESP_OK;
}

size_t macro_vars_list(macro_var_t out[MACRO_VARS_MAX])
{
    lock();
    size_t count = s_count;
    memcpy(out, s_vars, count * sizeof(s_vars[0]));
    unlock();
    return count;
}

/* Persist vars (count entries), erasing the key when there are none. */
static esp_err_t save(const macro_var_t *vars, size_t count)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    if (count > 0) {
        uint8_t blob[BLOB_MAX];
        err = nvs_set_blob(handle, NVS_KEY_VARS, blob, encode_blob(vars, count, blob));
    } else {
        err = nvs_erase_key(handle, NVS_KEY_VARS);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

/* Save the edited copy and, only if that worked, make it current. */
static esp_err_t commit(const macro_var_t *vars, size_t count)
{
    esp_err_t err = save(vars, count);
    if (err == ESP_OK) {
        memcpy(s_vars, vars, count * sizeof(s_vars[0]));
        s_count = count;
    }
    return err;
}

esp_err_t macro_vars_set(const char *name, const char *value)
{
    if (!valid_pair(name, value)) {
        return ESP_ERR_INVALID_ARG;
    }
    macro_var_t vars[MACRO_VARS_MAX];
    lock();
    size_t count = s_count;
    memcpy(vars, s_vars, sizeof(vars));
    bool found;
    size_t i = find(vars, count, name, &found);
    esp_err_t err = ESP_OK;
    if (!found) {
        if (count == MACRO_VARS_MAX) {
            err = ESP_ERR_NO_MEM;
        } else {
            memmove(&vars[i + 1], &vars[i], (count - i) * sizeof(vars[0]));
            strcpy(vars[i].name, name);
            count++;
        }
    }
    if (err == ESP_OK) {
        strcpy(vars[i].value, value);
        err = commit(vars, count);
    }
    unlock();
    return err;
}

esp_err_t macro_vars_clear(const char *name)
{
    macro_var_t vars[MACRO_VARS_MAX];
    lock();
    size_t count = s_count;
    memcpy(vars, s_vars, sizeof(vars));
    bool found;
    size_t i = find(vars, count, name, &found);
    esp_err_t err = ESP_ERR_NOT_FOUND;
    if (found) {
        memmove(&vars[i], &vars[i + 1], (count - i - 1) * sizeof(vars[0]));
        err = commit(vars, count - 1);
    }
    unlock();
    return err;
}

esp_err_t macro_vars_reset(void)
{
    lock();
    esp_err_t err = commit(s_vars, 0);
    unlock();
    return err;
}

/* Called with the lock held, from morse_text_expand(). */
static const char *lookup(const char *name, void *ctx)
{
    (void)ctx;
    bool found;
    size_t i = find(s_vars, s_count, name, &found);
    return found ? s_vars[i].value : NULL;
}

morse_text_result_t macro_vars_expand(const char *text, char *out, size_t out_size,
                                      char missing[MORSE_TEXT_VAR_NAME_MAX + 1])
{
    lock();
    morse_text_result_t r = morse_text_expand(text, lookup, NULL, out, out_size, missing);
    unlock();
    return r;
}
