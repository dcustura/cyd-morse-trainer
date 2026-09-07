#include "ui_calibration.h"

#include "display_init.h"
#include "touch_cal_store.h"
#include "touch_calibration.h"

#include <stdio.h>

#define POLL_PERIOD_MS 30
#define TARGET_MARGIN 24
#define CROSSHAIR_LEN 24
#define CROSSHAIR_THICK 3
#define BACK_BTN_W 100
#define BACK_BTN_H 40
#define BACK_BTN_MARGIN_BOTTOM 6
#define BACK_HIT_PADDING 16

static lv_obj_t *s_hline;
static lv_obj_t *s_vline;
static lv_obj_t *s_status_label;
static lv_obj_t *s_back_btn;
static lv_obj_t *s_next_screen;
static lv_obj_t *s_cancel_screen;
static lv_timer_t *s_poll_timer;
static touch_calibration_raw_point_t s_samples[TOUCH_CAL_POINT_COUNT];
static uint8_t s_current_index;
static bool s_was_pressed;
static uint16_t s_disp_w;
static uint16_t s_disp_h;

static void target_for_index(uint8_t idx, uint16_t *tx, uint16_t *ty)
{
    switch (idx) {
    case TOUCH_CAL_POINT_TOP_LEFT:
        *tx = TARGET_MARGIN;
        *ty = TARGET_MARGIN;
        break;
    case TOUCH_CAL_POINT_TOP_RIGHT:
        *tx = s_disp_w - 1 - TARGET_MARGIN;
        *ty = TARGET_MARGIN;
        break;
    case TOUCH_CAL_POINT_BOTTOM_LEFT:
        *tx = TARGET_MARGIN;
        *ty = s_disp_h - 1 - TARGET_MARGIN;
        break;
    case TOUCH_CAL_POINT_BOTTOM_RIGHT:
        *tx = s_disp_w - 1 - TARGET_MARGIN;
        *ty = s_disp_h - 1 - TARGET_MARGIN;
        break;
    case TOUCH_CAL_POINT_CENTER:
    default:
        *tx = s_disp_w / 2;
        *ty = s_disp_h / 2;
        break;
    }
}

static void show_target(uint8_t idx, const char *retry_notice)
{
    s_current_index = idx;

    uint16_t tx, ty;
    target_for_index(idx, &tx, &ty);
    lv_obj_set_pos(s_hline, tx - CROSSHAIR_LEN / 2, ty - CROSSHAIR_THICK / 2);
    lv_obj_set_pos(s_vline, tx - CROSSHAIR_THICK / 2, ty - CROSSHAIR_LEN / 2);

    char text[64];
    if (retry_notice != NULL) {
        snprintf(text, sizeof(text), "%s\nTap the crosshair (%d/%d)", retry_notice, idx + 1,
                 TOUCH_CAL_POINT_COUNT);
    } else {
        snprintf(text, sizeof(text), "Tap the crosshair (%d/%d)", idx + 1, TOUCH_CAL_POINT_COUNT);
    }
    lv_label_set_text(s_status_label, text);
}

/*
 * The Back button's target area, padded generously beyond its visual
 * bounds. Raw mode (see on_screen_loaded) makes LVGL's own click detection
 * for on-screen controls unusable while this screen is active - it reports
 * every touch, including taps on Back, as an unscaled raw sample rather
 * than a screen coordinate - so Back has to be hit-tested here instead,
 * against a best-effort screen position (display_touch_map_raw_to_screen)
 * computed from whatever calibration was last applied.
 */
static void back_btn_area(lv_area_t *area)
{
    uint16_t btn_x = (s_disp_w - BACK_BTN_W) / 2;
    uint16_t btn_y = s_disp_h - BACK_BTN_H - BACK_BTN_MARGIN_BOTTOM;
    area->x1 = btn_x - BACK_HIT_PADDING;
    area->x2 = btn_x + BACK_BTN_W + BACK_HIT_PADDING;
    area->y1 = btn_y - BACK_HIT_PADDING;
    area->y2 = btn_y + BACK_BTN_H + BACK_HIT_PADDING;
}

static bool point_in_back_btn(uint16_t x, uint16_t y)
{
    lv_area_t area;
    back_btn_area(&area);
    return x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2;
}

static void poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    uint16_t x, y;
    bool pressed = display_touch_read_point(&x, &y);

    if (pressed && !s_was_pressed) {
        if (s_cancel_screen != NULL) {
            uint16_t mapped_x, mapped_y;
            display_touch_map_raw_to_screen(x, y, &mapped_x, &mapped_y);
            if (point_in_back_btn(mapped_x, mapped_y)) {
                lv_scr_load(s_cancel_screen);
                s_was_pressed = pressed;
                return;
            }
        }

        s_samples[s_current_index].raw_horiz = x;
        s_samples[s_current_index].raw_vert = y;

        if (s_current_index + 1 < TOUCH_CAL_POINT_COUNT) {
            show_target(s_current_index + 1, NULL);
        } else {
            touch_calibration_t cal;
            if (touch_calibration_compute(s_samples, s_disp_w, s_disp_h, TARGET_MARGIN, &cal)) {
                touch_cal_store_save(&cal);
                display_touch_apply_calibration(&cal);
                lv_scr_load(s_next_screen);
            } else {
                show_target(0, "Calibration imprecise - try again");
            }
        }
    }

    s_was_pressed = pressed;
}

static void on_screen_loaded(lv_event_t *e)
{
    (void)e;
    s_was_pressed = false;
    display_touch_set_raw_mode(true);
    show_target(0, NULL);
    s_poll_timer = lv_timer_create(poll_timer_cb, POLL_PERIOD_MS, NULL);

    if (s_cancel_screen != NULL) {
        lv_obj_remove_flag(s_back_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_back_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void on_screen_unloaded(lv_event_t *e)
{
    (void)e;
    display_touch_set_raw_mode(false);
    if (s_poll_timer != NULL) {
        lv_timer_del(s_poll_timer);
        s_poll_timer = NULL;
    }
}

void ui_calibration_set_cancel_target(lv_obj_t *cancel_screen)
{
    s_cancel_screen = cancel_screen;
}

lv_obj_t *ui_calibration_create(lv_obj_t *next_screen)
{
    s_next_screen = next_screen;
    s_disp_w = lv_display_get_horizontal_resolution(lv_display_get_default());
    s_disp_h = lv_display_get_vertical_resolution(lv_display_get_default());

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_add_event_cb(scr, on_screen_loaded, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(scr, on_screen_unloaded, LV_EVENT_SCREEN_UNLOADED, NULL);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Calibrate Touchscreen");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    s_status_label = lv_label_create(scr);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status_label, s_disp_w - 2 * TARGET_MARGIN);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_white(), 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 28);

    s_hline = lv_obj_create(scr);
    lv_obj_remove_style_all(s_hline);
    lv_obj_set_size(s_hline, CROSSHAIR_LEN, CROSSHAIR_THICK);
    lv_obj_set_style_bg_color(s_hline, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_hline, LV_OPA_COVER, 0);

    s_vline = lv_obj_create(scr);
    lv_obj_remove_style_all(s_vline);
    lv_obj_set_size(s_vline, CROSSHAIR_THICK, CROSSHAIR_LEN);
    lv_obj_set_style_bg_color(s_vline, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_vline, LV_OPA_COVER, 0);

    /* Purely a visual affordance - not LVGL-clickable. Raw mode defeats
     * normal click detection while this screen is active, so Back taps are
     * hit-tested manually in poll_timer_cb() instead (see back_btn_area()). */
    s_back_btn = lv_button_create(scr);
    display_style_button_teal(s_back_btn);
    lv_obj_remove_flag(s_back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_back_btn, BACK_BTN_W, BACK_BTN_H);
    lv_obj_set_pos(s_back_btn, (s_disp_w - BACK_BTN_W) / 2, s_disp_h - BACK_BTN_H - BACK_BTN_MARGIN_BOTTOM);
    lv_obj_add_flag(s_back_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *back_label = lv_label_create(s_back_btn);
    lv_label_set_text(back_label, "Back");

    return scr;
}
