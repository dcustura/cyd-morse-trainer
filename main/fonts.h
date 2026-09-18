#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A custom 4bpp (anti-aliased) monospace font, 16px tall with a fixed 12px
 * character cell, generated from DejaVu Sans Mono for the Practice screen's
 * "Large" text size -- see main/fonts/lv_font_dejavu_mono_16_12.c for the
 * exact lv_font_conv invocation, the manual advance-width edit, and license
 * details.
 */
extern const lv_font_t lv_font_dejavu_mono_16_12;

#ifdef __cplusplus
}
#endif
