#include "ui_practice.h"

#include "display_init.h"
#include "morse_codec.h"
#include "paddle_input.h"

#include <stdio.h>
#include <stdlib.h>

#define QUEUE_DRAIN_PERIOD_MS 30

static lv_obj_t *s_status_label;
static lv_obj_t *s_text_spans;
static lv_obj_t *s_keying_dot;
static QueueHandle_t s_decoded_char_queue;

/* The trailing run of plain (letter/digit/punctuation/space) characters is
 * accumulated into one growing span rather than one span per character, to
 * avoid piling up an unbounded number of span objects during a long
 * session. A prosign or unknown-sequence placeholder gets its own
 * specially-styled span and ends the current run; the next plain character
 * starts a new one. */
static lv_span_t *s_plain_span;
static char *s_plain_buf;
static size_t s_plain_len;
static size_t s_plain_cap;

static void reset_plain_run(void)
{
    s_plain_span = NULL;
    s_plain_len = 0;
}

static void append_plain_char(char ch)
{
    if (s_plain_span == NULL) {
        s_plain_span = lv_spangroup_add_span(s_text_spans);
        s_plain_len = 0;
    }

    if (s_plain_len + 1 >= s_plain_cap) {
        size_t new_cap = (s_plain_cap == 0) ? 16 : s_plain_cap * 2;
        char *grown = realloc(s_plain_buf, new_cap);
        if (grown == NULL) {
            return;
        }
        s_plain_buf = grown;
        s_plain_cap = new_cap;
    }

    s_plain_buf[s_plain_len++] = ch;
    s_plain_buf[s_plain_len] = '\0';
    lv_spangroup_set_span_text(s_text_spans, s_plain_span, s_plain_buf);
}

/* Adds a standalone styled span (a prosign abbreviation or the
 * unknown-sequence placeholder) and ends the current plain run so the next
 * plain character starts a fresh span rather than continuing this one. */
static void append_special_span(const char *text, bool underline, bool recolor, lv_color_t color)
{
    lv_span_t *span = lv_spangroup_add_span(s_text_spans);
    lv_style_t *style = lv_span_get_style(span);
    if (underline) {
        lv_style_set_text_decor(style, LV_TEXT_DECOR_UNDERLINE);
    }
    if (recolor) {
        lv_style_set_text_color(style, color);
    }
    lv_spangroup_set_span_text(s_text_spans, span, text);
    reset_plain_run();
}

static void append_decoded_char(char ch)
{
    if (ch == MORSE_CODEC_UNKNOWN_CHAR) {
        char text[2] = { ch, '\0' };
        append_special_span(text, false, true, display_compensate_color(lv_palette_main(LV_PALETTE_RED)));
        return;
    }

    const char *prosign_name = morse_codec_prosign_name(ch);
    if (prosign_name != NULL) {
        append_special_span(prosign_name, true, false, lv_color_black());
        return;
    }

    append_plain_char(ch);
}

static const char *mode_name(iambic_keyer_mode_t mode)
{
    switch (mode) {
    case IAMBIC_KEYER_MODE_A:
        return "Iambic A";
    case IAMBIC_KEYER_MODE_B:
        return "Iambic B";
    case IAMBIC_KEYER_MODE_STRAIGHT:
        return "Straight Key";
    default:
        return "?";
    }
}

static void update_status_label(void)
{
    char status_text[48];
    snprintf(status_text, sizeof(status_text), "WPM: %u   Mode: %s",
             (unsigned)paddle_input_get_wpm(), mode_name(paddle_input_get_mode()));
    lv_label_set_text(s_status_label, status_text);
}

static void screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    update_status_label();
}

static void drain_queue_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    char ch;
    while (xQueueReceive(s_decoded_char_queue, &ch, 0) == pdTRUE) {
        append_decoded_char(ch);
    }

    if (paddle_input_is_keying()) {
        lv_obj_clear_flag(s_keying_dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_keying_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

static void clear_btn_cb(lv_event_t *e)
{
    (void)e;
    while (lv_spangroup_get_span_count(s_text_spans) > 0) {
        lv_spangroup_delete_span(s_text_spans, lv_spangroup_get_child(s_text_spans, 0));
    }
    reset_plain_run();
    paddle_input_reset_decoder();
}

static void back_btn_cb(lv_event_t *e)
{
    lv_obj_t *menu_screen = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(menu_screen);
}

lv_obj_t *ui_practice_create(QueueHandle_t decoded_char_queue, lv_obj_t *menu_screen,
                              iambic_keyer_mode_t initial_mode, uint16_t initial_wpm)
{
    s_decoded_char_queue = decoded_char_queue;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_event_cb(scr, screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

    char status_text[48];
    snprintf(status_text, sizeof(status_text), "WPM: %u   Mode: %s",
             (unsigned)initial_wpm, mode_name(initial_mode));
    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, status_text);

    s_text_spans = lv_spangroup_create(scr);
    lv_spangroup_set_mode(s_text_spans, LV_SPAN_MODE_BREAK);
    lv_spangroup_set_overflow(s_text_spans, LV_SPAN_OVERFLOW_CLIP);
    lv_obj_set_width(s_text_spans, LV_PCT(100));
    lv_obj_set_flex_grow(s_text_spans, 1);
    reset_plain_run();

    lv_obj_t *btn_row = lv_obj_create(scr);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *clear_btn = lv_button_create(btn_row);
    display_style_button_teal(clear_btn);
    lv_obj_add_event_cb(clear_btn, clear_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");

    lv_obj_t *back_btn = lv_button_create(btn_row);
    display_style_button_teal(back_btn);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, menu_screen);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");

    /* Layout must run once so clear_btn's height reflects its label/padding. */
    lv_obj_update_layout(btn_row);
    int32_t dot_diameter = lv_obj_get_height(clear_btn) / 2;

    s_keying_dot = lv_obj_create(btn_row);
    lv_obj_remove_style_all(s_keying_dot);
    lv_obj_clear_flag(s_keying_dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_keying_dot, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(s_keying_dot, dot_diameter, dot_diameter);
    lv_obj_set_style_radius(s_keying_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_keying_dot, display_compensate_color(lv_palette_main(LV_PALETTE_RED)), 0);
    lv_obj_set_style_bg_opa(s_keying_dot, LV_OPA_COVER, 0);
    lv_obj_align(s_keying_dot, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_timer_create(drain_queue_timer_cb, QUEUE_DRAIN_PERIOD_MS, NULL);

    return scr;
}
