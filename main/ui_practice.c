#include "ui_practice.h"

#include "display_init.h"
#include "morse_codec.h"
#include "paddle_input.h"

#include <stdio.h>
#include <stdlib.h>

#define QUEUE_DRAIN_PERIOD_MS 30
#define TEXT_SPANS_HEIGHT_SLACK_PX 6

static lv_obj_t *s_wpm_label;
static lv_obj_t *s_mode_label;
static lv_obj_t *s_text_container;
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
    lv_label_set_text_fmt(s_wpm_label, "WPM: %u", (unsigned)paddle_input_get_wpm());
    lv_label_set_text_fmt(s_mode_label, "Mode: %s", mode_name(paddle_input_get_mode()));
}

static void screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    update_status_label();
}

/* s_text_container has a fixed, flex-allocated height and scrolls;
 * s_text_spans is sized to its text content. Scrolling to the bottom after
 * new text is appended keeps the most recently decoded characters in view
 * once the content overflows.
 *
 * s_text_spans' height is set explicitly here (LV_SPAN_MODE_BREAK's own
 * automatic sizing gives its *last* line zero margin below the glyphs -
 * see lv_spangroup_get_expand_height() summing each line's
 * font-height-plus-line-space, then subtracting one line-space to avoid a
 * trailing gap - so a decoration like the underline on a prosign span,
 * which renders a couple of pixels below that, is clipped for as long as
 * its line is the last one). Padding the height by a few pixels beyond
 * what lv_spangroup_get_expand_height() reports gives every line, including
 * whichever one is currently last, room for that overhang. */
static void scroll_text_to_bottom(void)
{
    lv_obj_update_layout(s_text_container);
    int32_t width = lv_obj_get_content_width(s_text_spans);
    int32_t natural_height = lv_spangroup_get_expand_height(s_text_spans, width);
    lv_obj_set_height(s_text_spans, natural_height + TEXT_SPANS_HEIGHT_SLACK_PX);
    lv_obj_update_layout(s_text_container);
    lv_obj_scroll_to_y(s_text_container, LV_COORD_MAX, LV_ANIM_OFF);
}

static void drain_queue_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    char ch;
    bool appended = false;
    while (xQueueReceive(s_decoded_char_queue, &ch, 0) == pdTRUE) {
        append_decoded_char(ch);
        appended = true;
    }

    if (appended) {
        scroll_text_to_bottom();
    }

    lv_opa_t dot_opa = paddle_input_is_keying() ? LV_OPA_COVER : LV_OPA_TRANSP;
    lv_obj_set_style_bg_opa(s_keying_dot, dot_opa, 0);
}

static void clear_btn_cb(lv_event_t *e)
{
    (void)e;
    while (lv_spangroup_get_span_count(s_text_spans) > 0) {
        lv_spangroup_delete_span(s_text_spans, lv_spangroup_get_child(s_text_spans, 0));
    }
    reset_plain_run();
    paddle_input_reset_decoder();
    lv_obj_scroll_to_y(s_text_container, 0, LV_ANIM_OFF);
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

    /* s_text_container has a fixed, flex-allocated height and scrolls;
     * the spangroup inside it is left to size itself to its text content
     * (LV_SPAN_MODE_BREAK) so growth beyond the container's height can be
     * scrolled into view instead of being clipped and lost. */
    s_text_container = lv_obj_create(scr);
    lv_obj_set_style_border_width(s_text_container, 0, 0);
    lv_obj_set_style_pad_all(s_text_container, 4, 0);
    lv_obj_set_width(s_text_container, LV_PCT(100));
    lv_obj_set_flex_grow(s_text_container, 1);
    lv_obj_set_scroll_dir(s_text_container, LV_DIR_VER);

    s_text_spans = lv_spangroup_create(s_text_container);
    lv_spangroup_set_mode(s_text_spans, LV_SPAN_MODE_BREAK);
    lv_spangroup_set_overflow(s_text_spans, LV_SPAN_OVERFLOW_CLIP);
    lv_obj_set_width(s_text_spans, LV_PCT(100));
    lv_obj_set_style_text_font(s_text_spans, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_line_space(s_text_spans, 8, 0);
    reset_plain_run();

    /* A plain divider line instead of a bordered pane around the button
     * row, so the text area above keeps as much screen height as possible. */
    lv_obj_t *separator = lv_obj_create(scr);
    lv_obj_remove_style_all(separator);
    lv_obj_clear_flag(separator, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(separator, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(separator, display_compensate_color(lv_palette_main(LV_PALETTE_GREY)), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, 0);

    lv_obj_t *btn_row = lv_obj_create(scr);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 4, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *clear_btn = lv_button_create(btn_row);
    display_style_tile(clear_btn);
    lv_obj_add_event_cb(clear_btn, clear_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");
    lv_obj_set_style_text_color(clear_label, display_compensate_color(lv_color_white()), 0);

    /* status_col (WPM/Mode) is centered in btn_row independent of Clear's
     * and right_group's widths, so it stays visually centered regardless of
     * label length. */
    lv_obj_t *status_col = lv_obj_create(btn_row);
    lv_obj_remove_style_all(status_col);
    lv_obj_set_flex_flow(status_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(status_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_flag(status_col, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(status_col, LV_ALIGN_CENTER, 0, 0);

    s_wpm_label = lv_label_create(status_col);
    lv_label_set_text_fmt(s_wpm_label, "WPM: %u", (unsigned)initial_wpm);

    s_mode_label = lv_label_create(status_col);
    lv_label_set_text_fmt(s_mode_label, "Mode: %s", mode_name(initial_mode));

    /* right_group (the keying dot, then Back) is right-anchored in btn_row
     * as a unit, with Back as its last child so Back itself stays flush
     * against the row's right edge - the dot sits just to Back's left. */
    lv_obj_t *right_group = lv_obj_create(btn_row);
    lv_obj_remove_style_all(right_group);
    lv_obj_set_flex_flow(right_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right_group, 8, 0);
    lv_obj_set_size(right_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_flag(right_group, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(right_group, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Layout must run once so clear_btn's height reflects its label/padding. */
    lv_obj_update_layout(btn_row);
    int32_t dot_diameter = lv_obj_get_height(clear_btn) / 2;

    s_keying_dot = lv_obj_create(right_group);
    lv_obj_remove_style_all(s_keying_dot);
    lv_obj_clear_flag(s_keying_dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_keying_dot, dot_diameter, dot_diameter);
    lv_obj_set_style_radius(s_keying_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_keying_dot, display_compensate_color(lv_palette_main(LV_PALETTE_RED)), 0);
    lv_obj_set_style_bg_opa(s_keying_dot, LV_OPA_TRANSP, 0);

    lv_obj_t *back_btn = lv_button_create(right_group);
    display_style_button_dismiss(back_btn);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, menu_screen);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_color(back_label, display_compensate_color(lv_color_white()), 0);

    lv_timer_create(drain_queue_timer_cb, QUEUE_DRAIN_PERIOD_MS, NULL);

    return scr;
}
