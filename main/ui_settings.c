#include "ui_settings.h"

#include "display_init.h"
#include "paddle_input.h"
#include "settings_store.h"
#include "sidetone.h"
#include "touch_cal_store.h"
#include "ui_calibration.h"
#include "ui_touch_test.h"

#include "esp_system.h"

#include <stddef.h>
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
#define STEP_BTN_SIZE 60
#define VALUE_LABEL_WIDTH 70
#define OPTION_BTN_HEIGHT 70
#define FOOTER_BTN_HEIGHT 50
#define FOOTER_BTN_MIN_WIDTH 90

typedef enum {
    FIELD_WPM,
    FIELD_TONE_HZ,
    FIELD_VOLUME,
    FIELD_ENVELOPE,
    FIELD_COUNT,
} numeric_field_t;

typedef struct {
    const char *popup_title;
    int32_t min;
    int32_t max;
    int32_t step;
    const char *unit_fmt;
    bool has_test;
} numeric_field_info_t;

static const numeric_field_info_t s_field_info[FIELD_COUNT] = {
    [FIELD_WPM] = {"WPM", WPM_MIN, WPM_MAX, 1, "%d", false},
    [FIELD_TONE_HZ] = {"Pitch", TONE_HZ_MIN, TONE_HZ_MAX, 10, "%d Hz", true},
    [FIELD_VOLUME] = {"Volume", VOLUME_PCT_MIN, VOLUME_PCT_MAX, 5, "%d%%", true},
    [FIELD_ENVELOPE] = {"Smoothing", ENVELOPE_MS_MIN, ENVELOPE_MS_MAX, 2, "%d ms", true},
};

static lv_obj_t *s_settings_screen;
static lv_obj_t *s_touch_submenu_screen;
static morse_settings_t s_current_settings;

static lv_obj_t *s_wpm_tile_value;
static lv_obj_t *s_keymode_tile_value;
static lv_obj_t *s_swap_tile_value;
static lv_obj_t *s_tone_tile_value;
static lv_obj_t *s_volume_tile_value;
static lv_obj_t *s_envelope_tile_value;

/* Only one popup can be open at a time; these track the one currently shown. */
static numeric_field_t s_popup_field;
static lv_obj_t *s_popup_value_label;

static const char *keymode_text(iambic_keyer_mode_t mode)
{
    switch (mode) {
    case IAMBIC_KEYER_MODE_A:
        return "Mode A";
    case IAMBIC_KEYER_MODE_B:
        return "Mode B";
    case IAMBIC_KEYER_MODE_STRAIGHT:
        return "Straight";
    default:
        return "?";
    }
}

static void refresh_tile_labels(void)
{
    char text[24];

    snprintf(text, sizeof(text), s_field_info[FIELD_WPM].unit_fmt, (int)s_current_settings.wpm);
    lv_label_set_text(s_wpm_tile_value, text);

    lv_label_set_text(s_keymode_tile_value, keymode_text(s_current_settings.keymode));
    lv_label_set_text(s_swap_tile_value, s_current_settings.paddle_swap ? "Swapped" : "Normal");

    snprintf(text, sizeof(text), s_field_info[FIELD_TONE_HZ].unit_fmt, (int)s_current_settings.tone_hz);
    lv_label_set_text(s_tone_tile_value, text);

    snprintf(text, sizeof(text), s_field_info[FIELD_VOLUME].unit_fmt, (int)s_current_settings.volume_pct);
    lv_label_set_text(s_volume_tile_value, text);

    snprintf(text, sizeof(text), s_field_info[FIELD_ENVELOPE].unit_fmt, (int)s_current_settings.envelope_ms);
    lv_label_set_text(s_envelope_tile_value, text);
}

static int32_t field_get_value(numeric_field_t field)
{
    switch (field) {
    case FIELD_WPM:
        return s_current_settings.wpm;
    case FIELD_TONE_HZ:
        return s_current_settings.tone_hz;
    case FIELD_VOLUME:
        return s_current_settings.volume_pct;
    case FIELD_ENVELOPE:
        return s_current_settings.envelope_ms;
    default:
        return 0;
    }
}

static void field_set_value(numeric_field_t field, int32_t value)
{
    switch (field) {
    case FIELD_WPM:
        s_current_settings.wpm = (uint16_t)value;
        paddle_input_set_wpm(s_current_settings.wpm);
        break;
    case FIELD_TONE_HZ:
        s_current_settings.tone_hz = (uint16_t)value;
        sidetone_set_freq(s_current_settings.tone_hz);
        break;
    case FIELD_VOLUME:
        s_current_settings.volume_pct = (uint8_t)value;
        sidetone_set_volume(s_current_settings.volume_pct);
        break;
    case FIELD_ENVELOPE:
        s_current_settings.envelope_ms = (uint16_t)value;
        sidetone_set_envelope_ms(s_current_settings.envelope_ms);
        break;
    default:
        return;
    }
    settings_store_save(&s_current_settings);
}

static void settings_screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    refresh_tile_labels();
}

static void msgbox_close_cb(lv_event_t *e)
{
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
    lv_msgbox_close(mbox);
}

static void strip_pane_style(lv_obj_t *obj)
{
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 4, 0);
}

static void style_footer_button(lv_obj_t *btn)
{
    lv_obj_set_height(btn, FOOTER_BTN_HEIGHT);
    lv_obj_set_style_min_width(btn, FOOTER_BTN_MIN_WIDTH, 0);
}

static void test_tone_stop_cb(lv_timer_t *timer)
{
    sidetone_key(false);
    lv_timer_del(timer);
}

static void test_field_btn_cb(lv_event_t *e)
{
    (void)e;
    sidetone_key(true);
    lv_timer_create(test_tone_stop_cb, TEST_TONE_DURATION_MS, NULL);
}

static void update_popup_value_label(void)
{
    char text[24];
    snprintf(text, sizeof(text), s_field_info[s_popup_field].unit_fmt, (int)field_get_value(s_popup_field));
    lv_label_set_text(s_popup_value_label, text);
}

static void numeric_step_cb(lv_event_t *e)
{
    int32_t delta = (int32_t)(intptr_t)lv_event_get_user_data(e);
    const numeric_field_info_t *info = &s_field_info[s_popup_field];

    int32_t value = field_get_value(s_popup_field) + delta;
    if (value < info->min) {
        value = info->min;
    }
    if (value > info->max) {
        value = info->max;
    }

    field_set_value(s_popup_field, value);
    update_popup_value_label();
    refresh_tile_labels();
}

static void open_numeric_popup(numeric_field_t field)
{
    s_popup_field = field;
    const numeric_field_info_t *info = &s_field_info[field];

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, info->popup_title);

    lv_obj_t *content = lv_msgbox_get_content(mbox);
    strip_pane_style(content);
    lv_obj_t *row = lv_obj_create(content);
    strip_pane_style(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *minus_btn = lv_button_create(row);
    display_style_button_teal(minus_btn);
    lv_obj_set_size(minus_btn, STEP_BTN_SIZE, STEP_BTN_SIZE);
    lv_obj_add_event_cb(minus_btn, numeric_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(-info->step));
    lv_obj_add_event_cb(minus_btn, numeric_step_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)(-info->step));
    lv_obj_t *minus_label = lv_label_create(minus_btn);
    lv_label_set_text(minus_label, "-");
    lv_obj_center(minus_label);

    s_popup_value_label = lv_label_create(row);
    lv_obj_set_style_text_font(s_popup_value_label, &lv_font_unscii_8, 0);
    lv_obj_set_width(s_popup_value_label, VALUE_LABEL_WIDTH);
    lv_obj_set_style_text_align(s_popup_value_label, LV_TEXT_ALIGN_CENTER, 0);
    update_popup_value_label();

    lv_obj_t *plus_btn = lv_button_create(row);
    display_style_button_teal(plus_btn);
    lv_obj_set_size(plus_btn, STEP_BTN_SIZE, STEP_BTN_SIZE);
    lv_obj_add_event_cb(plus_btn, numeric_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(info->step));
    lv_obj_add_event_cb(plus_btn, numeric_step_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)(info->step));
    lv_obj_t *plus_label = lv_label_create(plus_btn);
    lv_label_set_text(plus_label, "+");
    lv_obj_center(plus_label);

    if (info->has_test) {
        lv_obj_t *test_btn = lv_msgbox_add_footer_button(mbox, "Test");
        style_footer_button(test_btn);
        lv_obj_add_event_cb(test_btn, test_field_btn_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *close_btn = lv_msgbox_add_footer_button(mbox, "Close");
    style_footer_button(close_btn);
    lv_obj_add_event_cb(close_btn, msgbox_close_cb, LV_EVENT_CLICKED, mbox);
}

static void numeric_tile_cb(lv_event_t *e)
{
    numeric_field_t field = (numeric_field_t)(intptr_t)lv_event_get_user_data(e);
    open_numeric_popup(field);
}

static void keymode_select_cb(lv_event_t *e)
{
    iambic_keyer_mode_t mode = (iambic_keyer_mode_t)(intptr_t)lv_event_get_user_data(e);
    s_current_settings.keymode = mode;
    paddle_input_set_mode(mode);
    settings_store_save(&s_current_settings);
    refresh_tile_labels();
}

static void keymode_tile_cb(lv_event_t *e)
{
    (void)e;

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "Key Mode");
    lv_obj_t *content = lv_msgbox_get_content(mbox);
    strip_pane_style(content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);

    static const struct {
        const char *label;
        iambic_keyer_mode_t mode;
    } options[] = {
        {"Mode A", IAMBIC_KEYER_MODE_A},
        {"Mode B", IAMBIC_KEYER_MODE_B},
        {"Straight", IAMBIC_KEYER_MODE_STRAIGHT},
    };

    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
        lv_obj_t *btn = lv_button_create(content);
        display_style_button_teal(btn);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, OPTION_BTN_HEIGHT);
        lv_obj_add_event_cb(btn, keymode_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)options[i].mode);
        lv_obj_add_event_cb(btn, msgbox_close_cb, LV_EVENT_CLICKED, mbox);
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, options[i].label);
        lv_obj_center(label);
    }

    lv_obj_t *close_btn = lv_msgbox_add_footer_button(mbox, "Close");
    style_footer_button(close_btn);
    lv_obj_add_event_cb(close_btn, msgbox_close_cb, LV_EVENT_CLICKED, mbox);
}

static void swap_select_cb(lv_event_t *e)
{
    bool swap = (bool)(intptr_t)lv_event_get_user_data(e);
    s_current_settings.paddle_swap = swap;
    paddle_input_set_swap(swap);
    settings_store_save(&s_current_settings);
    refresh_tile_labels();
}

static void swap_tile_cb(lv_event_t *e)
{
    (void)e;

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "Paddle Swap");
    lv_obj_t *content = lv_msgbox_get_content(mbox);
    strip_pane_style(content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);

    static const struct {
        const char *label;
        bool swap;
    } options[] = {
        {"Normal", false},
        {"Swapped", true},
    };

    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
        lv_obj_t *btn = lv_button_create(content);
        display_style_button_teal(btn);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, OPTION_BTN_HEIGHT);
        lv_obj_add_event_cb(btn, swap_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)options[i].swap);
        lv_obj_add_event_cb(btn, msgbox_close_cb, LV_EVENT_CLICKED, mbox);
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, options[i].label);
        lv_obj_center(label);
    }

    lv_obj_t *close_btn = lv_msgbox_add_footer_button(mbox, "Close");
    style_footer_button(close_btn);
    lv_obj_add_event_cb(close_btn, msgbox_close_cb, LV_EVENT_CLICKED, mbox);
}

static void nav_btn_cb(lv_event_t *e)
{
    lv_obj_t *target_screen = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(target_screen);
}

static void calibrate_btn_cb(lv_event_t *e)
{
    lv_obj_t *calibration_screen = (lv_obj_t *)lv_event_get_user_data(e);
    ui_calibration_set_cancel_target(s_touch_submenu_screen);
    lv_scr_load(calibration_screen);
}

static void touch_tile_cb(lv_event_t *e)
{
    (void)e;
    lv_scr_load(s_touch_submenu_screen);
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

static lv_obj_t *create_tile(lv_obj_t *grid, const char *name, int32_t col, int32_t row, lv_event_cb_t cb,
                              void *user_data, lv_obj_t **value_label_out)
{
    lv_obj_t *tile = lv_button_create(grid);
    display_style_button_teal(tile);
    lv_obj_set_grid_cell(tile, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(tile, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *name_label = lv_label_create(tile);
    lv_label_set_text(name_label, name);

    if (value_label_out != NULL) {
        *value_label_out = lv_label_create(tile);
    }

    return tile;
}

static lv_obj_t *create_touch_submenu(lv_obj_t *settings_screen, lv_obj_t *calibration_screen,
                                       lv_obj_t *touch_test_screen)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 4, 0);

    lv_obj_t *top_bar = lv_obj_create(scr);
    lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(top_bar, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *back_btn = lv_button_create(top_bar);
    display_style_button_teal(back_btn);
    lv_obj_add_event_cb(back_btn, nav_btn_cb, LV_EVENT_CLICKED, settings_screen);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "< Back");

    lv_obj_t *title = lv_label_create(top_bar);
    lv_label_set_text(title, "Touchscreen");

    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_pad_column(row, 4, 0);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);

    lv_obj_t *calibrate_tile = create_tile(row, "Calibrate", 0, 0, calibrate_btn_cb, calibration_screen, NULL);
    lv_obj_set_flex_grow(calibrate_tile, 1);
    lv_obj_set_height(calibrate_tile, LV_PCT(100));

    lv_obj_t *verify_tile = create_tile(row, "Verify\nCalibration", 0, 0, nav_btn_cb, touch_test_screen, NULL);
    lv_obj_set_flex_grow(verify_tile, 1);
    lv_obj_set_height(verify_tile, LV_PCT(100));

    return scr;
}

lv_obj_t *ui_settings_create(lv_obj_t *menu_screen, lv_obj_t *calibration_screen,
                              lv_obj_t *touch_test_screen, iambic_keyer_mode_t initial_mode,
                              uint16_t initial_wpm, bool initial_swap, uint16_t initial_tone_hz,
                              uint8_t initial_volume_pct, uint16_t initial_envelope_ms)
{
    s_current_settings = (morse_settings_t){
        .wpm = initial_wpm,
        .keymode = initial_mode,
        .paddle_swap = initial_swap,
        .tone_hz = initial_tone_hz,
        .volume_pct = initial_volume_pct,
        .envelope_ms = initial_envelope_ms,
    };

    lv_obj_t *scr = lv_obj_create(NULL);
    s_settings_screen = scr;
    lv_obj_add_event_cb(scr, settings_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_set_style_pad_all(scr, 4, 0);

    lv_obj_t *grid = scr;
    static const int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    create_tile(grid, "WPM", 0, 0, numeric_tile_cb, (void *)(intptr_t)FIELD_WPM, &s_wpm_tile_value);
    create_tile(grid, "Key Mode", 1, 0, keymode_tile_cb, NULL, &s_keymode_tile_value);
    create_tile(grid, "Paddle Swap", 2, 0, swap_tile_cb, NULL, &s_swap_tile_value);

    create_tile(grid, "Touchscreen", 0, 1, touch_tile_cb, NULL, NULL);
    create_tile(grid, "Pitch", 1, 1, numeric_tile_cb, (void *)(intptr_t)FIELD_TONE_HZ, &s_tone_tile_value);
    create_tile(grid, "Volume", 2, 1, numeric_tile_cb, (void *)(intptr_t)FIELD_VOLUME, &s_volume_tile_value);

    create_tile(grid, "Smoothing", 0, 2, numeric_tile_cb, (void *)(intptr_t)FIELD_ENVELOPE,
                &s_envelope_tile_value);
    create_tile(grid, "Reset to\nDefaults", 1, 2, reset_btn_cb, NULL, NULL);
    create_tile(grid, "< Back", 2, 2, nav_btn_cb, menu_screen, NULL);

    s_touch_submenu_screen = create_touch_submenu(scr, calibration_screen, touch_test_screen);
    ui_touch_test_set_back_target(s_touch_submenu_screen);

    refresh_tile_labels();

    return scr;
}
