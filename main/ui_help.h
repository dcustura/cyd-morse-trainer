#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the Morse Reference screen: a tab per character category ("A-Z",
 * "0-9", "/?", Prosigns), each a grid of tappable buttons, plus "Back" as a
 * same-sized fifth tab-bar slot that returns to practice_screen instead of
 * switching tabs. Tapping a character button plays its Morse code audibly
 * (dit/dah timing at the trainer's current WPM) and highlights it - no
 * on-screen character/pattern readout, since the screen is meant for
 * learning each code by ear, not by sight. Must be called while holding the
 * LVGL lock (lvgl_port_lock).
 */
lv_obj_t *ui_help_create(lv_obj_t *practice_screen);

#ifdef __cplusplus
}
#endif
