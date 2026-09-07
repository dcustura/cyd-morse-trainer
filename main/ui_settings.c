#include "ui_settings.h"

#include "paddle_input.h"
#include "settings_store.h"
#include "sidetone.h"
#include "touch_cal_store.h"
#include "ui_calibration.h"

#include "esp_system.h"

#include <stdio.h>
#include <string.h>

#define WPM_MIN MORSE_SETTINGS_WPM_MIN
#define WPM_MAX MORSE_SETTINGS_WPM_MAX
#define TONE_HZ_MIN MORSE_SETTINGS_TONE_HZ_MIN
#define TONE_HZ_MAX MORSE_SETTINGS_TONE_HZ_MAX
#define VOLUME_PCT_MIN MORSE_SETTINGS_VOLUME_PCT_MIN
#define VOLUME_PCT_MAX MORSE_SETTINGS_VOLUME_PCT_MAX
#define ENVELOPE_MS_MIN MORSE_SETTINGS_ENVELOPE_MS_MIN
#define ENVELOPE_MS_MAX MORSE_SETTINGS_ENVELOPE_MS_MAX
#define TEST_TONE_DURATION_MS 300

static lv_obj_t *s_settings_screen;
static lv_obj_t *s_wpm_slider;
static lv_obj_t *s_wpm_value_label;
static lv_obj_t *s_mode_dropdown;
static lv_obj_t *s_swap_switch;
static lv_obj_t *s_tone_slider;
static lv_obj_t *s_tone_value_label;
static lv_obj_t *s_envelope_slider;
static lv_obj_t *s_envelope_value_label;
static lv_obj_t *s_volume_slider;
static lv_obj_t *s_volume_value_label;

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

static void update_volume_label(int32_t pct)
{
    char text[24];
    snprintf(text, sizeof(text), "Volume: %d%%", (int)pct);
    lv_label_set_text(s_volume_value_label, text);
}

static void update_envelope_label(int32_t ms)
{
    char text[32];
    snprintf(text, sizeof(text), "Envelope: %d ms", (int)ms);
    lv_label_set_text(s_envelope_value_label, text);
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

static void volume_slider_cb(lv_event_t *e)
{
    (void)e;
    update_volume_label(lv_slider_get_value(s_volume_slider));
}

static void envelope_slider_cb(lv_event_t *e)
{
    (void)e;
    update_envelope_label(lv_slider_get_value(s_envelope_slider));
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
    sidetone_set_volume((uint8_t)lv_slider_get_value(s_volume_slider));
    sidetone_set_envelope_ms((uint16_t)lv_slider_get_value(s_envelope_slider));
    sidetone_key(true);
    lv_timer_create(test_tone_stop_cb, TEST_TONE_DURATION_MS, NULL);
}

static void save_btn_cb(lv_event_t *e)
{
    lv_obj_t *menu_screen = (lv_obj_t *)lv_event_get_user_data(e);

    uint16_t wpm = (uint16_t)lv_slider_get_value(s_wpm_slider);
    iambic_keyer_mode_t mode = (iambic_keyer_mode_t)lv_dropdown_get_selected(s_mode_dropdown);
    bool swap = lv_obj_has_state(s_swap_switch, LV_STATE_CHECKED);
    uint16_t tone_hz = (uint16_t)lv_slider_get_value(s_tone_slider);
    uint8_t volume_pct = (uint8_t)lv_slider_get_value(s_volume_slider);
    uint16_t envelope_ms = (uint16_t)lv_slider_get_value(s_envelope_slider);

    paddle_input_set_wpm(wpm);
    paddle_input_set_mode(mode);
    paddle_input_set_swap(swap);
    sidetone_set_freq(tone_hz);
    sidetone_set_volume(volume_pct);
    sidetone_set_envelope_ms(envelope_ms);

    const morse_settings_t settings = {
        .wpm = wpm,
        .keymode = mode,
        .paddle_swap = swap,
        .tone_hz = tone_hz,
        .volume_pct = volume_pct,
        .envelope_ms = envelope_ms,
    };
    settings_store_save(&settings);

    lv_scr_load(menu_screen);
}

static void nav_btn_cb(lv_event_t *e)
{
    lv_obj_t *target_screen = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(target_screen);
}

static void calibrate_btn_cb(lv_event_t *e)
{
    lv_obj_t *calibration_screen = (lv_obj_t *)lv_event_get_user_data(e);
    ui_calibration_set_cancel_target(s_settings_screen);
    lv_scr_load(calibration_screen);
}

static void reset_confirm_btn_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target_obj(e);
    lv_obj_t *btn_label = lv_obj_get_child(btn, 0);
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);

    if (strcmp(lv_label_get_text(btn_label), "Reset") == 0) {
        settings_store_reset();
        touch_cal_store_reset();
        esp_restart();
    }

    lv_msgbox_close(mbox);
}

static void reset_btn_cb(lv_event_t *e)
{
    (void)e;

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "Reset to Factory Defaults?");
    lv_msgbox_add_text(mbox, "This clears all keyer settings and the touch calibration, then restarts the device.");

    lv_obj_t *reset_confirm_btn = lv_msgbox_add_footer_button(mbox, "Reset");
    lv_obj_add_event_cb(reset_confirm_btn, reset_confirm_btn_cb, LV_EVENT_CLICKED, mbox);

    lv_obj_t *cancel_btn = lv_msgbox_add_footer_button(mbox, "Cancel");
    lv_obj_add_event_cb(cancel_btn, reset_confirm_btn_cb, LV_EVENT_CLICKED, mbox);
}

lv_obj_t *ui_settings_create(lv_obj_t *menu_screen, lv_obj_t *calibration_screen,
                              lv_obj_t *touch_test_screen, iambic_keyer_mode_t initial_mode,
                              uint16_t initial_wpm, bool initial_swap, uint16_t initial_tone_hz,
                              uint8_t initial_volume_pct, uint16_t initial_envelope_ms)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    s_settings_screen = scr;
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

    s_envelope_value_label = lv_label_create(scr);
    update_envelope_label(initial_envelope_ms);

    s_envelope_slider = lv_slider_create(scr);
    lv_slider_set_range(s_envelope_slider, ENVELOPE_MS_MIN, ENVELOPE_MS_MAX);
    lv_slider_set_value(s_envelope_slider, initial_envelope_ms, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_envelope_slider, envelope_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_volume_value_label = lv_label_create(scr);
    update_volume_label(initial_volume_pct);

    s_volume_slider = lv_slider_create(scr);
    lv_slider_set_range(s_volume_slider, VOLUME_PCT_MIN, VOLUME_PCT_MAX);
    lv_slider_set_value(s_volume_slider, initial_volume_pct, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_volume_slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *test_tone_btn = lv_button_create(scr);
    lv_obj_add_event_cb(test_tone_btn, test_tone_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *test_tone_label = lv_label_create(test_tone_btn);
    lv_label_set_text(test_tone_label, "Test tone");

    lv_obj_t *calibrate_btn = lv_button_create(scr);
    lv_obj_add_event_cb(calibrate_btn, calibrate_btn_cb, LV_EVENT_CLICKED, calibration_screen);
    lv_obj_t *calibrate_label = lv_label_create(calibrate_btn);
    lv_label_set_text(calibrate_label, "Calibrate Touchscreen");

    lv_obj_t *verify_btn = lv_button_create(scr);
    lv_obj_add_event_cb(verify_btn, nav_btn_cb, LV_EVENT_CLICKED, touch_test_screen);
    lv_obj_t *verify_label = lv_label_create(verify_btn);
    lv_label_set_text(verify_label, "Verify Calibration");

    lv_obj_t *reset_btn = lv_button_create(scr);
    lv_obj_add_event_cb(reset_btn, reset_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Reset to Factory Defaults");

    lv_obj_t *btn_row = lv_obj_create(scr);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *save_btn = lv_button_create(btn_row);
    lv_obj_add_event_cb(save_btn, save_btn_cb, LV_EVENT_CLICKED, menu_screen);
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "OK");

    lv_obj_t *back_btn = lv_button_create(btn_row);
    lv_obj_add_event_cb(back_btn, nav_btn_cb, LV_EVENT_CLICKED, menu_screen);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Cancel");

    return scr;
}
