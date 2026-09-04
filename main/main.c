#include "esp_log.h"
#include "display_init.h"
#include "esp_lvgl_port.h"
#include "paddle_input.h"
#include "settings_store.h"
#include "ui_menu.h"
#include "ui_practice.h"
#include "ui_settings.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define DECODED_CHAR_QUEUE_DEPTH 32

static const char *TAG = "template_project";

void app_main(void)
{
    morse_settings_t settings;
    ESP_ERROR_CHECK(settings_store_init_and_load(&settings));

    lv_display_t *disp = display_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "display_init failed, halting");
        return;
    }

    QueueHandle_t decoded_char_queue = xQueueCreate(DECODED_CHAR_QUEUE_DEPTH, sizeof(char));

    lvgl_port_lock(0);

    lv_obj_t *menu_screen = lv_obj_create(NULL);
    lv_obj_t *practice_screen = ui_practice_create(decoded_char_queue, menu_screen,
                                                    settings.keymode, settings.wpm);
    lv_obj_t *settings_screen = ui_settings_create(menu_screen, settings.keymode, settings.wpm,
                                                    settings.paddle_swap, settings.tone_hz);
    ui_menu_populate(menu_screen, practice_screen, settings_screen);
    lv_scr_load(menu_screen);

    lvgl_port_unlock();

    paddle_input_start(decoded_char_queue, settings.keymode, settings.wpm,
                        settings.paddle_swap, settings.tone_hz);
}
