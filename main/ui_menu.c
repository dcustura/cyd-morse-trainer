#include "ui_menu.h"

static void practice_btn_cb(lv_event_t *e)
{
    lv_obj_t *practice_screen = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(practice_screen);
}

void ui_menu_populate(lv_obj_t *menu_screen, lv_obj_t *practice_screen)
{
    lv_obj_set_flex_flow(menu_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu_screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(menu_screen);
    lv_label_set_text(title, "Morse Trainer");

    lv_obj_t *practice_btn = lv_button_create(menu_screen);
    lv_obj_add_event_cb(practice_btn, practice_btn_cb, LV_EVENT_CLICKED, practice_screen);
    lv_obj_t *practice_label = lv_label_create(practice_btn);
    lv_label_set_text(practice_label, "Practice");
}
