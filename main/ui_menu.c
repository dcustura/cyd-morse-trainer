#include "ui_menu.h"

#include "display_init.h"

#define MENU_BTN_WIDTH 140
#define MENU_BTN_HEIGHT 60

static void nav_btn_cb(lv_event_t *e)
{
    lv_obj_t *target_screen = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(target_screen);
}

static lv_obj_t *create_menu_button(lv_obj_t *parent, const char *text, lv_obj_t *target_screen)
{
    lv_obj_t *btn = lv_button_create(parent);
    display_style_tile(btn);
    lv_obj_set_size(btn, MENU_BTN_WIDTH, MENU_BTN_HEIGHT);
    lv_obj_add_event_cb(btn, nav_btn_cb, LV_EVENT_CLICKED, target_screen);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, display_compensate_color(lv_color_white()), 0);
    lv_obj_center(label);

    return btn;
}

void ui_menu_populate(lv_obj_t *menu_screen, lv_obj_t *practice_screen, lv_obj_t *settings_screen)
{
    lv_obj_set_flex_flow(menu_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu_screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(menu_screen, 16, 0);

    lv_obj_t *title = lv_label_create(menu_screen);
    lv_label_set_text(title, "Morse Trainer");

    create_menu_button(menu_screen, "Practice", practice_screen);
    create_menu_button(menu_screen, "Settings", settings_screen);
}
