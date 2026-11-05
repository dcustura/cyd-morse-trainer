#include "cli_mem.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
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

/* Normalize value into dst (a name or text buffer), printing ERR on failure. */
static bool normalize_field(const char *field, const char *value, bool allow_prosigns, char *dst, size_t dst_size)
{
    size_t pos = 0;
    morse_text_result_t r = morse_text_normalize(value, allow_prosigns, dst, dst_size, &pos);
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
        if (!normalize_field("name", value, false, m.name, sizeof(m.name))) {
            return 1;
        }
    } else if (strcmp(field, "text") == 0) {
        if (!normalize_field("text", value, true, m.text, sizeof(m.text))) {
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
    strlcpy(msg.text, m.text, sizeof(msg.text));

    char subject[16];
    snprintf(subject, sizeof(subject), "memory %u", (unsigned)n);
    return enqueue_and_report(&msg, "play", subject);
}

/* `send` messages get tags from here up, each one new, so repeated sends of
 * the same text all queue. Clear of tile tags (1..6) and CLI memory plays
 * (-1..-32). */
#define SEND_TAG_FIRST 1000

int cli_send_cmd(int argc, char **argv)
{
    static int s_next_tag = SEND_TAG_FIRST;

    if (argc < 2) {
        printf("ERR usage: send <text>\n");
        return 1;
    }
    if (argc >= CLI_MAX_ARGS - 1) {
        /* The console may have dropped words past this point. */
        printf("ERR too many words (max %d unquoted): quote the text\n", CLI_MAX_ARGS - 3);
        return 1;
    }
    /* Unquoted words arrive as separate arguments: join them with one
     * space, so `send CQ CQ DE X` works as well as `send "CQ CQ DE X"`. */
    char joined[256] = "";
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            strlcat(joined, " ", sizeof(joined));
        }
        if (strlcat(joined, argv[i], sizeof(joined)) >= sizeof(joined)) {
            printf("ERR text too long (max %u)\n", (unsigned)MESSAGE_PLAYER_TEXT_MAX);
            return 1;
        }
    }

    message_player_message_t msg = {.repeat = false};
    if (!normalize_field("text", joined, true, msg.text, sizeof(msg.text))) {
        return 1;
    }
    msg.tag = s_next_tag;

    int rc = enqueue_and_report(&msg, "send", "text");
    if (rc == 0) {
        s_next_tag = (s_next_tag == INT_MAX) ? SEND_TAG_FIRST : s_next_tag + 1;
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
