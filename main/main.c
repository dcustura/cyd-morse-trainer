#include "esp_log.h"
#include "display_init.h"
#include "esp_lvgl_port.h"

static const char *TAG = "template_project";

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
    lv_obj_center(label);

    lvgl_port_unlock();
}
