#include "cli_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "macro_vars.h"
#include "memory_store.h"
#include "message_player.h"
#include "paddle_input.h"
#include "morse_text.h"

#define USAGE                                                                                                     \
    "ERR usage: mem list | get <N> | set <N> name|text|repeat|count|pause <value> | clear <N> | tile <T> <N|none> " \
    "| tiles | play <N> | stop\n"

static bool parse_uint_in(const char *s, long lo, long hi, long *out)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < lo || v > hi) {
        return false;
    }
    *out = v;
    return true;
}

static bool parse_memory_number(const char *s, uint8_t *out)
{
    long v;
    if (!parse_uint_in(s, 1, MEMORY_COUNT, &v)) {
        printf("ERR memory number must be 1-%d\n", MEMORY_COUNT);
        return false;
    }
    *out = (uint8_t)v;
    return true;
}

/* Print s in double quotes, escaping '"' so the output can be pasted back
 * into "mem set" (backslash isn't an allowed character, so needs none). */
static void print_quoted(const char *s)
{
    putchar('"');
    for (; *s != '\0'; ++s) {
        if (*s == '"') {
            putchar('\\');
        }
        putchar(*s);
    }
    putchar('"');
}

static void print_memory(uint8_t n, const memory_t *m)
{
    printf("%02u name=", (unsigned)n);
    print_quoted(m->name);
    printf(" text=");
    print_quoted(m->text);
    printf(" repeat=%s count=%u pause=%u\n", m->repeat ? "on" : "off", m->count, m->pause_s);
}

static void print_tile(uint8_t t)
{
    uint8_t n = memory_store_get_tile(t);
    if (n == MEMORY_TILE_NONE) {
        printf("tile%u=none\n", (unsigned)t);
    } else {
        printf("tile%u=%02u\n", (unsigned)t, (unsigned)n);
    }
}

static bool print_save_error(esp_err_t err)
{
    if (err != ESP_OK) {
        printf("ERR save failed (%s)\n", esp_err_to_name(err));
        return true;
    }
    return false;
}

static int mem_list(void)
{
    for (uint8_t n = 1; n <= MEMORY_COUNT; ++n) {
        memory_t m;
        memory_store_get(n, &m);
        if (m.name[0] != '\0' || m.text[0] != '\0') {
            print_memory(n, &m);
        }
    }
    printf("OK\n");
    return 0;
}

static int mem_get(uint8_t n)
{
    memory_t m;
    memory_store_get(n, &m);
    print_memory(n, &m);
    printf("OK\n");
    return 0;
}

/* Normalize value into dst (a name or text buffer) with morse_text_normalize()
 * flags, printing ERR on failure. */
static bool normalize_field(const char *field, const char *value, unsigned flags, char *dst, size_t dst_size)
{
    size_t pos = 0;
    morse_text_result_t r = morse_text_normalize(value, flags, dst, dst_size, &pos);
    if (r == MORSE_TEXT_OK) {
        return true;
    }
    if (r == MORSE_TEXT_ERR_TOO_LONG) {
        printf("ERR %s too long (max %u)\n", field, (unsigned)(dst_size - 1));
    } else {
        printf("ERR %s: %s at position %u\n", field, morse_text_result_str(r), (unsigned)pos + 1);
    }
    return false;
}

static int mem_set(uint8_t n, const char *field, const char *value)
{
    memory_t m;
    memory_store_get(n, &m);
    long num;
    if (strcmp(field, "name") == 0) {
        if (!normalize_field("name", value, 0, m.name, sizeof(m.name))) {
            return 1;
        }
    } else if (strcmp(field, "text") == 0) {
        if (!normalize_field("text", value, MEMORY_TEXT_FLAGS, m.text, sizeof(m.text))) {
            return 1;
        }
    } else if (strcmp(field, "repeat") == 0) {
        if (strcmp(value, "on") == 0) {
            m.repeat = true;
        } else if (strcmp(value, "off") == 0) {
            m.repeat = false;
        } else {
            printf("ERR repeat must be on or off\n");
            return 1;
        }
    } else if (strcmp(field, "count") == 0) {
        if (!parse_uint_in(value, 0, MESSAGE_PLAYER_COUNT_MAX, &num)) {
            printf("ERR count must be 0-%d\n", MESSAGE_PLAYER_COUNT_MAX);
            return 1;
        }
        m.count = (uint8_t)num;
    } else if (strcmp(field, "pause") == 0) {
        if (!parse_uint_in(value, MESSAGE_PLAYER_PAUSE_MIN_S, MESSAGE_PLAYER_PAUSE_MAX_S, &num)) {
            printf("ERR pause must be %d-%d\n", MESSAGE_PLAYER_PAUSE_MIN_S, MESSAGE_PLAYER_PAUSE_MAX_S);
            return 1;
        }
        m.pause_s = (uint8_t)num;
    } else {
        printf("ERR unknown field %s\n", field);
        return 1;
    }

    if (print_save_error(memory_store_set(n, &m))) {
        return 1;
    }
    return mem_get(n);
}

static int mem_clear(uint8_t n)
{
    memory_t m;
    memory_store_default(&m);
    if (print_save_error(memory_store_set(n, &m))) {
        return 1;
    }
    return mem_get(n);
}

static int mem_tile(const char *tile_arg, const char *memory_arg)
{
    long t;
    if (!parse_uint_in(tile_arg, 1, MEMORY_TILE_COUNT, &t)) {
        printf("ERR tile number must be 1-%d\n", MEMORY_TILE_COUNT);
        return 1;
    }
    uint8_t n = MEMORY_TILE_NONE;
    if (strcmp(memory_arg, "none") != 0 && !parse_memory_number(memory_arg, &n)) {
        return 1;
    }
    if (print_save_error(memory_store_set_tile((uint8_t)t, n))) {
        return 1;
    }
    print_tile((uint8_t)t);
    printf("OK\n");
    return 0;
}

static int mem_tiles(void)
{
    for (uint8_t t = 1; t <= MEMORY_TILE_COUNT; ++t) {
        print_tile(t);
    }
    printf("OK\n");
    return 0;
}

/* Enqueue msg and print the outcome: `<verb>=started|queued` then OK, or
 * an ERR line naming subject (e.g. "memory 3"). Returns the command's exit
 * code. */
static int enqueue_and_report(const message_player_message_t *msg, const char *verb, const char *subject)
{
    switch (paddle_input_message_enqueue(msg)) {
    case MESSAGE_PLAYER_STARTED:
        printf("%s=started\n", verb);
        break;
    case MESSAGE_PLAYER_QUEUED:
        printf("%s=queued\n", verb);
        break;
    case MESSAGE_PLAYER_IGNORED_PLAYING:
        printf("ERR %s is already playing\n", subject);
        return 1;
    case MESSAGE_PLAYER_IGNORED_QUEUED:
        printf("ERR %s is already queued\n", subject);
        return 1;
    case MESSAGE_PLAYER_QUEUE_FULL:
        printf("ERR queue full\n");
        return 1;
    case MESSAGE_PLAYER_INVALID:
        printf("ERR %s is empty\n", subject);
        return 1;
    }
    printf("OK\n");
    return 0;
}

/* Fill text's {VAR}s into out (MESSAGE_PLAYER_TEXT_MAX + 1 chars), printing
 * ERR on failure. */
static bool expand_or_report(const char *text, char out[MESSAGE_PLAYER_TEXT_MAX + 1])
{
    char missing[MORSE_TEXT_VAR_NAME_MAX + 1];
    morse_text_result_t r = macro_vars_expand(text, out, MESSAGE_PLAYER_TEXT_MAX + 1, missing);
    if (r == MORSE_TEXT_OK) {
        return true;
    }
    if (r == MORSE_TEXT_ERR_UNSET_VARIABLE) {
        printf("ERR variable {%s} is not set (var set %s <value>)\n", missing, missing);
    } else if (r == MORSE_TEXT_ERR_TOO_LONG) {
        printf("ERR text too long once variables are filled in (max %u)\n", (unsigned)MESSAGE_PLAYER_TEXT_MAX);
    } else {
        printf("ERR text: %s\n", morse_text_result_str(r));
    }
    return false;
}

/* Plays (or queues) memory n with its own repeat defaults. CLI plays are
 * tagged -n so they never collide with a panel tile's tag (1..6). */
static int mem_play(uint8_t n)
{
    memory_t m;
    memory_store_get(n, &m);
    message_player_message_t msg = {
        .repeat = m.repeat,
        .count = m.count,
        .pause_s = m.pause_s,
        .tag = -(int)n,
    };
    if (!expand_or_report(m.text, msg.text)) {
        return 1;
    }

    char subject[16];
    snprintf(subject, sizeof(subject), "memory %u", (unsigned)n);
    return enqueue_and_report(&msg, "play", subject);
}

/* `send` messages get tags from here up, each one new, so repeated sends of
 * the same text all queue. Clear of tile tags (1..6) and CLI memory plays
 * (-1..-32). */
#define SEND_TAG_FIRST 1000

/* Append ch to buf (holding *len chars, room for size - 1), skipping a
 * space at the start or after another space. False if it doesn't fit. */
static bool append_collapsing_spaces(char *buf, size_t size, size_t *len, char ch)
{
    if (ch == ' ' && (*len == 0 || buf[*len - 1] == ' ')) {
        return true;
    }
    if (*len + 1 >= size) {
        return false;
    }
    buf[(*len)++] = ch;
    return true;
}

/* Join argv[first..argc) into buf with single spaces, so unquoted words work
 * as well as one quoted argument. Runs of spaces (inside quotes too) collapse
 * as they're copied, the way normalizing would, so only text that really is
 * too long overflows. Prints ERR (naming max as the limit) and returns false
 * if it doesn't fit, or if the console may have dropped words. */
static bool join_args(int argc, char **argv, int first, char *buf, size_t size, unsigned max)
{
    if (argc >= CLI_MAX_ARGS - 1) {
        /* The console may have dropped words past this point. */
        printf("ERR too many words (max %d unquoted): quote the text\n", CLI_MAX_ARGS - 1 - first - 1);
        return false;
    }
    size_t len = 0;
    bool fits = true;
    for (int i = first; i < argc && fits; ++i) {
        for (const char *c = argv[i]; *c != '\0' && fits; ++c) {
            fits = append_collapsing_spaces(buf, size, &len, *c);
        }
        fits = fits && append_collapsing_spaces(buf, size, &len, ' ');
    }
    if (!fits) {
        printf("ERR text too long (max %u)\n", max);
        return false;
    }
    buf[len] = '\0';
    return true;
}

int cli_send_cmd(int argc, char **argv)
{
    static int s_next_tag = SEND_TAG_FIRST;

    if (argc < 2) {
        printf("ERR usage: send <text>\n");
        return 1;
    }
    char joined[256];
    if (!join_args(argc, argv, 1, joined, sizeof(joined), MESSAGE_PLAYER_TEXT_MAX)) {
        return 1;
    }

    char text[MESSAGE_PLAYER_TEXT_MAX + 1];
    message_player_message_t msg = {.repeat = false};
    if (!normalize_field("text", joined, MEMORY_TEXT_FLAGS, text, sizeof(text)) || !expand_or_report(text, msg.text)) {
        return 1;
    }
    msg.tag = s_next_tag;

    int rc = enqueue_and_report(&msg, "send", "text");
    if (rc == 0) {
        s_next_tag++; /* ~2^31 sends to overflow: not a concern */
    }
    return rc;
}

static int mem_stop(void)
{
    paddle_input_message_stop();
    printf("OK\n");
    return 0;
}

int cli_mem_cmd(int argc, char **argv)
{
    const char *sub = (argc >= 2) ? argv[1] : "";
    uint8_t n;

    if (strcmp(sub, "list") == 0 && argc == 2) {
        return mem_list();
    }
    if (strcmp(sub, "tiles") == 0 && argc == 2) {
        return mem_tiles();
    }
    if (strcmp(sub, "get") == 0 && argc == 3) {
        return parse_memory_number(argv[2], &n) ? mem_get(n) : 1;
    }
    if (strcmp(sub, "clear") == 0 && argc == 3) {
        return parse_memory_number(argv[2], &n) ? mem_clear(n) : 1;
    }
    if (strcmp(sub, "set") == 0 && argc == 5) {
        return parse_memory_number(argv[2], &n) ? mem_set(n, argv[3], argv[4]) : 1;
    }
    if (strcmp(sub, "play") == 0 && argc == 3) {
        return parse_memory_number(argv[2], &n) ? mem_play(n) : 1;
    }
    if (strcmp(sub, "stop") == 0 && argc == 2) {
        return mem_stop();
    }
    if (strcmp(sub, "tile") == 0 && argc == 4) {
        return mem_tile(argv[2], argv[3]);
    }
    printf(USAGE);
    return 1;
}

#define VAR_USAGE "ERR usage: var list | set <NAME> <value> | clear <NAME>\n"

static bool parse_var_name(const char *s, char name[MORSE_TEXT_VAR_NAME_MAX + 1])
{
    if (!morse_text_normalize_var_name(s, name)) {
        printf("ERR variable name must be 1-%d letters/digits\n", MORSE_TEXT_VAR_NAME_MAX);
        return false;
    }
    return true;
}

static void print_var(const macro_var_t *v)
{
    printf("%s=", v->name);
    print_quoted(v->value);
    putchar('\n');
}

static int var_list(void)
{
    macro_var_t vars[MACRO_VARS_MAX];
    size_t count = macro_vars_list(vars);
    for (size_t i = 0; i < count; ++i) {
        print_var(&vars[i]);
    }
    printf("OK\n");
    return 0;
}

static int var_set(int argc, char **argv)
{
    macro_var_t v;
    if (!parse_var_name(argv[2], v.name)) {
        return 1;
    }
    char joined[64];
    if (!join_args(argc, argv, 3, joined, sizeof(joined), MACRO_VAR_VALUE_MAX)
        || !normalize_field("value", joined, MACRO_VAR_VALUE_FLAGS, v.value, sizeof(v.value))) {
        return 1;
    }
    if (v.value[0] == '\0') {
        printf("ERR value is empty (to delete: var clear %s)\n", v.name);
        return 1;
    }
    esp_err_t err = macro_vars_set(v.name, v.value);
    if (err == ESP_ERR_NO_MEM) {
        printf("ERR all %d variables in use (var clear one first)\n", MACRO_VARS_MAX);
        return 1;
    }
    if (print_save_error(err)) {
        return 1;
    }
    print_var(&v);
    printf("OK\n");
    return 0;
}

static int var_clear(const char *arg)
{
    char name[MORSE_TEXT_VAR_NAME_MAX + 1];
    if (!parse_var_name(arg, name)) {
        return 1;
    }
    esp_err_t err = macro_vars_clear(name);
    if (err == ESP_ERR_NOT_FOUND) {
        printf("ERR variable %s is not set\n", name);
        return 1;
    }
    if (print_save_error(err)) {
        return 1;
    }
    printf("OK\n");
    return 0;
}

int cli_var_cmd(int argc, char **argv)
{
    const char *sub = (argc >= 2) ? argv[1] : "";
    if (strcmp(sub, "list") == 0 && argc == 2) {
        return var_list();
    }
    if (strcmp(sub, "set") == 0 && argc >= 4) {
        return var_set(argc, argv);
    }
    if (strcmp(sub, "clear") == 0 && argc == 3) {
        return var_clear(argv[2]);
    }
    printf(VAR_USAGE);
    return 1;
}
