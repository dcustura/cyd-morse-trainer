#include "ui_touch_test.h"

#define DOT_SIZE 6
#define MIN_DOT_SPACING 4

static lv_obj_t *s_draw_area;
static lv_obj_t *s_back_screen;
static lv_point_t s_last_dot;
static bool s_has_last_dot;

static void draw_dot(lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *dot = lv_obj_create(s_draw_area);
    lv_obj_remove_style_all(dot);
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
    on_touch_point(e);
}

static void clear_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_clean(s_draw_area);
    s_has_last_dot = false;
}

static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_back_screen != NULL) {
        lv_scr_load(s_back_screen);
    }
}

void ui_touch_test_set_back_target(lv_obj_t *back_screen)
{
    s_back_screen = back_screen;
}

lv_obj_t *ui_touch_test_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Draw anywhere to check touch accuracy");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    s_draw_area = lv_obj_create(scr);
    lv_obj_remove_style_all(s_draw_area);
    lv_obj_remove_flag(s_draw_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_draw_area, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_draw_area, LV_OPA_COVER, 0);
    lv_obj_set_width(s_draw_area, LV_PCT(100));
    lv_obj_set_flex_grow(s_draw_area, 1);
    lv_obj_add_event_cb(s_draw_area, on_pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_draw_area, on_touch_point, LV_EVENT_PRESSING, NULL);

    lv_obj_t *btn_row = lv_obj_create(scr);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *clear_btn = lv_button_create(btn_row);
    lv_obj_add_event_cb(clear_btn, clear_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");

    lv_obj_t *back_btn = lv_button_create(btn_row);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");

    return scr;
}
