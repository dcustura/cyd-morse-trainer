#include "esp_log.h"
#include "display_init.h"
#include "esp_lvgl_port.h"
#include "paddle_input.h"
#include "settings_store.h"
#include "touch_cal_store.h"
#include "touch_calibration.h"
#include "ui_calibration.h"
#include "ui_menu.h"
#include "ui_practice.h"
#include "ui_settings.h"
#include "ui_touch_test.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define DECODED_CHAR_QUEUE_DEPTH 32

static const char *TAG = "morse_trainer";

void app_main(void)
{
    morse_settings_t settings;
    ESP_ERROR_CHECK(settings_store_init_and_load(&settings));

    touch_calibration_t touch_cal;
    bool touch_cal_found = false;
    ESP_ERROR_CHECK(touch_cal_store_load(&touch_cal, &touch_cal_found));

    lv_display_t *disp = display_init(settings.brightness_pct);
    if (disp == NULL) {
        ESP_LOGE(TAG, "display_init failed, halting");
        return;
    }
    if (touch_cal_found) {
        display_touch_apply_calibration(&touch_cal);
    }

    QueueHandle_t decoded_char_queue = xQueueCreate(DECODED_CHAR_QUEUE_DEPTH, sizeof(char));

    lvgl_port_lock(0);

    lv_obj_t *menu_screen = lv_obj_create(NULL);
    lv_obj_t *calibration_screen = ui_calibration_create(menu_screen);
    lv_obj_t *touch_test_screen = ui_touch_test_create();
    lv_obj_t *practice_screen = ui_practice_create(decoded_char_queue, menu_screen,
                                                    settings.keymode, settings.wpm);
    lv_obj_t *settings_screen = ui_settings_create(menu_screen, calibration_screen, touch_test_screen,
                                                    settings.keymode, settings.wpm, settings.paddle_swap,
                                                    settings.tone_hz, settings.volume_pct, settings.envelope_ms,
                                                    settings.brightness_pct);
    ui_menu_populate(menu_screen, practice_screen, settings_screen);
    lv_scr_load(touch_cal_found ? menu_screen : calibration_screen);

    lvgl_port_unlock();

    paddle_input_start(decoded_char_queue, settings.keymode, settings.wpm,
                        settings.paddle_swap, settings.tone_hz, settings.volume_pct, settings.envelope_ms);
}
