#include "cli.h"

#include "auto_brightness.h"
#include "display_init.h"
#include "paddle_input.h"
#include "settings_store.h"
#include "sidetone.h"
#include "ui_practice.h"
#include "ui_settings.h"

#include "esp_console.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    KEY_WPM,
    KEY_KEYMODE,
    KEY_PADDLE_SWAP,
    KEY_PADDLE_DEBOUNCE,
    KEY_TONE_HZ,
    KEY_VOLUME_PCT,
    KEY_ENVELOPE_MS,
    KEY_BRIGHTNESS_PCT,
    KEY_BRIGHTNESS_AUTO,
    KEY_PRACTICE_LARGE_TEXT,
    KEY_BLE_HID_ENABLED,
    KEY_COUNT,
} cli_key_t;

static const char *const s_key_names[KEY_COUNT] = {
    [KEY_WPM] = "wpm",
    [KEY_KEYMODE] = "keymode",
    [KEY_PADDLE_SWAP] = "paddle_swap",
    [KEY_PADDLE_DEBOUNCE] = "paddle_debounce",
    [KEY_TONE_HZ] = "tone_hz",
    [KEY_VOLUME_PCT] = "volume_pct",
    [KEY_ENVELOPE_MS] = "envelope_ms",
    [KEY_BRIGHTNESS_PCT] = "brightness_pct",
    [KEY_BRIGHTNESS_AUTO] = "brightness_auto",
    [KEY_PRACTICE_LARGE_TEXT] = "practice_large_text",
    [KEY_BLE_HID_ENABLED] = "ble_hid_enabled",
};

static settings_t s_settings;

static bool parse_key(const char *s, cli_key_t *out)
{
    for (int i = 0; i < KEY_COUNT; i++) {
        if (strcmp(s, s_key_names[i]) == 0) {
            *out = (cli_key_t)i;
            return true;
        }
    }
    return false;
}

static bool parse_long(const char *s, long *out)
{
    char *end;
    *out = strtol(s, &end, 10);
    return end != s && *end == '\0';
}

static bool parse_bool(const char *s, bool *out)
{
    if (strcmp(s, "on") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(s, "off") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static const char *keymode_to_str(iambic_keyer_mode_t mode)
{
    switch (mode) {
    case IAMBIC_KEYER_MODE_A:
        return "a";
    case IAMBIC_KEYER_MODE_B:
        return "b";
    case IAMBIC_KEYER_MODE_STRAIGHT:
        return "straight";
    case IAMBIC_KEYER_MODE_ULTIMATIC:
        return "ultimatic";
    default:
        return "?";
    }
}

static bool keymode_from_str(const char *s, iambic_keyer_mode_t *out)
{
    if (strcmp(s, "a") == 0) {
        *out = IAMBIC_KEYER_MODE_A;
    } else if (strcmp(s, "b") == 0) {
        *out = IAMBIC_KEYER_MODE_B;
    } else if (strcmp(s, "straight") == 0) {
        *out = IAMBIC_KEYER_MODE_STRAIGHT;
    } else if (strcmp(s, "ultimatic") == 0) {
        *out = IAMBIC_KEYER_MODE_ULTIMATIC;
    } else {
        return false;
    }
    return true;
}

static void print_value(cli_key_t key)
{
    switch (key) {
    case KEY_WPM:
        printf("wpm=%u\n", s_settings.wpm);
        break;
    case KEY_KEYMODE:
        printf("keymode=%s\n", keymode_to_str(s_settings.keymode));
        break;
    case KEY_PADDLE_SWAP:
        printf("paddle_swap=%s\n", s_settings.paddle_swap ? "on" : "off");
        break;
    case KEY_PADDLE_DEBOUNCE:
        printf("paddle_debounce=%s\n", s_settings.paddle_debounce ? "on" : "off");
        break;
    case KEY_TONE_HZ:
        printf("tone_hz=%u\n", s_settings.tone_hz);
        break;
    case KEY_VOLUME_PCT:
        printf("volume_pct=%u\n", s_settings.volume_pct);
        break;
    case KEY_ENVELOPE_MS:
        printf("envelope_ms=%u\n", s_settings.envelope_ms);
        break;
    case KEY_BRIGHTNESS_PCT:
        printf("brightness_pct=%u\n", s_settings.brightness_pct);
        break;
    case KEY_BRIGHTNESS_AUTO:
        printf("brightness_auto=%s\n", s_settings.brightness_auto ? "on" : "off");
        break;
    case KEY_PRACTICE_LARGE_TEXT:
        printf("practice_large_text=%s\n", s_settings.practice_large_text ? "on" : "off");
        break;
    case KEY_BLE_HID_ENABLED:
        printf("ble_hid_enabled=%s\n", s_settings.ble_hid_enabled ? "on" : "off");
        break;
    default:
        break;
    }
}

static int cmd_get(int argc, char **argv)
{
    if (argc != 2) {
        printf("ERR usage: get <key>|settings\n");
        return 1;
    }
    if (strcmp(argv[1], "settings") == 0) {
        for (int i = 0; i < KEY_COUNT; i++) {
            print_value((cli_key_t)i);
        }
        printf("OK\n");
        return 0;
    }
    cli_key_t key;
    if (!parse_key(argv[1], &key)) {
        printf("ERR unknown key %s\n", argv[1]);
        return 1;
    }
    print_value(key);
    printf("OK\n");
    return 0;
}

static int cmd_set(int argc, char **argv)
{
    if (argc != 3) {
        printf("ERR usage: set <key> <value>\n");
        return 1;
    }
    cli_key_t key;
    if (!parse_key(argv[1], &key)) {
        printf("ERR unknown key %s\n", argv[1]);
        return 1;
    }
    const char *val = argv[2];
    long num;
    bool flag;

    switch (key) {
    case KEY_WPM:
        if (!parse_long(val, &num)) {
            printf("ERR invalid value %s\n", val);
            return 1;
        }
        s_settings.wpm = settings_clamp_wpm((uint16_t)num);
        paddle_input_set_wpm(s_settings.wpm);
        break;
    case KEY_KEYMODE:
        if (!keymode_from_str(val, &s_settings.keymode)) {
            printf("ERR unknown value %s\n", val);
            return 1;
        }
        paddle_input_set_mode(s_settings.keymode);
        break;
    case KEY_PADDLE_SWAP:
        if (!parse_bool(val, &flag)) {
            printf("ERR unknown value %s\n", val);
            return 1;
        }
        s_settings.paddle_swap = flag;
        paddle_input_set_swap(flag);
        break;
    case KEY_PADDLE_DEBOUNCE:
        if (!parse_bool(val, &flag)) {
            printf("ERR unknown value %s\n", val);
            return 1;
        }
        s_settings.paddle_debounce = flag;
        paddle_input_set_debounce(flag);
        break;
    case KEY_TONE_HZ:
        if (!parse_long(val, &num)) {
            printf("ERR invalid value %s\n", val);
            return 1;
        }
        s_settings.tone_hz = settings_clamp_tone_hz((uint16_t)num);
        sidetone_set_freq(s_settings.tone_hz);
        break;
    case KEY_VOLUME_PCT:
        if (!parse_long(val, &num)) {
            printf("ERR invalid value %s\n", val);
            return 1;
        }
        s_settings.volume_pct = settings_clamp_volume_pct((uint8_t)num);
        sidetone_set_volume(s_settings.volume_pct);
        break;
    case KEY_ENVELOPE_MS:
        if (!parse_long(val, &num)) {
            printf("ERR invalid value %s\n", val);
            return 1;
        }
        s_settings.envelope_ms = settings_clamp_envelope_ms((uint16_t)num);
        sidetone_set_envelope_ms(s_settings.envelope_ms);
        break;
    case KEY_BRIGHTNESS_PCT:
        if (!parse_long(val, &num)) {
            printf("ERR invalid value %s\n", val);
            return 1;
        }
        s_settings.brightness_pct = settings_clamp_brightness_pct((uint8_t)num);
        if (s_settings.brightness_auto) {
            /* An explicit level always overrides Auto, matching the touchscreen UI. */
            s_settings.brightness_auto = false;
            auto_brightness_set_enabled(false);
        }
        display_set_brightness(s_settings.brightness_pct);
        break;
    case KEY_BRIGHTNESS_AUTO:
        if (!parse_bool(val, &flag)) {
            printf("ERR unknown value %s\n", val);
            return 1;
        }
        s_settings.brightness_auto = flag;
        auto_brightness_set_enabled(flag);
        if (!flag) {
            display_set_brightness(s_settings.brightness_pct);
        }
        break;
    case KEY_PRACTICE_LARGE_TEXT:
        if (!parse_bool(val, &flag)) {
            printf("ERR unknown value %s\n", val);
            return 1;
        }
        s_settings.practice_large_text = flag;
        lvgl_port_lock(0);
        ui_practice_set_text_size(flag);
        lvgl_port_unlock();
        break;
    case KEY_BLE_HID_ENABLED:
        if (!parse_bool(val, &flag)) {
            printf("ERR unknown value %s\n", val);
            return 1;
        }
        s_settings.ble_hid_enabled = flag;
        printf("note=restart required to apply\n");
        break;
    default:
        return 1;
    }

    settings_store_save(&s_settings);
    lvgl_port_lock(0);
    ui_settings_sync(&s_settings);
    lvgl_port_unlock();

    print_value(key);
    printf("OK\n");
    return 0;
}

static int cmd_save(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t err = settings_store_save(&s_settings);
    if (err != ESP_OK) {
        printf("ERR save failed (%s)\n", esp_err_to_name(err));
        return 1;
    }
    printf("OK\n");
    return 0;
}

static int cmd_reset(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t err = settings_store_reset();
    if (err != ESP_OK) {
        printf("ERR reset failed (%s)\n", esp_err_to_name(err));
        return 1;
    }
    printf("note=restart required to apply defaults\n");
    printf("OK\n");
    return 0;
}

static int cmd_log(int argc, char **argv)
{
    static const struct {
        const char *name;
        esp_log_level_t level;
    } levels[] = {
        {"none", ESP_LOG_NONE}, {"error", ESP_LOG_ERROR}, {"warn", ESP_LOG_WARN},
        {"info", ESP_LOG_INFO}, {"debug", ESP_LOG_DEBUG}, {"verbose", ESP_LOG_VERBOSE},
    };
    if (argc != 2) {
        printf("ERR usage: log <none|error|warn|info|debug|verbose>\n");
        return 1;
    }
    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
        if (strcmp(argv[1], levels[i].name) == 0) {
            esp_log_level_set("*", levels[i].level);
            printf("OK\n");
            return 0;
        }
    }
    printf("ERR unknown level %s\n", argv[1]);
    return 1;
}

void cli_start(const settings_t *initial_settings)
{
    s_settings = *initial_settings;

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "morse> ";

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    static const esp_console_cmd_t cmds[] = {
        {.command = "get", .help = "get <key>|settings - read one setting or all of them", .hint = NULL, .func = &cmd_get},
        {.command = "set", .help = "set <key> <value> - change and apply a setting", .hint = NULL, .func = &cmd_set},
        {.command = "save", .help = "save - persist current settings to flash", .hint = NULL, .func = &cmd_save},
        {.command = "reset", .help = "reset - erase saved settings (restart to apply defaults)", .hint = NULL, .func = &cmd_reset},
        {.command = "log", .help = "log <none|error|warn|info|debug|verbose> - set log verbosity", .hint = NULL, .func = &cmd_log},
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
    ESP_ERROR_CHECK(esp_console_register_help_command());

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
