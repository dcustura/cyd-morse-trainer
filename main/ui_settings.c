#include "ui_settings.h"

#include "paddle_input.h"
#include "sidetone.h"

#include <stdio.h>

#define WPM_MIN 5
#define WPM_MAX 40
#define TONE_HZ_MIN 300
#define TONE_HZ_MAX 1200
#define TEST_TONE_DURATION_MS 300

static lv_obj_t *s_wpm_slider;
static lv_obj_t *s_wpm_value_label;
static lv_obj_t *s_mode_dropdown;
static lv_obj_t *s_swap_switch;
static lv_obj_t *s_tone_slider;
static lv_obj_t *s_tone_value_label;

static void update_wpm_label(int32_t wpm)
{
    char text[16];
    snprintf(text, sizeof(text), "WPM: %d", (int)wpm);
    lv_label_set_text(s_wpm_value_label, text);
}

static void update_tone_label(int32_t hz)
{
    char text[24];
    snprintf(text, sizeof(text), "Tone: %d Hz", (int)hz);
    lv_label_set_text(s_tone_value_label, text);
}

static void wpm_slider_cb(lv_event_t *e)
{
    (void)e;
    update_wpm_label(lv_slider_get_value(s_wpm_slider));
}

static void tone_slider_cb(lv_event_t *e)
{
    (void)e;
    update_tone_label(lv_slider_get_value(s_tone_slider));
}

static void test_tone_stop_cb(lv_timer_t *timer)
{
    sidetone_key(false);
    lv_timer_del(timer);
}

static void test_tone_btn_cb(lv_event_t *e)
{
    (void)e;
    sidetone_set_freq((uint16_t)lv_slider_get_value(s_tone_slider));
    sidetone_key(true);
    lv_timer_create(test_tone_stop_cb, TEST_TONE_DURATION_MS, NULL);
}

static void save_btn_cb(lv_event_t *e)
{
    (void)e;

    uint16_t wpm = (uint16_t)lv_slider_get_value(s_wpm_slider);
    iambic_keyer_mode_t mode = (iambic_keyer_mode_t)lv_dropdown_get_selected(s_mode_dropdown);
    bool swap = lv_obj_has_state(s_swap_switch, LV_STATE_CHECKED);
    uint16_t tone_hz = (uint16_t)lv_slider_get_value(s_tone_slider);

    paddle_input_set_wpm(wpm);
    paddle_input_set_mode(mode);
    paddle_input_set_swap(swap);
    sidetone_set_freq(tone_hz);
}

static void back_btn_cb(lv_event_t *e)
{
    lv_obj_t *menu_screen = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(menu_screen);
}

lv_obj_t *ui_settings_create(lv_obj_t *menu_screen, iambic_keyer_mode_t initial_mode,
                              uint16_t initial_wpm, bool initial_swap, uint16_t initial_tone_hz)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Settings");

    s_wpm_value_label = lv_label_create(scr);
    update_wpm_label(initial_wpm);

    s_wpm_slider = lv_slider_create(scr);
    lv_slider_set_range(s_wpm_slider, WPM_MIN, WPM_MAX);
    lv_slider_set_value(s_wpm_slider, initial_wpm, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_wpm_slider, wpm_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_mode_dropdown = lv_dropdown_create(scr);
    lv_dropdown_set_options(s_mode_dropdown, "Iambic Mode A\nIambic Mode B\nStraight Key");
    lv_dropdown_set_selected(s_mode_dropdown, (uint32_t)initial_mode);

    lv_obj_t *swap_row = lv_obj_create(scr);
    lv_obj_set_flex_flow(swap_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(swap_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_t *swap_label = lv_label_create(swap_row);
    lv_label_set_text(swap_label, "Swap dit/dah");
    s_swap_switch = lv_switch_create(swap_row);
    if (initial_swap) {
        lv_obj_add_state(s_swap_switch, LV_STATE_CHECKED);
    }

    s_tone_value_label = lv_label_create(scr);
    update_tone_label(initial_tone_hz);

    s_tone_slider = lv_slider_create(scr);
    lv_slider_set_range(s_tone_slider, TONE_HZ_MIN, TONE_HZ_MAX);
    lv_slider_set_value(s_tone_slider, initial_tone_hz, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_tone_slider, tone_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *test_tone_btn = lv_button_create(scr);
    lv_obj_add_event_cb(test_tone_btn, test_tone_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *test_tone_label = lv_label_create(test_tone_btn);
    lv_label_set_text(test_tone_label, "Test tone");

    lv_obj_t *btn_row = lv_obj_create(scr);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *save_btn = lv_button_create(btn_row);
    lv_obj_add_event_cb(save_btn, save_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");

    lv_obj_t *back_btn = lv_button_create(btn_row);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, menu_screen);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");

    return scr;
}
