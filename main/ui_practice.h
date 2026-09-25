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
 * initial_large_text selects the decoded-text font at creation (see
 * ui_practice_set_text_size()).
 */
lv_obj_t *ui_practice_create(QueueHandle_t decoded_char_queue, lv_obj_t *menu_screen,
                              iambic_keyer_mode_t initial_mode, uint16_t initial_wpm,
                              bool initial_large_text);

/**
 * Switches the Practice screen's decoded-text font, live, even while the
 * screen isn't the one currently shown: 8px unscii_8 for "Small", or the
 * custom 12x16 anti-aliased monospace font (see main/fonts.h) for "Large".
 */
void ui_practice_set_text_size(bool large_text);

/**
 * The Help screen isn't known yet at creation time (it's created afterwards,
 * since it needs the Practice screen as its own Back target) - call this
 * once it exists, before the Help button can be used.
 */
void ui_practice_set_help_screen(lv_obj_t *help_screen);

/**
 * The Keyer submenu isn't known yet at creation time (it's created by
 * ui_settings_create(), afterwards) - call this once it exists, before the
 * WPM/Mode status label can be tapped. Tapping it sets the Keyer submenu's
 * Back target (see ui_settings_set_keyer_back_target()) to the Practice
 * screen, then navigates there, so its own Back tile returns here instead of
 * to the Settings screen.
 */
void ui_practice_set_keyer_settings_screen(lv_obj_t *keyer_settings_screen);

#ifdef __cplusplus
}
#endif
