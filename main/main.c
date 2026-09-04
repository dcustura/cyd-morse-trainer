#include "esp_log.h"
#include "display_init.h"
#include "esp_lvgl_port.h"

static const char *TAG = "template_project";

/* Throwaway touch smoke test: toggles the label on tap. Replaced by the
 * real menu screen once US6 wires up ui_menu.c. */
static void toggle_button_cb(lv_event_t *e)
{
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
    static bool toggled = false;
    toggled = !toggled;
    lv_label_set_text(label, toggled ? "Touched!" : "Morse Trainer");
}

void app_main(void)
{
    lv_display_t *disp = display_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "display_init failed, halting");
        return;
    }

    lvgl_port_lock(0);

    lv_obj_t *scr = lv_display_get_screen_active(disp);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Morse Trainer");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn, toggle_button_cb, LV_EVENT_CLICKED, label);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Tap me");
    lv_obj_center(btn_label);

    lvgl_port_unlock();
}
