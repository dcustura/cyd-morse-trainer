#include "ui_practice.h"

#include "paddle_input.h"

#include <stdio.h>

#define QUEUE_DRAIN_PERIOD_MS 30

static lv_obj_t *s_text_label;
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

static void drain_queue_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    char ch;
    while (xQueueReceive(s_decoded_char_queue, &ch, 0) == pdTRUE) {
        char text[2] = { ch, '\0' };
        lv_label_ins_text(s_text_label, LV_LABEL_POS_LAST, text);
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
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

    char status_text[48];
    snprintf(status_text, sizeof(status_text), "WPM: %u   Mode: %s",
             (unsigned)initial_wpm, mode_name(initial_mode));
    lv_obj_t *status_label = lv_label_create(scr);
    lv_label_set_text(status_label, status_text);

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

    lv_timer_create(drain_queue_timer_cb, QUEUE_DRAIN_PERIOD_MS, NULL);

    return scr;
}
