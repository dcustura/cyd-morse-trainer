#include "ui_touch_test.h"

#include <stdio.h>

#define DOT_SIZE 6
#define MIN_DOT_SPACING 4

/* How close to the top/bottom edge a touch must start for it to be
 * considered a candidate Back/Clear swipe rather than ordinary drawing. */
#define EDGE_BAND_PX 24

/* Minimum, mostly-vertical travel for a candidate swipe to actually fire,
 * so a short test stroke started near an edge still draws normally instead
 * of accidentally navigating away or wiping the canvas. */
#define SWIPE_MIN_DELTA_PX 60

static lv_obj_t *s_screen;
static lv_obj_t *s_dot_container;
static lv_obj_t *s_coord_label;
static lv_obj_t *s_back_screen;
static uint16_t s_disp_h;

static lv_point_t s_last_dot;
static bool s_has_last_dot;

static lv_point_t s_gesture_start;
static lv_point_t s_current_point;
static bool s_gesture_start_near_top;
static bool s_gesture_start_near_bottom;

/*
 * s_dot_container exactly overlaps the screen (position (0,0), full
 * display size), so a touch point in the display's absolute coordinate
 * system - what lv_indev_get_point() reports - can be used directly as
 * this child's local position with no translation. Dots are also marked
 * non-clickable so a touch landing on top of one still reaches the screen
 * (base lv_obj instances are clickable by default; labels are not, so only
 * dots need this).
 */
static void draw_dot(lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *dot = lv_obj_create(s_dot_container);
    lv_obj_remove_style_all(dot);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(dot, DOT_SIZE, DOT_SIZE);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_pos(dot, x - DOT_SIZE / 2, y - DOT_SIZE / 2);
}

static void on_touch_point(lv_event_t *e)
{
    (void)e;

    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }

    lv_point_t p;
    lv_indev_get_point(indev, &p);
    s_current_point = p;

    char text[32];
    snprintf(text, sizeof(text), "X: %d  Y: %d", (int)p.x, (int)p.y);
    lv_label_set_text(s_coord_label, text);

    if (s_has_last_dot) {
        int32_t dx = p.x - s_last_dot.x;
        int32_t dy = p.y - s_last_dot.y;
        if (dx * dx + dy * dy < MIN_DOT_SPACING * MIN_DOT_SPACING) {
            return;
        }
    }

    draw_dot(p.x, p.y);
    s_last_dot = p;
    s_has_last_dot = true;
}

static void on_pressed(lv_event_t *e)
{
    s_has_last_dot = false;

    lv_indev_t *indev = lv_indev_active();
    if (indev != NULL) {
        lv_indev_get_point(indev, &s_gesture_start);
    } else {
        s_gesture_start.x = 0;
        s_gesture_start.y = 0;
    }
    s_current_point = s_gesture_start;
    s_gesture_start_near_top = s_gesture_start.y < EDGE_BAND_PX;
    s_gesture_start_near_bottom = s_gesture_start.y > (int32_t)s_disp_h - 1 - EDGE_BAND_PX;

    on_touch_point(e);
}

/*
 * Back and Clear are triggered by swipe direction and distance from the
 * press-down point, not by hitting a fixed-position button. A calibration
 * error distorts absolute position, but it preserves the sign of movement -
 * a drag that's genuinely moving down on the glass still maps to increasing
 * Y even under a fairly badly miscalibrated touchscreen - so this keeps
 * working precisely in the situation this screen exists to diagnose. It
 * still requires starting near an edge (a large, forgiving target - the
 * full screen width by EDGE_BAND_PX tall) so an ordinary test stroke isn't
 * misread as a swipe.
 */
static void on_released(lv_event_t *e)
{
    (void)e;

    int32_t dy = s_current_point.y - s_gesture_start.y;
    int32_t dx = s_current_point.x - s_gesture_start.x;
    int32_t abs_dy = (dy < 0) ? -dy : dy;
    int32_t abs_dx = (dx < 0) ? -dx : dx;
    bool mostly_vertical = abs_dy > 2 * abs_dx;

    if (s_gesture_start_near_top && mostly_vertical && dy > SWIPE_MIN_DELTA_PX) {
        if (s_back_screen != NULL) {
            lv_scr_load(s_back_screen);
        }
        return;
    }

    if (s_gesture_start_near_bottom && mostly_vertical && dy < -SWIPE_MIN_DELTA_PX) {
        lv_obj_clean(s_dot_container);
        s_has_last_dot = false;
    }
}

void ui_touch_test_set_back_target(lv_obj_t *back_screen)
{
    s_back_screen = back_screen;
}

lv_obj_t *ui_touch_test_create(void)
{
    s_disp_h = lv_display_get_vertical_resolution(lv_display_get_default());

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(s_screen, on_pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_screen, on_touch_point, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_screen, on_released, LV_EVENT_RELEASED, NULL);

    s_dot_container = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_dot_container);
    lv_obj_remove_flag(s_dot_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_dot_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_dot_container, 0, 0);
    lv_obj_set_size(s_dot_container, LV_PCT(100), LV_PCT(100));

    /* Non-clickable labels (LVGL's default for lv_label) drawn on top of
     * the canvas don't intercept touch, so they can overlay the full-screen
     * drawing area without leaving any dead zones. */
    lv_obj_t *instructions = lv_label_create(s_screen);
    lv_label_set_text(instructions, "Swipe down from top edge: Back\nSwipe up from bottom edge: Clear");
    lv_obj_set_style_text_color(instructions, lv_color_white(), 0);
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(instructions, LV_ALIGN_TOP_MID, 0, 4);

    s_coord_label = lv_label_create(s_screen);
    lv_label_set_text(s_coord_label, "X: -  Y: -");
    lv_obj_set_style_text_color(s_coord_label, lv_color_white(), 0);
    lv_obj_align(s_coord_label, LV_ALIGN_BOTTOM_MID, 0, -4);

    return s_screen;
}
