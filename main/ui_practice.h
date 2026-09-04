#pragma once

#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "iambic_keyer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the Practice screen: a live Morse-decode text area, a status
 * label, and Clear/Back buttons. Must be called while holding the LVGL
 * lock (lvgl_port_lock). Starts an lv_timer that drains decoded_char_queue
 * (populated by paddle_input.c) and appends each character to the text area.
 */
lv_obj_t *ui_practice_create(QueueHandle_t decoded_char_queue, lv_obj_t *menu_screen,
                              iambic_keyer_mode_t initial_mode, uint16_t initial_wpm);

#ifdef __cplusplus
}
#endif
