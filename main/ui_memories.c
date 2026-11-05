#include "ui_memories.h"

#include "display_init.h"
#include "fonts.h"
#include "memory_store.h"
#include "message_player.h"
#include "paddle_input.h"

#include <stdio.h>
#include <string.h>

#define REFRESH_PERIOD_MS 100
#define TILE_LABEL_CHARS 8   /* text shown on a tile whose memory has no name */
#define PICKER_TEXT_CHARS 24 /* start of a memory's text shown in a picker row */
#define STRIP_GLYPH_WIDTH_PX 12 /* lv_font_dejavu_mono_16_12 is fixed-advance */
#define STEP_BTN_SIZE 44
#define VALUE_LABEL_WIDTH 80
#define FOOTER_BTN_HEIGHT 44
#define NOTICE_MS 2000 /* how long a strip notice (e.g. "Queue full") shows */

typedef struct {
    lv_obj_t *btn;
    lv_obj_t *name_label;
    lv_obj_t *num_label;
    lv_obj_t *badge_label;
} tile_widgets_t;

/* A tile's repeat state for the current visit to the panel; see the
 * functional spec. Starts as a copy of its memory's defaults. */
typedef struct {
    bool valid;
    bool repeat;
    uint8_t count;
    uint8_t pause_s;
} session_repeat_t;

static lv_obj_t *s_screen;
static lv_obj_t *s_send_screen;
static tile_widgets_t s_tiles[MEMORY_TILE_COUNT];
static lv_obj_t *s_repeat_tile;
static lv_obj_t *s_repeat_label;
static lv_obj_t *s_strip_text;
static lv_obj_t *s_strip_progress;
static lv_obj_t *s_picker;

static session_repeat_t s_session[MEMORY_TILE_COUNT];
static int s_last_played_tile; /* 1-based; 0 = none this visit */
static uint32_t s_shown_generation;
static bool s_labels_valid;
static char s_highlight_hex[7];

/* A short notice shown in the sending strip in place of the message text,
 * for an action that failed. Empty = none. */
static const char *s_notice = "";
static uint32_t s_notice_start_ms;

/* Repeat popup: the tile it edits, and its value labels. */
static int s_popup_tile;
static lv_obj_t *s_popup_count_label;
static lv_obj_t *s_popup_pause_label;

static void set_text_if_changed(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void show_notice(const char *text)
{
    s_notice = text;
    s_notice_start_ms = lv_tick_get();
}

/* The notice to show now, or NULL once it has expired. */
static const char *active_notice(void)
{
    if (s_notice[0] != '\0' && lv_tick_elaps(s_notice_start_ms) >= NOTICE_MS) {
        s_notice = "";
    }
    return (s_notice[0] != '\0') ? s_notice : NULL;
}

static void session_from_memory(int tile, session_repeat_t *out)
{
    memory_t m;
    memory_store_get(memory_store_get_tile((uint8_t)tile), &m);
    *out = (session_repeat_t){ .valid = true, .repeat = m.repeat, .count = m.count, .pause_s = m.pause_s };
}

/* The tile the Repeat tile acts on: the one playing, else the one played
 * most recently in this visit; 0 if none, or while a message that isn't a
 * tile's plays (CLI `mem play` or `send`), so Repeat can't seem to act on
 * that message while really changing an earlier tile. */
static int current_tile(const message_player_status_t *st)
{
    if (st->active) {
        return (st->tag >= 1 && st->tag <= MEMORY_TILE_COUNT) ? st->tag : 0;
    }
    return s_last_played_tile;
}

static bool tile_is_playing(int tile, const message_player_status_t *st)
{
    return st->active && st->tag == tile;
}

static bool tile_is_queued(int tile, const message_player_status_t *st)
{
    for (uint8_t i = 0; i < st->queue_len; ++i) {
        if (st->queue_tags[i] == tile) {
            return true;
        }
    }
    return false;
}

/* The session state of tile, created on first use. For the tile that's
 * playing, it starts from what the player is actually using (e.g. after
 * coming back to the panel while a message kept playing). */
static session_repeat_t *ensure_session(int tile, const message_player_status_t *st)
{
    session_repeat_t *s = &s_session[tile - 1];
    if (!s->valid) {
        if (tile_is_playing(tile, st)) {
            *s = (session_repeat_t){ .valid = true, .repeat = st->repeat, .count = st->count, .pause_s = st->pause_s };
        } else {
            session_from_memory(tile, s);
        }
    }
    return s;
}

static void get_status(message_player_status_t *st)
{
    static char text[MESSAGE_PLAYER_TEXT_MAX + 1];
    paddle_input_message_get_status(st, text);
}

/* ---- Tile labels ---- */

static void relabel_tile(int tile)
{
    tile_widgets_t *w = &s_tiles[tile - 1];
    uint8_t n = memory_store_get_tile((uint8_t)tile);
    char name[MEMORY_TEXT_MAX + 1];
    char num[24];

    if (n == MEMORY_TILE_NONE) {
        set_text_if_changed(w->name_label, "(none)");
        set_text_if_changed(w->num_label, "");
        return;
    }

    memory_t m;
    memory_store_get(n, &m);
    if (m.text[0] == '\0') {
        set_text_if_changed(w->name_label, "");
        snprintf(num, sizeof(num), "#%02u (empty)", (unsigned)n);
    } else {
        strlcpy(name, m.name[0] != '\0' ? m.name : m.text, sizeof(name));
        name[TILE_LABEL_CHARS] = '\0';
        set_text_if_changed(w->name_label, name);
        snprintf(num, sizeof(num), "#%02u", (unsigned)n);
    }
    set_text_if_changed(w->num_label, num);
}

static void relabel_tiles(void)
{
    for (int t = 1; t <= MEMORY_TILE_COUNT; ++t) {
        relabel_tile(t);
    }
}

/* ---- Periodic refresh ---- */

static void refresh_tiles(const message_player_status_t *st)
{
    for (int t = 1; t <= MEMORY_TILE_COUNT; ++t) {
        tile_widgets_t *w = &s_tiles[t - 1];
        bool playing = tile_is_playing(t, st);
        if (playing) {
            lv_obj_add_state(w->btn, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(w->btn, LV_STATE_CHECKED);
        }

        char badge[4] = "";
        if (playing) {
            strlcpy(badge, LV_SYMBOL_PLAY, sizeof(badge));
        } else {
            for (uint8_t i = 0; i < st->queue_len; ++i) {
                if (st->queue_tags[i] == t) {
                    snprintf(badge, sizeof(badge), "%u", (unsigned)(i + 1));
                }
            }
        }
        set_text_if_changed(w->badge_label, badge);
    }
}

static void refresh_repeat_tile(const message_player_status_t *st)
{
    int tile = current_tile(st);
    if (tile == 0) {
        lv_obj_add_state(s_repeat_tile, LV_STATE_DISABLED);
        set_text_if_changed(s_repeat_label, "Repeat");
        return;
    }
    lv_obj_remove_state(s_repeat_tile, LV_STATE_DISABLED);
    bool on = tile_is_playing(tile, st) ? st->repeat : ensure_session(tile, st)->repeat;
    set_text_if_changed(s_repeat_label, on ? "Repeat: ON" : "Repeat: OFF");
}

/* The message text, windowed so the character being sent stays in view,
 * with that character recolored. */
static void refresh_strip_text(const message_player_status_t *st)
{
    size_t len = strlen(st->text);
    size_t visible = (size_t)(lv_obj_get_content_width(s_strip_text) / STRIP_GLYPH_WIDTH_PX);
    if (visible == 0) {
        visible = 1;
    }
    size_t start = (st->char_pos > visible / 3) ? st->char_pos - visible / 3 : 0;
    if (len > visible && start > len - visible) {
        start = len - visible;
    }
    if (len <= visible) {
        start = 0;
    }
    size_t end = (start + visible < len) ? start + visible : len;

    char buf[MESSAGE_PLAYER_TEXT_MAX + 16];
    if (st->char_len > 0 && st->char_pos >= start && st->char_pos + st->char_len <= end) {
        snprintf(buf, sizeof(buf), "%.*s#%s %.*s#%.*s", (int)(st->char_pos - start), st->text + start,
                 s_highlight_hex, (int)st->char_len, st->text + st->char_pos,
                 (int)(end - st->char_pos - st->char_len), st->text + st->char_pos + st->char_len);
    } else {
        snprintf(buf, sizeof(buf), "%.*s", (int)(end - start), st->text + start);
    }
    set_text_if_changed(s_strip_text, buf);
}

static void refresh_strip_progress(const message_player_status_t *st)
{
    char progress[32] = "";
    if (st->active && st->repeat) {
        if (st->count == 0) {
            snprintf(progress, sizeof(progress), "%u/" LV_SYMBOL_LOOP, (unsigned)st->send_number);
        } else {
            snprintf(progress, sizeof(progress), "%u/%u", (unsigned)st->send_number, (unsigned)st->count);
        }
        if (st->paused) {
            size_t n = strlen(progress);
            snprintf(progress + n, sizeof(progress) - n, " %us", (unsigned)((st->pause_remaining_ms + 999) / 1000));
        }
    }
    set_text_if_changed(s_strip_progress, progress);
}

static void refresh_strip(const message_player_status_t *st)
{
    const char *notice = active_notice();
    if (notice != NULL) {
        set_text_if_changed(s_strip_text, notice);
    } else if (st->active) {
        refresh_strip_text(st);
    } else {
        set_text_if_changed(s_strip_text, "");
    }
    refresh_strip_progress(st);
}

static void refresh(void)
{
    message_player_status_t st;
    get_status(&st);

    if (st.active && st.tag >= 1 && st.tag <= MEMORY_TILE_COUNT) {
        s_last_played_tile = st.tag;
    }
    uint32_t generation = memory_store_generation();
    if (!s_labels_valid || generation != s_shown_generation) {
        s_shown_generation = generation;
        s_labels_valid = true;
        relabel_tiles();
    }
    refresh_tiles(&st);
    refresh_repeat_tile(&st);
    refresh_strip(&st);
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (lv_screen_active() == s_screen) {
        refresh();
    }
}

/* A new visit: forget the previous visit's repeat states. */
static void screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    memset(s_session, 0, sizeof(s_session));
    s_last_played_tile = 0;
    s_notice = "";
    s_labels_valid = false;
    refresh();
}

/* ---- Memory tiles ---- */

static void tile_clicked_cb(lv_event_t *e)
{
    int tile = (int)(intptr_t)lv_event_get_user_data(e);
    uint8_t n = memory_store_get_tile((uint8_t)tile);
    if (n == MEMORY_TILE_NONE) {
        return;
    }
    memory_t m;
    memory_store_get(n, &m);
    if (m.text[0] == '\0') {
        return;
    }

    message_player_status_t st;
    get_status(&st);
    if (tile_is_playing(tile, &st)) {
        return;
    }
    session_repeat_t *s = ensure_session(tile, &st);
    message_player_message_t msg = {
        .repeat = s->repeat,
        .count = s->count,
        .pause_s = s->pause_s,
        .tag = tile,
    };
    /* The notice must outlive this call: show_notice() keeps the pointer. */
    static char s_unset_notice[MORSE_TEXT_VAR_NAME_MAX + 16];
    morse_text_result_t text_result;
    char missing[MORSE_TEXT_VAR_NAME_MAX + 1];
    message_player_enqueue_result_t result = paddle_input_message_enqueue(&msg, m.text, &text_result, missing);
    if (text_result == MORSE_TEXT_ERR_UNSET_VARIABLE) {
        snprintf(s_unset_notice, sizeof(s_unset_notice), "{%s} not set", missing);
        show_notice(s_unset_notice);
    } else if (text_result != MORSE_TEXT_OK) {
        show_notice("Too long");
    } else if (result == MESSAGE_PLAYER_QUEUE_FULL) {
        show_notice("Queue full");
    } else if (result == MESSAGE_PLAYER_INVALID) {
        show_notice("Can't send");
    }
    refresh();
}

static void close_picker(void)
{
    if (s_picker != NULL) {
        lv_obj_delete_async(s_picker);
        s_picker = NULL;
    }
}

static void picker_row_cb(lv_event_t *e)
{
    int tile = (int)(intptr_t)lv_obj_get_user_data(s_picker);
    uint8_t n = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (memory_store_set_tile((uint8_t)tile, n) == ESP_OK) {
        s_session[tile - 1].valid = false; /* a different memory now */
    } else {
        show_notice("Save failed");
    }
    close_picker();
    refresh();
}

static void picker_cancel_cb(lv_event_t *e)
{
    (void)e;
    close_picker();
}

/* Marks the row of the memory the tile has now: the list theme gives a
 * CHECKED button no look of its own. Amber, like a playing tile. */
static void style_picker_row(lv_obj_t *btn)
{
    static lv_style_t s_style;
    static bool s_style_inited = false;
    if (!s_style_inited) {
        lv_style_init(&s_style);
        lv_style_set_bg_color(&s_style, display_compensate_color(lv_palette_main(LV_PALETTE_AMBER)));
        lv_style_set_bg_opa(&s_style, LV_OPA_COVER);
        lv_style_set_text_color(&s_style, display_compensate_color(lv_color_black()));
        s_style_inited = true;
    }
    lv_obj_add_style(btn, &s_style, LV_STATE_CHECKED);
}

/* Full-screen list of all memories, laid over the panel (a child of its
 * screen, not a screen of its own, so the panel's visit - and its repeat
 * states - carry on). Built on open and deleted on close to keep its ~70
 * objects out of LVGL's small heap the rest of the time. */
static void open_picker(int tile)
{
    s_picker = lv_obj_create(s_screen);
    lv_obj_add_flag(s_picker, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(s_picker, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_picker, 0, 0);
    lv_obj_set_style_radius(s_picker, 0, 0);
    lv_obj_set_style_pad_all(s_picker, 4, 0);
    lv_obj_set_style_pad_row(s_picker, 4, 0);
    lv_obj_set_flex_flow(s_picker, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_user_data(s_picker, (void *)(intptr_t)tile);

    lv_obj_t *title = lv_label_create(s_picker);
    lv_label_set_text_fmt(title, "Memory for tile %d", tile);

    lv_obj_t *list = lv_list_create(s_picker);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);

    uint8_t assigned = memory_store_get_tile((uint8_t)tile);
    lv_obj_t *none_btn = lv_list_add_button(list, NULL, "None");
    lv_obj_add_event_cb(none_btn, picker_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)MEMORY_TILE_NONE);
    style_picker_row(none_btn);
    if (assigned == MEMORY_TILE_NONE) {
        lv_obj_add_state(none_btn, LV_STATE_CHECKED);
    }

    for (uint8_t n = 1; n <= MEMORY_COUNT; ++n) {
        memory_t m;
        memory_store_get(n, &m);
        char row[MEMORY_NAME_MAX + PICKER_TEXT_CHARS + 16];
        snprintf(row, sizeof(row), "#%02u  %-8s  %.*s", (unsigned)n, m.name, PICKER_TEXT_CHARS, m.text);
        lv_obj_t *btn = lv_list_add_button(list, NULL, row);
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_add_event_cb(btn, picker_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)n);
        style_picker_row(btn);
        if (n == assigned) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }
    }
    lv_obj_t *cancel = lv_button_create(s_picker);
    display_style_button_dismiss(cancel);
    lv_obj_set_size(cancel, LV_PCT(100), FOOTER_BTN_HEIGHT);
    lv_obj_add_event_cb(cancel, picker_cancel_cb, LV_EVENT_CLICKED, NULL);
    display_add_button_label(cancel, LV_SYMBOL_CLOSE " Cancel");

    /* Center the current row. The layout must be computed first (with
     * Cancel in place, as it sets the list's height): until then the rows
     * have no real positions and the scroll lands short of the row. */
    if (assigned != MEMORY_TILE_NONE) {
        lv_obj_update_layout(s_picker);
        lv_obj_t *row = lv_obj_get_child(list, assigned);
        int32_t y = lv_obj_get_y(row) - (lv_obj_get_content_height(list) - lv_obj_get_height(row)) / 2;
        int32_t max_y = lv_obj_get_scroll_bottom(list); /* scroll is still 0 here */
        lv_obj_scroll_to_y(list, y < 0 ? 0 : (y > max_y ? max_y : y), LV_ANIM_OFF);
    }
}

static void tile_long_pressed_cb(lv_event_t *e)
{
    int tile = (int)(intptr_t)lv_event_get_user_data(e);
    message_player_status_t st;
    get_status(&st);
    if (tile_is_playing(tile, &st) || tile_is_queued(tile, &st) || s_picker != NULL) {
        return;
    }
    open_picker(tile);
}

/* ---- Repeat tile and popup ---- */

static void apply_session_to_player(int tile, const session_repeat_t *s)
{
    message_player_status_t st;
    get_status(&st);
    if (tile_is_playing(tile, &st)) {
        paddle_input_message_set_repeat_params(s->count, s->pause_s);
        paddle_input_message_set_repeat(s->repeat);
    }
}

static void repeat_clicked_cb(lv_event_t *e)
{
    (void)e;
    message_player_status_t st;
    get_status(&st);
    int tile = current_tile(&st);
    if (tile == 0) {
        return;
    }
    session_repeat_t *s = ensure_session(tile, &st);
    s->repeat = !s->repeat;
    if (tile_is_playing(tile, &st)) {
        paddle_input_message_set_repeat(s->repeat);
    }
    refresh();
}

static void update_popup_labels(void)
{
    const session_repeat_t *s = &s_session[s_popup_tile - 1];
    if (s->count == 0) {
        lv_label_set_text(s_popup_count_label, "No limit");
    } else {
        lv_label_set_text_fmt(s_popup_count_label, "%u", (unsigned)s->count);
    }
    lv_label_set_text_fmt(s_popup_pause_label, "%u s", (unsigned)s->pause_s);
}

static void popup_step_cb(lv_event_t *e)
{
    int code = (int)(intptr_t)lv_event_get_user_data(e); /* +-1: count, +-2: pause */
    session_repeat_t *s = &s_session[s_popup_tile - 1];
    if (code == 1 || code == -1) {
        int v = (int)s->count + code;
        s->count = (uint8_t)(v < 0 ? 0 : v > MESSAGE_PLAYER_COUNT_MAX ? MESSAGE_PLAYER_COUNT_MAX : v);
    } else {
        int v = (int)s->pause_s + code / 2;
        s->pause_s = (uint8_t)(v < MESSAGE_PLAYER_PAUSE_MIN_S   ? MESSAGE_PLAYER_PAUSE_MIN_S
                               : v > MESSAGE_PLAYER_PAUSE_MAX_S ? MESSAGE_PLAYER_PAUSE_MAX_S
                                                                : v);
    }
    apply_session_to_player(s_popup_tile, s);
    update_popup_labels();
}

static void popup_defaults_cb(lv_event_t *e)
{
    (void)e;
    session_repeat_t *s = &s_session[s_popup_tile - 1];
    session_from_memory(s_popup_tile, s);
    apply_session_to_player(s_popup_tile, s);
    update_popup_labels();
    refresh();
}

static void popup_close_cb(lv_event_t *e)
{
    lv_msgbox_close((lv_obj_t *)lv_event_get_user_data(e));
}

static lv_obj_t *create_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data,
                               bool dismiss)
{
    lv_obj_t *btn = lv_button_create(parent);
    if (dismiss) {
        display_style_button_dismiss(btn);
    } else {
        display_style_tile(btn);
    }
    lv_obj_set_height(btn, FOOTER_BTN_HEIGHT);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    display_add_button_label(btn, text);
    return btn;
}

static lv_obj_t *create_stepper(lv_obj_t *parent, const char *name, int step_code)
{
    lv_obj_t *row = create_row(parent);
    lv_obj_t *name_label = lv_label_create(row);
    lv_label_set_text(name_label, name);
    lv_obj_set_width(name_label, 50);

    for (int sign = -1; sign <= 1; sign += 2) {
        lv_obj_t *btn = lv_button_create(row);
        display_style_tile(btn);
        lv_obj_set_size(btn, STEP_BTN_SIZE, STEP_BTN_SIZE);
        void *code = (void *)(intptr_t)(sign * step_code);
        lv_obj_add_event_cb(btn, popup_step_cb, LV_EVENT_CLICKED, code);
        lv_obj_add_event_cb(btn, popup_step_cb, LV_EVENT_LONG_PRESSED_REPEAT, code);
        display_add_button_label(btn, sign < 0 ? LV_SYMBOL_MINUS : LV_SYMBOL_PLUS);
        if (sign < 0) {
            lv_obj_t *value = lv_label_create(row);
            lv_obj_set_width(value, VALUE_LABEL_WIDTH);
            lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
            if (step_code == 1) {
                s_popup_count_label = value;
            } else {
                s_popup_pause_label = value;
            }
        }
    }
    return row;
}

static void repeat_long_pressed_cb(lv_event_t *e)
{
    (void)e;
    message_player_status_t st;
    get_status(&st);
    int tile = current_tile(&st);
    if (tile == 0) {
        return;
    }
    ensure_session(tile, &st);
    s_popup_tile = tile;

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    char title[32];
    snprintf(title, sizeof(title), "Repeat (tile %d)", tile);
    lv_msgbox_add_title(mbox, title);

    lv_obj_t *content = lv_msgbox_get_content(mbox);
    lv_obj_set_style_pad_all(content, 2, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    create_stepper(content, "Count", 1);
    create_stepper(content, "Pause", 2);

    lv_obj_t *actions = create_row(content);
    create_button(actions, "Memory defaults", popup_defaults_cb, NULL, false);
    create_button(actions, LV_SYMBOL_CLOSE " Close", popup_close_cb, mbox, true);
    update_popup_labels();
}

static void stop_clicked_cb(lv_event_t *e)
{
    (void)e;
    paddle_input_message_stop();
    refresh();
}

static void back_clicked_cb(lv_event_t *e)
{
    (void)e;
    close_picker();
    lv_scr_load(s_send_screen);
}

/* ---- Construction ---- */

static lv_obj_t *create_grid_tile(lv_obj_t *grid, int col, int row)
{
    lv_obj_t *tile = lv_button_create(grid);
    display_style_tile(tile);
    lv_obj_set_grid_cell(tile, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tile, 2, 0);
    lv_obj_set_style_pad_row(tile, 2, 0);
    return tile;
}

static void create_memory_tile(lv_obj_t *grid, int tile, int col, int row)
{
    tile_widgets_t *w = &s_tiles[tile - 1];
    w->btn = create_grid_tile(grid, col, row);
    /* SHORT_CLICKED, not CLICKED: LVGL also sends CLICKED on release after
     * a long press, which would play the tile right after opening the
     * picker. */
    lv_obj_add_event_cb(w->btn, tile_clicked_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)tile);
    lv_obj_add_event_cb(w->btn, tile_long_pressed_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)tile);
    lv_obj_set_style_bg_color(w->btn, display_compensate_color(lv_palette_main(LV_PALETTE_AMBER)),
                              LV_STATE_CHECKED);

    w->name_label = display_add_button_label(w->btn, "");
    lv_obj_set_style_text_font(w->name_label, &lv_font_dejavu_mono_16_12, 0);

    w->num_label = display_add_button_label(w->btn, "");
    lv_obj_set_style_text_font(w->num_label, &lv_font_montserrat_12, 0);

    w->badge_label = display_add_button_label(w->btn, "");
    lv_obj_add_flag(w->badge_label, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_text_font(w->badge_label, &lv_font_montserrat_12, 0);
    lv_obj_align(w->badge_label, LV_ALIGN_TOP_RIGHT, 0, 0);
}

lv_obj_t *ui_memories_create(lv_obj_t *send_screen)
{
    static const int32_t cols_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    s_send_screen = send_screen;
    lv_color_t hl = display_compensate_color(lv_palette_main(LV_PALETTE_AMBER));
    snprintf(s_highlight_hex, sizeof(s_highlight_hex), "%02X%02X%02X", hl.red, hl.green, hl.blue);

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(s_screen, 4, 0);
    lv_obj_set_style_pad_row(s_screen, 4, 0);
    lv_obj_set_flex_flow(s_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_screen, screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);

    lv_obj_t *grid = lv_obj_create(s_screen);
    lv_obj_remove_style_all(grid);
    lv_obj_set_style_pad_row(grid, 4, 0);
    lv_obj_set_style_pad_column(grid, 4, 0);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_grid_dsc_array(grid, cols_dsc, rows_dsc);

    for (int t = 1; t <= MEMORY_TILE_COUNT; ++t) {
        create_memory_tile(grid, t, (t - 1) % 3, (t - 1) / 3);
    }

    s_repeat_tile = create_grid_tile(grid, 0, 2);
    lv_obj_add_event_cb(s_repeat_tile, repeat_clicked_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(s_repeat_tile, repeat_long_pressed_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_set_style_opa(s_repeat_tile, LV_OPA_40, LV_STATE_DISABLED);
    s_repeat_label = display_add_button_label(s_repeat_tile, "Repeat");

    lv_obj_t *stop = create_grid_tile(grid, 1, 2);
    lv_obj_add_event_cb(stop, stop_clicked_cb, LV_EVENT_CLICKED, NULL);
    display_add_button_label(stop, LV_SYMBOL_STOP " Stop");

    lv_obj_t *back = create_grid_tile(grid, 2, 2);
    display_style_button_dismiss(back);
    lv_obj_add_event_cb(back, back_clicked_cb, LV_EVENT_CLICKED, NULL);
    display_add_button_label(back, LV_SYMBOL_LEFT " Back");

    lv_obj_t *strip = lv_obj_create(s_screen);
    lv_obj_remove_style_all(strip);
    lv_obj_set_size(strip, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(strip, 6, 0);

    s_strip_text = lv_label_create(strip);
    lv_obj_set_style_text_font(s_strip_text, &lv_font_dejavu_mono_16_12, 0);
    lv_label_set_recolor(s_strip_text, true);
    lv_label_set_long_mode(s_strip_text, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_flex_grow(s_strip_text, 1);
    lv_label_set_text(s_strip_text, "");

    s_strip_progress = lv_label_create(strip);
    lv_label_set_text(s_strip_progress, "");

    lv_timer_create(refresh_timer_cb, REFRESH_PERIOD_MS, NULL);
    return s_screen;
}
