#include "ui_practice.h"

#include "display_init.h"
#include "fonts.h"
#include "morse_codec.h"
#include "paddle_input.h"
#include "ui_help.h"
#include "ui_settings.h"

#include <stdio.h>
#include <stdlib.h>

#define QUEUE_DRAIN_PERIOD_MS 30
#define TEXT_SPANS_HEIGHT_SLACK_PX 8
#define TEXT_LINE_SPACE_SMALL_PX 8
#define TEXT_LINE_SPACE_LARGE_PX 4

static lv_obj_t *s_wpm_label;
static lv_obj_t *s_mode_label;
static lv_obj_t *s_text_container;
static lv_obj_t *s_text_spans;
static lv_obj_t *s_keying_dot;
static lv_obj_t *s_help_screen;
static lv_obj_t *s_keyer_settings_screen;
static QueueHandle_t s_decoded_char_queue;

/* A unit HH can erase: either a run of plain (letter/digit/punctuation/
 * space) characters, erasable one word at a time via its own growing
 * buffer, or a single unknown-sequence "*" marker, erasable as one whole
 * unit. Letters/digits/spaces keep extending the topmost plain unit rather
 * than getting one span per character, to avoid piling up an unbounded
 * number of span objects during a long session; an unknown marker always
 * gets pushed as its own unit, so the next plain character starts a fresh
 * one on top of it.
 *
 * A real prosign badge (a decodable SK/VE/CT, or the forced line break after
 * BT/AR/SK) is a hard boundary HH must never cross: appending one clears the
 * whole stack below instead of pushing onto it (its own span stays visible,
 * just untracked from here on), so repeated HH always stops there rather
 * than reaching into an already-finished transmission. */
typedef struct {
    lv_span_t *span;
    bool is_marker;
    char *buf; /* unused (NULL) when is_marker */
    size_t len;
    size_t cap;
} erase_unit_t;

static erase_unit_t *s_erase_stack;
static size_t s_erase_count;
static size_t s_erase_cap;

static void clear_erase_stack(void)
{
    for (size_t i = 0; i < s_erase_count; i++) {
        free(s_erase_stack[i].buf);
    }
    s_erase_count = 0;
}

static erase_unit_t *push_erase_unit(void)
{
    if (s_erase_count + 1 > s_erase_cap) {
        size_t new_cap = (s_erase_cap == 0) ? 8 : s_erase_cap * 2;
        erase_unit_t *grown = realloc(s_erase_stack, new_cap * sizeof(*grown));
        if (grown == NULL) {
            return NULL;
        }
        s_erase_stack = grown;
        s_erase_cap = new_cap;
    }
    return &s_erase_stack[s_erase_count++];
}

/* The plain unit to extend: the topmost stack entry if it already is one,
 * otherwise a freshly pushed one (e.g. the stack is empty, or the last thing
 * pushed was an unknown marker or got cleared by a badge/line break). */
static erase_unit_t *current_plain_unit(void)
{
    if (s_erase_count > 0 && !s_erase_stack[s_erase_count - 1].is_marker) {
        return &s_erase_stack[s_erase_count - 1];
    }

    erase_unit_t *unit = push_erase_unit();
    if (unit == NULL) {
        return NULL;
    }
    *unit = (erase_unit_t){ .span = lv_spangroup_add_span(s_text_spans) };
    return unit;
}

static void append_plain_char(char ch)
{
    erase_unit_t *unit = current_plain_unit();
    if (unit == NULL) {
        return;
    }

    if (unit->len + 1 >= unit->cap) {
        size_t new_cap = (unit->cap == 0) ? 16 : unit->cap * 2;
        char *grown = realloc(unit->buf, new_cap);
        if (grown == NULL) {
            return;
        }
        unit->buf = grown;
        unit->cap = new_cap;
    }

    unit->buf[unit->len++] = ch;
    unit->buf[unit->len] = '\0';
    lv_spangroup_set_span_text(s_text_spans, unit->span, unit->buf);
}

static lv_span_t *add_styled_span(const char *text, lv_color_t color)
{
    lv_span_t *span = lv_spangroup_add_span(s_text_spans);
    lv_style_t *style = lv_span_get_style(span);
    lv_style_set_text_color(style, color);
    lv_spangroup_set_span_text(s_text_spans, span, text);
    return span;
}

static void append_unknown_marker(void)
{
    char text[2] = { MORSE_CODEC_UNKNOWN_CHAR, '\0' };
    lv_span_t *span = add_styled_span(text, display_compensate_color(lv_palette_main(LV_PALETTE_RED)));

    erase_unit_t *unit = push_erase_unit();
    if (unit == NULL) {
        return;
    }
    *unit = (erase_unit_t){ .span = span, .is_marker = true };
}

static void append_prosign_badge(const char *name)
{
    char text[8];
    snprintf(text, sizeof(text), "/%s", name);
    add_styled_span(text, display_compensate_color(lv_palette_main(LV_PALETTE_GREEN)));
    clear_erase_stack();
}

/* Whether the top of the erase stack is currently sitting at a word
 * boundary: empty (nothing pushed yet, or the stack was just cleared by a
 * badge/line break/Clear), a marker, or a plain run already ending in a
 * space. A word-gap SPACE decoded in this state would add nothing but a
 * stray/redundant blank, so append_decoded_char() drops it instead of
 * appending it - see there. This also matters for HH: the codec emits a
 * word-gap SPACE after essentially any character, HH included, once the
 * following pause is long enough, and without this check that space would
 * get appended right after an HH-triggered erase, so the *next* HH would
 * just strip that stray space back off instead of removing more of the run -
 * repeated HH would appear to stall after one press. */
static bool at_word_boundary(void)
{
    if (s_erase_count == 0 || s_erase_stack[s_erase_count - 1].is_marker) {
        return true;
    }
    const erase_unit_t *unit = &s_erase_stack[s_erase_count - 1];
    return unit->len == 0 || unit->buf[unit->len - 1] == ' ' || unit->buf[unit->len - 1] == '\n';
}

/* Embeds a literal newline in the growing plain run; lv_spangroup forces a
 * line break there. '\n' isn't in lv_font_unscii_8's glyph range (32-127),
 * so it draws with zero width - no stray glyph, unlike padding with spaces
 * would produce (and spaces run the risk of being silently eaten by
 * lv_spangroup's leading-space-collapsing on the next auto-wrapped line, so
 * padding can't reliably produce a guaranteed blank line either).
 *
 * Clears the erase stack right after inserting the newline, the same as a
 * real prosign badge, so a forced line break is a hard boundary HH can never
 * erase back across - without this, the trim in erase_last_word() would
 * treat '\n' as just another non-space character and happily eat through it
 * into whatever preceded the break. Clearing the stack also means
 * at_word_boundary() is automatically true right after, so a word gap the
 * operator happens to key next doesn't show up as a stray leading blank on
 * the new line. */
static void force_line_break(void)
{
    append_plain_char('\n');
    clear_erase_stack();
}

/* HH ("error, back up") erases the top of the erase stack one unit at a
 * time, matching its traditional meaning as a correction signal: each HH
 * either trims the last word off the topmost plain run (deleting the run
 * entirely once it's fully drained) or, if the top is an unknown-sequence
 * marker, deletes that marker outright. Repeated HH therefore walks back
 * through however many words and garbled markers were sent since the last
 * badge/line break/Clear, one unit per press, like repeated word-backspace -
 * it stops only once the stack is empty, which happens right after a real
 * prosign badge or a forced line break (with nothing typed since) or at the
 * very start of the text; HH never reaches back past those. */
static void erase_last_word(void)
{
    if (s_erase_count == 0) {
        return;
    }
    erase_unit_t *top = &s_erase_stack[s_erase_count - 1];

    if (top->is_marker) {
        lv_spangroup_delete_span(s_text_spans, top->span);
        s_erase_count--;
        return;
    }

    size_t new_len = top->len;
    if (new_len > 0 && top->buf[new_len - 1] == ' ') {
        new_len--;
    }
    while (new_len > 0 && top->buf[new_len - 1] != ' ') {
        new_len--;
    }
    top->len = new_len;

    if (top->len == 0) {
        /* Fully drained: drop this unit entirely so the next HH reaches
         * whatever precedes it instead of re-trimming nothing forever. */
        lv_spangroup_delete_span(s_text_spans, top->span);
        free(top->buf);
        s_erase_count--;
        return;
    }

    top->buf[top->len] = '\0';
    lv_spangroup_set_span_text(s_text_spans, top->span, top->buf);
}

static void append_decoded_char(char ch)
{
    if (ch == ' ' && at_word_boundary()) {
        return;
    }

    if (ch == MORSE_CODEC_PROSIGN_HH) {
        erase_last_word();
        return;
    }

    if (ch == MORSE_CODEC_UNKNOWN_CHAR) {
        append_unknown_marker();
        return;
    }

    const char *prosign_name = morse_codec_prosign_name(ch);
    if (prosign_name != NULL) {
        append_prosign_badge(prosign_name);

        if (ch == MORSE_CODEC_PROSIGN_SK) {
            /* End of contact: leave a blank line before whatever follows. */
            force_line_break();
            force_line_break();
        }
        return;
    }

    append_plain_char(ch);

    /* BT and AR are sent as one unbroken sequence that also happens to be
     * the standard timing for '=' and '+' (see morse_codec.h); by the same
     * convention used there, treat either glyph as the prosign and break
     * the line after it. */
    if (ch == '=' || ch == '+') {
        force_line_break();
    }
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
    case IAMBIC_KEYER_MODE_ULTIMATIC:
        return "Ultimatic";
    default:
        return "?";
    }
}

static void update_status_label(void)
{
    lv_label_set_text_fmt(s_wpm_label, "%u WPM", (unsigned)paddle_input_get_wpm());
    lv_label_set_text(s_mode_label, mode_name(paddle_input_get_mode()));
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
 * trailing gap - so glyph ink that extends a couple of pixels below the
 * font's reported line box (e.g. descenders) is clipped for as long as
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

void ui_practice_set_text_size(bool large_text)
{
    lv_obj_set_style_text_font(s_text_spans, large_text ? &lv_font_dejavu_mono_16_12 : &lv_font_unscii_8, 0);
    lv_obj_set_style_text_line_space(s_text_spans, large_text ? TEXT_LINE_SPACE_LARGE_PX : TEXT_LINE_SPACE_SMALL_PX,
                                      0);
    scroll_text_to_bottom();
}

void ui_practice_set_help_screen(lv_obj_t *help_screen)
{
    s_help_screen = help_screen;
}

void ui_practice_set_keyer_settings_screen(lv_obj_t *keyer_settings_screen)
{
    s_keyer_settings_screen = keyer_settings_screen;
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
    clear_erase_stack();
    paddle_input_reset_decoder();
    lv_obj_scroll_to_y(s_text_container, 0, LV_ANIM_OFF);
}

static void back_btn_cb(lv_event_t *e)
{
    lv_obj_t *menu_screen = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(menu_screen);
}

static void help_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_scr_load(s_help_screen);
}

static void keyer_settings_btn_cb(lv_event_t *e)
{
    lv_obj_t *practice_screen = (lv_obj_t *)lv_event_get_user_data(e);
    ui_settings_set_keyer_back_target(practice_screen);
    lv_scr_load(s_keyer_settings_screen);
}

lv_obj_t *ui_practice_create(QueueHandle_t decoded_char_queue, lv_obj_t *menu_screen,
                              iambic_keyer_mode_t initial_mode, uint16_t initial_wpm,
                              bool initial_large_text)
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
    ui_practice_set_text_size(initial_large_text);

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
    /* SPACE_BETWEEN, with status_col/right_group as real flex children
     * (below) rather than absolutely-positioned overlays: the flex engine
     * then guarantees Clear/status/right never overlap, however wide
     * right_group grows (e.g. once Help was added, an IGNORE_LAYOUT +
     * manual-align right_group here used to grow leftward into status_col's
     * centered text instead of pushing it out of the way). */
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *clear_btn = lv_button_create(btn_row);
    display_style_tile(clear_btn);
    lv_obj_add_event_cb(clear_btn, clear_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");
    lv_obj_set_style_text_color(clear_label, display_compensate_color(lv_color_white()), 0);

    /* status_col (WPM/Mode) is tappable - it opens the Keyer settings - but
     * stays a plain layout wrapper around the two labels otherwise (no
     * button styling), so it doesn't visually compete with Clear/Help/Back.
     * lv_obj_create() objects default to clickable, so remove_style_all()
     * below leaves that on; it does strip the default object theme's
     * padding though, so it's added back explicitly here - without it the
     * tap target would shrink to the bare bounding box of the two text
     * labels, much smaller than the finger-sized buttons beside it. */
    lv_obj_t *status_col = lv_obj_create(btn_row);
    lv_obj_remove_style_all(status_col);
    lv_obj_set_style_pad_all(status_col, 10, 0);
    lv_obj_add_event_cb(status_col, keyer_settings_btn_cb, LV_EVENT_CLICKED, scr);
    lv_obj_set_flex_flow(status_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(status_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    s_wpm_label = lv_label_create(status_col);
    lv_label_set_text_fmt(s_wpm_label, "%u WPM", (unsigned)initial_wpm);

    s_mode_label = lv_label_create(status_col);
    lv_label_set_text(s_mode_label, mode_name(initial_mode));

    /* right_group (Help, then the keying dot, then Back) - likewise not
     * clickable itself, only its button/dot children are. */
    lv_obj_t *right_group = lv_obj_create(btn_row);
    lv_obj_remove_style_all(right_group);
    lv_obj_clear_flag(right_group, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(right_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right_group, 8, 0);
    lv_obj_set_size(right_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    /* Icon-only: no text label, just the LV_SYMBOL_LIST glyph, since no
     * dedicated "help" glyph exists in LVGL's built-in symbol set and a
     * reference list is what this actually opens. */
    lv_obj_t *help_btn = lv_button_create(right_group);
    display_style_tile(help_btn);
    lv_obj_add_event_cb(help_btn, help_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *help_label = lv_label_create(help_btn);
    lv_label_set_text(help_label, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(help_label, display_compensate_color(lv_color_white()), 0);

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
