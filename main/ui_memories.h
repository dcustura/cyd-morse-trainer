#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the message memories panel: six memory tiles (tap to send or
 * queue, long-press to pick the memory), Repeat, Stop and Back tiles, and a
 * one-line sending strip. Back returns to send_screen; playback keeps
 * going.
 */
lv_obj_t *ui_memories_create(lv_obj_t *send_screen);

#ifdef __cplusplus
}
#endif
