#include "memory_store.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "message_player.h"
#include "morse_text.h"
#include "nvs.h"

_Static_assert(MEMORY_TEXT_MAX == MESSAGE_PLAYER_TEXT_MAX, "a memory's text must fit the player");
_Static_assert(MEMORY_DEFAULT_PAUSE_S >= MESSAGE_PLAYER_PAUSE_MIN_S
                   && MEMORY_DEFAULT_PAUSE_S <= MESSAGE_PLAYER_PAUSE_MAX_S,
               "default pause out of range");

#define NVS_NAMESPACE "morse_mem"
#define NVS_KEY_TILES "tiles"
/* Memory n is stored under "m01".."m32" as a blob: a format version, the
 * repeat/count/pause bytes, then name and text, each NUL-terminated. An
 * empty default memory has no key at all. */
#define BLOB_VERSION 1
#define BLOB_HEADER 4
#define BLOB_MAX (BLOB_HEADER + MEMORY_NAME_MAX + 1 + MEMORY_TEXT_MAX + 1)

static const char *TAG = "memory_store";

static memory_t s_memories[MEMORY_COUNT];
static uint8_t s_tiles[MEMORY_TILE_COUNT];
static uint32_t s_generation;
static SemaphoreHandle_t s_lock;

static void lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void)
{
    xSemaphoreGive(s_lock);
}

static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static void memory_key(uint8_t n, char key[12])
{
    snprintf(key, 12, "m%02u", (unsigned)n);
}

void memory_store_default(memory_t *out)
{
    memset(out, 0, sizeof(*out));
    out->pause_s = MEMORY_DEFAULT_PAUSE_S;
}

static bool is_default(const memory_t *m)
{
    return m->name[0] == '\0' && m->text[0] == '\0' && !m->repeat && m->count == 0
           && m->pause_s == MEMORY_DEFAULT_PAUSE_S;
}

static void set_default_tiles(void)
{
    for (uint8_t t = 0; t < MEMORY_TILE_COUNT; ++t) {
        s_tiles[t] = t + 1;
    }
}

/* Parse a stored blob into *out. Anything malformed or no longer valid
 * (e.g. text the current rules reject) falls back to the default. */
static void decode_blob(const uint8_t *blob, size_t len, memory_t *out)
{
    memory_store_default(out);
    if (len < BLOB_HEADER + 2 || blob[0] != BLOB_VERSION) {
        return;
    }
    const char *name = (const char *)&blob[BLOB_HEADER];
    size_t name_len = strnlen(name, len - BLOB_HEADER);
    if (BLOB_HEADER + name_len + 1 >= len) {
        return;
    }
    const char *text = name + name_len + 1;
    size_t text_room = len - (BLOB_HEADER + name_len + 1);
    if (strnlen(text, text_room) == text_room) {
        return; /* not NUL-terminated */
    }

    memory_t m;
    memory_store_default(&m);
    if (morse_text_normalize(name, false, m.name, sizeof(m.name), NULL) != MORSE_TEXT_OK
        || morse_text_normalize(text, true, m.text, sizeof(m.text), NULL) != MORSE_TEXT_OK) {
        return;
    }
    m.repeat = blob[1] != 0;
    m.count = clamp_u8(blob[2], 0, MESSAGE_PLAYER_COUNT_MAX);
    m.pause_s = clamp_u8(blob[3], MESSAGE_PLAYER_PAUSE_MIN_S, MESSAGE_PLAYER_PAUSE_MAX_S);
    *out = m;
}

static size_t encode_blob(const memory_t *m, uint8_t blob[BLOB_MAX])
{
    blob[0] = BLOB_VERSION;
    blob[1] = m->repeat ? 1 : 0;
    blob[2] = m->count;
    blob[3] = m->pause_s;
    size_t len = BLOB_HEADER;
    size_t name_len = strlen(m->name) + 1;
    memcpy(&blob[len], m->name, name_len);
    len += name_len;
    size_t text_len = strlen(m->text) + 1;
    memcpy(&blob[len], m->text, text_len);
    return len + text_len;
}

static void load_all(void)
{
    for (uint8_t i = 0; i < MEMORY_COUNT; ++i) {
        memory_store_default(&s_memories[i]);
    }
    set_default_tiles();

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        }
        return;
    }

    for (uint8_t i = 0; i < MEMORY_COUNT; ++i) {
        char key[12];
        memory_key(i + 1, key);
        uint8_t blob[BLOB_MAX];
        size_t len = sizeof(blob);
        if (nvs_get_blob(handle, key, blob, &len) == ESP_OK) {
            decode_blob(blob, len, &s_memories[i]);
        }
    }

    uint8_t tiles[MEMORY_TILE_COUNT];
    size_t len = sizeof(tiles);
    if (nvs_get_blob(handle, NVS_KEY_TILES, tiles, &len) == ESP_OK && len == sizeof(tiles)) {
        for (uint8_t t = 0; t < MEMORY_TILE_COUNT; ++t) {
            s_tiles[t] = (tiles[t] <= MEMORY_COUNT) ? tiles[t] : MEMORY_TILE_NONE;
        }
    }
    nvs_close(handle);
}

esp_err_t memory_store_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    load_all();
    return ESP_OK;
}

void memory_store_get(uint8_t n, memory_t *out)
{
    if (n < 1 || n > MEMORY_COUNT) {
        memory_store_default(out);
        return;
    }
    lock();
    *out = s_memories[n - 1];
    unlock();
}

/* Write one key (or erase it, when blob is NULL) and commit. */
static esp_err_t write_key(const char *key, const void *blob, size_t len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    if (blob != NULL) {
        err = nvs_set_blob(handle, key, blob, len);
    } else {
        err = nvs_erase_key(handle, key);
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

esp_err_t memory_store_set(uint8_t n, const memory_t *in)
{
    if (n < 1 || n > MEMORY_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    memory_t m = *in;
    m.name[MEMORY_NAME_MAX] = '\0';
    m.text[MEMORY_TEXT_MAX] = '\0';
    m.count = clamp_u8(m.count, 0, MESSAGE_PLAYER_COUNT_MAX);
    m.pause_s = clamp_u8(m.pause_s, MESSAGE_PLAYER_PAUSE_MIN_S, MESSAGE_PLAYER_PAUSE_MAX_S);

    char key[12];
    memory_key(n, key);
    uint8_t blob[BLOB_MAX];
    size_t len = encode_blob(&m, blob);

    lock();
    esp_err_t err = write_key(key, is_default(&m) ? NULL : blob, len);
    if (err == ESP_OK) {
        s_memories[n - 1] = m;
        s_generation++;
    }
    unlock();
    return err;
}

uint8_t memory_store_get_tile(uint8_t t)
{
    if (t < 1 || t > MEMORY_TILE_COUNT) {
        return MEMORY_TILE_NONE;
    }
    lock();
    uint8_t n = s_tiles[t - 1];
    unlock();
    return n;
}

esp_err_t memory_store_set_tile(uint8_t t, uint8_t n)
{
    if (t < 1 || t > MEMORY_TILE_COUNT || n > MEMORY_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    lock();
    uint8_t tiles[MEMORY_TILE_COUNT];
    memcpy(tiles, s_tiles, sizeof(tiles));
    tiles[t - 1] = n;
    esp_err_t err = write_key(NVS_KEY_TILES, tiles, sizeof(tiles));
    if (err == ESP_OK) {
        memcpy(s_tiles, tiles, sizeof(tiles));
        s_generation++;
    }
    unlock();
    return err;
}

uint32_t memory_store_generation(void)
{
    lock();
    uint32_t g = s_generation;
    unlock();
    return g;
}

esp_err_t memory_store_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    lock();
    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    load_all();
    s_generation++;
    unlock();
    return err;
}
