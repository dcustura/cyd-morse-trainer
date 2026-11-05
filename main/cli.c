#include "cli.h"

#include "auto_brightness.h"
#include "cli_mem.h"
#include "display_init.h"
#include "paddle_input.h"
#include "settings_store.h"
#include "sidetone.h"
#include "ui_send.h"
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
    KEY_AUTO_SPACE_PCT,
    KEY_TONE_HZ,
    KEY_VOLUME_PCT,
    KEY_ENVELOPE_MS,
    KEY_BRIGHTNESS_PCT,
    KEY_BRIGHTNESS_AUTO,
    KEY_SEND_LARGE_TEXT,
    KEY_STRAIGHT_RING_INPUT,
    KEY_ON_AIR,
    KEY_COUNT,
} cli_key_t;

static const char *const s_key_names[KEY_COUNT] = {
    [KEY_WPM] = "wpm",
    [KEY_KEYMODE] = "keymode",
    [KEY_PADDLE_SWAP] = "paddle_swap",
    [KEY_PADDLE_DEBOUNCE] = "paddle_debounce",
    [KEY_AUTO_SPACE_PCT] = "auto_space_pct",
    [KEY_TONE_HZ] = "tone_hz",
    [KEY_VOLUME_PCT] = "volume_pct",
    [KEY_ENVELOPE_MS] = "envelope_ms",
    [KEY_BRIGHTNESS_PCT] = "brightness_pct",
    [KEY_BRIGHTNESS_AUTO] = "brightness_auto",
    [KEY_SEND_LARGE_TEXT] = "send_large_text",
    [KEY_STRAIGHT_RING_INPUT] = "straight_ring_input",
    [KEY_ON_AIR] = "on_air",
};

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

static void print_value(const settings_t *s, cli_key_t key)
{
    switch (key) {
    case KEY_WPM:
        printf("wpm=%u\n", s->wpm);
        break;
    case KEY_KEYMODE:
        printf("keymode=%s\n", keymode_to_str(s->keymode));
        break;
    case KEY_PADDLE_SWAP:
        printf("paddle_swap=%s\n", s->paddle_swap ? "on" : "off");
        break;
    case KEY_PADDLE_DEBOUNCE:
        printf("paddle_debounce=%s\n", s->paddle_debounce ? "on" : "off");
        break;
    case KEY_AUTO_SPACE_PCT:
        printf("auto_space_pct=%u\n", s->auto_space_pct);
        break;
    case KEY_TONE_HZ:
        printf("tone_hz=%u\n", s->tone_hz);
        break;
    case KEY_VOLUME_PCT:
        printf("volume_pct=%u\n", s->volume_pct);
        break;
    case KEY_ENVELOPE_MS:
        printf("envelope_ms=%u\n", s->envelope_ms);
        break;
    case KEY_BRIGHTNESS_PCT:
        printf("brightness_pct=%u\n", s->brightness_pct);
        break;
    case KEY_BRIGHTNESS_AUTO:
        printf("brightness_auto=%s\n", s->brightness_auto ? "on" : "off");
        break;
    case KEY_SEND_LARGE_TEXT:
        printf("send_large_text=%s\n", s->send_large_text ? "on" : "off");
        break;
    case KEY_STRAIGHT_RING_INPUT:
        printf("straight_ring_input=%s\n", s->straight_ring_input ? "on" : "off");
        break;
    case KEY_ON_AIR:
        printf("on_air=%s\n", s->on_air ? "on" : "off");
        break;
    default:
        break;
    }
}

/* The Settings screen owns the live settings (every touchscreen change
 * lands there first), so each command starts from a fresh copy of them
 * rather than a CLI-private copy that the touchscreen never updates. */
static void snapshot_settings(settings_t *out)
{
    lvgl_port_lock(0);
    ui_settings_get(out);
    lvgl_port_unlock();
}

static int cmd_get(int argc, char **argv)
{
    if (argc != 2) {
        printf("ERR usage: get <key>|settings\n");
        return 1;
    }
    bool all = strcmp(argv[1], "settings") == 0;
    cli_key_t key = KEY_WPM;
    if (!all && !parse_key(argv[1], &key)) {
        printf("ERR unknown key %s\n", argv[1]);
        return 1;
    }

    settings_t s;
    snapshot_settings(&s);
    int first = all ? 0 : (int)key;
    int last = all ? KEY_COUNT - 1 : (int)key;
    for (int i = first; i <= last; i++) {
        print_value(&s, (cli_key_t)i);
    }
    printf("OK\n");
    return 0;
}

/* Valid range per numeric key; max == 0 marks a non-numeric key. */
static const struct {
    long min;
    long max;
} s_num_range[KEY_COUNT] = {
    [KEY_WPM] = {SETTINGS_WPM_MIN, SETTINGS_WPM_MAX},
    [KEY_AUTO_SPACE_PCT] = {SETTINGS_AUTO_SPACE_PCT_MIN, SETTINGS_AUTO_SPACE_PCT_MAX},
    [KEY_TONE_HZ] = {SETTINGS_TONE_HZ_MIN, SETTINGS_TONE_HZ_MAX},
    [KEY_VOLUME_PCT] = {SETTINGS_VOLUME_PCT_MIN, SETTINGS_VOLUME_PCT_MAX},
    [KEY_ENVELOPE_MS] = {SETTINGS_ENVELOPE_MS_MIN, SETTINGS_ENVELOPE_MS_MAX},
    [KEY_BRIGHTNESS_PCT] = {SETTINGS_BRIGHTNESS_PCT_MIN, SETTINGS_BRIGHTNESS_PCT_MAX},
};

/* Parses val, stores it in *s, and applies it live. Returns 0 on success,
 * 1 (after printing an ERR line) on a bad value. Called with the LVGL lock
 * held. */
static int apply_setting(settings_t *s, cli_key_t key, const char *val)
{
    long num = 0;
    bool flag = false;
    iambic_keyer_mode_t mode = s->keymode;

    if (s_num_range[key].max != 0) {
        if (!parse_long(val, &num)) {
            printf("ERR invalid value %s\n", val);
            return 1;
        }
        /* Clamp while still a long, before narrowing to the field's type,
         * so out-of-range input saturates instead of wrapping (e.g.
         * volume_pct 300 -> 100, not 44). */
        if (num < s_num_range[key].min) {
            num = s_num_range[key].min;
        } else if (num > s_num_range[key].max) {
            num = s_num_range[key].max;
        }
    } else if (key == KEY_KEYMODE ? !keymode_from_str(val, &mode) : !parse_bool(val, &flag)) {
        printf("ERR unknown value %s\n", val);
        return 1;
    }

    switch (key) {
    case KEY_WPM:
        s->wpm = (uint16_t)num;
        paddle_input_set_wpm(s->wpm);
        break;
    case KEY_KEYMODE:
        s->keymode = mode;
        paddle_input_set_mode(mode);
        break;
    case KEY_PADDLE_SWAP:
        s->paddle_swap = flag;
        paddle_input_set_swap(flag);
        break;
    case KEY_PADDLE_DEBOUNCE:
        s->paddle_debounce = flag;
        paddle_input_set_debounce(flag);
        break;
    case KEY_AUTO_SPACE_PCT:
        s->auto_space_pct = (uint8_t)num;
        paddle_input_set_auto_space_pct(s->auto_space_pct);
        break;
    case KEY_TONE_HZ:
        s->tone_hz = (uint16_t)num;
        sidetone_set_freq(s->tone_hz);
        break;
    case KEY_VOLUME_PCT:
        s->volume_pct = (uint8_t)num;
        sidetone_set_volume(s->volume_pct);
        break;
    case KEY_ENVELOPE_MS:
        s->envelope_ms = (uint16_t)num;
        sidetone_set_envelope_ms(s->envelope_ms);
        break;
    case KEY_BRIGHTNESS_PCT:
        s->brightness_pct = (uint8_t)num;
        if (s->brightness_auto) {
            /* An explicit level always overrides Auto, matching the touchscreen UI. */
            s->brightness_auto = false;
            auto_brightness_set_enabled(false);
        }
        display_set_brightness(s->brightness_pct);
        break;
    case KEY_BRIGHTNESS_AUTO:
        s->brightness_auto = flag;
        auto_brightness_set_enabled(flag);
        if (!flag) {
            display_set_brightness(s->brightness_pct);
        }
        break;
    case KEY_SEND_LARGE_TEXT:
        s->send_large_text = flag;
        ui_send_set_text_size(flag);
        break;
    case KEY_STRAIGHT_RING_INPUT:
        s->straight_ring_input = flag;
        paddle_input_set_straight_ring_input(flag);
        break;
    case KEY_ON_AIR:
        s->on_air = flag;
        paddle_input_set_on_air(flag);
        break;
    default:
        return 1;
    }
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

    /* Held across read-modify-write so a touchscreen edit can't land in
     * between and be overwritten. */
    lvgl_port_lock(0);
    settings_t s;
    ui_settings_get(&s);
    int rc = apply_setting(&s, key, argv[2]);
    if (rc == 0) {
        settings_store_save(&s);
        ui_settings_sync(&s);
    }
    lvgl_port_unlock();

    if (rc == 0) {
        print_value(&s, key);
        printf("OK\n");
    }
    return rc;
}

static int cmd_save(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    settings_t s;
    snapshot_settings(&s);
    esp_err_t err = settings_store_save(&s);
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

/* Live on-air state only, like the touchscreen's quick toggles: the saved
 * boot value (`set on_air`) is left alone. */
static int cmd_tx(int argc, char **argv)
{
    if (argc == 2) {
        bool on_air;
        if (!parse_bool(argv[1], &on_air)) {
            printf("ERR unknown value %s\n", argv[1]);
            return 1;
        }
        paddle_input_set_on_air(on_air);
    } else if (argc != 1) {
        printf("ERR usage: tx [on|off]\n");
        return 1;
    }
    printf("tx=%s\n", paddle_input_is_on_air() ? "on" : "off");
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

void cli_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "morse> ";
    repl_config.max_cmdline_args = CLI_MAX_ARGS;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    static const esp_console_cmd_t cmds[] = {
        {.command = "get", .help = "get <key>|settings - read one setting or all of them", .hint = NULL, .func = &cmd_get},
        {.command = "set", .help = "set <key> <value> - change and apply a setting", .hint = NULL, .func = &cmd_set},
        {.command = "save", .help = "save - persist current settings to flash", .hint = NULL, .func = &cmd_save},
        {.command = "reset", .help = "reset - erase saved settings, not memories (restart to apply defaults)", .hint = NULL, .func = &cmd_reset},
        {.command = "mem", .help = "mem list | get <N> | set <N> name|text|repeat|count|pause <value> | clear <N> | tile <T> <N|none> | tiles | play <N> | stop - message memories", .hint = NULL, .func = &cli_mem_cmd},
        {.command = "send", .help = "send <text> - key text once, or queue it behind a playing message (stop: mem stop)", .hint = NULL, .func = &cli_send_cmd},
        {.command = "tx", .help = "tx [on|off] - show or change the live on-air state (not saved; see set on_air)", .hint = NULL, .func = &cmd_tx},
        {.command = "log", .help = "log <none|error|warn|info|debug|verbose> - set log verbosity", .hint = NULL, .func = &cmd_log},
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
    ESP_ERROR_CHECK(esp_console_register_help_command());

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
