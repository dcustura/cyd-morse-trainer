#include "ui_practice.h"

#include "display_init.h"
#include "paddle_input.h"

#include <stdio.h>

#define QUEUE_DRAIN_PERIOD_MS 30

static lv_obj_t *s_status_label;
static lv_obj_t *s_text_label;
static lv_obj_t *s_keying_dot;
static QueueHandle_t s_decoded_char_queue;

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
        char text[2] = { ch, '\0' };
        lv_label_ins_text(s_text_label, LV_LABEL_POS_LAST, text);
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
    lv_label_set_text(s_text_label, "");
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

    s_text_label = lv_label_create(scr);
    lv_label_set_long_mode(s_text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_text_label, LV_PCT(100));
    lv_label_set_text(s_text_label, "");
    lv_obj_set_flex_grow(s_text_label, 1);

    lv_obj_t *btn_row = lv_obj_create(scr);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *clear_btn = lv_button_create(btn_row);
    lv_obj_add_event_cb(clear_btn, clear_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");

    lv_obj_t *back_btn = lv_button_create(btn_row);
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
