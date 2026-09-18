#include "ui_help.h"

#include "display_init.h"
#include "esp_timer.h"
#include "morse_codec.h"
#include "paddle_input.h"
#include "sidetone.h"

#include <string.h>

#define TAB_BAR_SIZE 32

/* One lv_buttonmatrix per tab instead of one lv_button+lv_label per
 * character: LVGL here uses a small fixed-size internal memory pool
 * (CONFIG_LV_MEM_SIZE_KILOBYTES=64, not the general heap), and a much
 * heavier earlier version of this screen (~130 individual objects, all
 * stacked in one scrolling popup) exhausted it, hanging the whole UI task
 * forever (LVGL's out-of-memory assert spins rather than failing
 * gracefully) - and even short of that, was visibly sluggish. A button
 * matrix draws a whole grid from one lightweight widget, and only one
 * tab's matrix is ever on screen at a time. lv_buttonmatrix_set_map() keeps
 * a reference to the map array rather than copying it, so these must
 * outlive the matrix - static storage does that for free. */
/* 26 letters don't divide evenly into rows; a 9/9/8 split would leave the
 * last row's buttons wider than the other rows' (lv_buttonmatrix splits
 * each row's width evenly among only that row's own buttons). Pad to 9/9/9
 * with a trailing hidden filler instead - LV_BUTTONMATRIX_CTRL_HIDDEN keeps
 * its layout slot but removes it from drawing and touch, so every real
 * button ends up the same size. See LETTERS_FILLER_ID below. */
static const char *const LETTERS_MAP[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "\n",
    "J", "K", "L", "M", "N", "O", "P", "Q", "R", "\n",
    "S", "T", "U", "V", "W", "X", "Y", "Z", " ", "",
};
#define LETTERS_FILLER_ID 26

static const char *const DIGITS_MAP[] = {
    "0", "1", "2", "3", "4", "\n",
    "5", "6", "7", "8", "9", "",
};

/* 18 symbols, so 3 rows of 6 divide evenly - no filler needed. */
static const char *const SYMBOLS_MAP[] = {
    ".", ",", "?", "'", "!", "/", "\n",
    "(", ")", "&", ":", ";", "=", "\n",
    "+", "-", "_", "\"", "$", "@", "",
};

static const char *const PROSIGN_MAP[] = {
    "SK", "HH", "VE", "CT", "\n",
    "BT", "AR", "KN", "AS", "",
};

/* BT/AR/KN/AS have no sentinel of their own (see morse_codec.h) - they
 * decode to the punctuation glyph they're timed identically to, by
 * convention, so their pattern is looked up via that glyph instead. Letters/
 * digits/symbols need no such table: their button text (a single character)
 * is the lookup character. */
typedef struct {
    const char *label;
    char lookup_ch;
} prosign_entry_t;

static const prosign_entry_t PROSIGNS[] = {
    { "SK", MORSE_CODEC_PROSIGN_SK }, { "HH", MORSE_CODEC_PROSIGN_HH },
    { "VE", MORSE_CODEC_PROSIGN_VE }, { "CT", MORSE_CODEC_PROSIGN_CT },
    { "BT", '=' },                    { "AR", '+' },
    { "KN", '(' },                    { "AS", '&' },
};

static lv_obj_t *s_highlighted_matrix;
static uint32_t s_highlighted_id = LV_BUTTONMATRIX_BUTTON_NONE;

static esp_timer_handle_t s_play_timer;
static const char *s_play_pattern;
static size_t s_play_index;
static bool s_play_tone_phase;
static uint32_t s_play_unit_ms;

static void clear_highlight(void)
{
    if (s_highlighted_matrix != NULL && s_highlighted_id != LV_BUTTONMATRIX_BUTTON_NONE) {
        lv_buttonmatrix_clear_button_ctrl(s_highlighted_matrix, s_highlighted_id, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    s_highlighted_matrix = NULL;
    s_highlighted_id = LV_BUTTONMATRIX_BUTTON_NONE;
}

static void set_highlight(lv_obj_t *matrix, uint32_t id)
{
    clear_highlight();
    lv_buttonmatrix_set_button_ctrl(matrix, id, LV_BUTTONMATRIX_CTRL_CHECKED);
    s_highlighted_matrix = matrix;
    s_highlighted_id = id;
}

static void stop_playback(void)
{
    if (s_play_timer != NULL && esp_timer_is_active(s_play_timer)) {
        esp_timer_stop(s_play_timer);
    }
    sidetone_key(false);
}

static void play_step_cb(void *arg);

/*
 * Schedules the next play_step_cb() call as a one-shot esp_timer rather
 * than an lv_timer: an lv_timer only runs from inside lv_timer_handler(),
 * on the same LVGL task that also does screen redraws. Highlighting the
 * tapped button queues a redraw right as the first element starts, and on
 * this SPI TFT that redraw took long enough to noticeably delay the LVGL
 * task getting back to check pending timers - the first dit/gap came out
 * ~30-80ms longer than the rest, which were all spot-on. esp_timer
 * callbacks run on their own dedicated task, so they're unaffected by LVGL
 * redraw work - matching how the real keying path (paddle_input's own
 * FreeRTOS task) has always avoided this.
 */
static void schedule_step(uint32_t delay_ms)
{
    ESP_ERROR_CHECK(esp_timer_start_once(s_play_timer, (uint64_t)delay_ms * 1000u));
}

static void play_step_cb(void *arg)
{
    (void)arg;

    if (s_play_tone_phase) {
        /* An element (dit or dah) just finished sounding. */
        sidetone_key(false);
        s_play_index++;
        if (s_play_pattern[s_play_index] == '\0') {
            return; /* Pattern finished; leave the highlight as-is. */
        }
        s_play_tone_phase = false;
        schedule_step(s_play_unit_ms); /* inter-element gap */
        return;
    }

    /* The inter-element gap just finished; sound the next element. */
    s_play_tone_phase = true;
    sidetone_key(true);
    uint32_t duration_ms = (s_play_pattern[s_play_index] == '-') ? 3u * s_play_unit_ms : s_play_unit_ms;
    schedule_step(duration_ms);
}

/* Plays pattern (a string of '.'/'-') at the trainer's current WPM, so the
 * reference chart always sounds like the speed the operator is practicing
 * at. matrix_event_cb() only calls this once any previous playback has
 * finished, so there's nothing to cancel here. */
static void start_playback(const char *pattern)
{
    s_play_unit_ms = morse_codec_wpm_to_unit_ms(paddle_input_get_wpm());

    s_play_pattern = pattern;
    s_play_index = 0;
    s_play_tone_phase = true;
    sidetone_key(true);
    uint32_t duration_ms = (pattern[0] == '-') ? 3u * s_play_unit_ms : s_play_unit_ms;
    schedule_step(duration_ms);
}

/* Letters/digits/symbols: the button's own (single-character) text is the
 * lookup character. Prosigns: looked up by name in PROSIGNS. */
static char lookup_char_for_label(const char *label_text)
{
    if (label_text[1] == '\0') {
        return label_text[0];
    }
    for (size_t i = 0; i < sizeof(PROSIGNS) / sizeof(PROSIGNS[0]); i++) {
        if (strcmp(PROSIGNS[i].label, label_text) == 0) {
            return PROSIGNS[i].lookup_ch;
        }
    }
    return 0;
}

static void matrix_event_cb(lv_event_t *e)
{
    /* Ignore any further press - the same button held down, a repeat event
     * from it (see add_matrix_tab()'s NO_REPEAT ctrl below, belt-and-
     * suspenders), or a different button tapped mid-tone - until the
     * current character has finished playing. */
    if (s_play_timer != NULL && esp_timer_is_active(s_play_timer)) {
        return;
    }

    lv_obj_t *matrix = lv_event_get_target_obj(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(matrix);
    if (id == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }

    const char *label_text = lv_buttonmatrix_get_button_text(matrix, id);
    const char *pattern = morse_codec_pattern_for_char(lookup_char_for_label(label_text));
    if (pattern == NULL) {
        return;
    }

    /* Deliberately no on-screen character/pattern readout: the point of
     * this screen is to learn each code by ear, not to read it off - the
     * highlighted button is feedback enough for "which one did I just
     * play." */
    set_highlight(matrix, id);
    start_playback(pattern);
}

/* One tab per category, each holding a single full-size button matrix -
 * only the active tab's matrix is ever visible, so it gets the whole
 * content area (bigger, easier-to-hit buttons than cramming every category
 * into one scrolling list). Returns the matrix so a caller whose map needs
 * padding (see LETTERS_FILLER_ID) can hide that one filler button itself. */
static lv_obj_t *add_matrix_tab(lv_obj_t *tabview, const char *title, const char *const map[])
{
    lv_obj_t *tab = lv_tabview_add_tab(tabview, title);
    lv_obj_set_style_pad_all(tab, 2, 0);

    lv_obj_t *matrix = lv_buttonmatrix_create(tab);
    lv_buttonmatrix_set_map(matrix, map);
    /* Without this, lv_buttonmatrix also fires LV_EVENT_VALUE_CHANGED
     * repeatedly (~every 300ms) while a button is held down, restarting
     * playback over and over and making a held key sound like one
     * continuous, overlapping tone instead of a single character. */
    lv_buttonmatrix_set_button_ctrl_all(matrix, LV_BUTTONMATRIX_CTRL_NO_REPEAT);
    lv_obj_set_size(matrix, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(matrix, 0, 0);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS);
    display_style_tile_items(matrix);
    lv_obj_set_style_bg_color(matrix, display_compensate_color(lv_palette_main(LV_PALETTE_ORANGE)),
                               LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_event_cb(matrix, matrix_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return matrix;
}

static void back_btn_cb(lv_event_t *e)
{
    lv_obj_t *practice_screen = (lv_obj_t *)lv_event_get_user_data(e);
    stop_playback();
    lv_scr_load(practice_screen);
}

lv_obj_t *ui_help_create(lv_obj_t *practice_screen)
{
    const esp_timer_create_args_t play_timer_args = {
        .callback = play_step_cb,
        .name = "ui_help_play",
    };
    ESP_ERROR_CHECK(esp_timer_create(&play_timer_args, &s_play_timer));

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(scr, 4, 0);

    /* No header: the whole screen is just the tabview, so the button grid
     * gets as much space as possible. Back lives as a plain, non-tab-
     * switching button appended to the tab bar itself (see below), rather
     * than a separate row, for the same reason. */
    lv_obj_t *tabview = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_size(tabview, TAB_BAR_SIZE);
    lv_obj_set_size(tabview, LV_PCT(100), LV_PCT(100));
    /* Tab buttons still switch tabs (lv_tabview_set_active() scrolls the
     * content programmatically regardless of this flag) - only the user's
     * own swipe/drag gesture is disabled, since it was too easy to trigger
     * by accident while reaching for a button near a tab's edge. */
    lv_obj_clear_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *letters_matrix = add_matrix_tab(tabview, "A-Z", LETTERS_MAP);
    lv_buttonmatrix_set_button_ctrl(letters_matrix, LETTERS_FILLER_ID, LV_BUTTONMATRIX_CTRL_HIDDEN);
    add_matrix_tab(tabview, "0-9", DIGITS_MAP);
    add_matrix_tab(tabview, "/?", SYMBOLS_MAP);
    add_matrix_tab(tabview, "Prosigns", PROSIGN_MAP);

    /* A plain button appended after the 4 real tab buttons, sized/grown the
     * same way lv_tabview_add_tab() sizes its own buttons (see
     * lv_tabview.c) so it reads as a same-sized 5th slot in the tab bar -
     * but with its own click handler instead of button_clicked_event_cb,
     * so tapping it navigates back instead of switching to a (nonexistent)
     * tab. */
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
    lv_obj_t *back_btn = lv_button_create(tab_bar);
    lv_obj_set_flex_grow(back_btn, 1);
    lv_obj_set_size(back_btn, LV_PCT(100), LV_PCT(100));
    display_style_button_dismiss(back_btn);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, practice_screen);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_label, display_compensate_color(lv_color_white()), 0);

    return scr;
}
