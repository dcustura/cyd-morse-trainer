#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Populate an already-created blank screen (lv_obj_create(NULL)) with the
 * main menu's title and navigation buttons. Must be called while holding
 * the LVGL lock (lvgl_port_lock).
 */
void ui_menu_populate(lv_obj_t *menu_screen, lv_obj_t *practice_screen, lv_obj_t *settings_screen);

#ifdef __cplusplus
}
#endif
