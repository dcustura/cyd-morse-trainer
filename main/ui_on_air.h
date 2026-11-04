#pragma once

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * On-air indication and quick toggles. While on-air (see
 * paddle_input_set_on_air()), a red frame is drawn around every screen.
 * Call once, with the LVGL lock held, before creating the toggles.
 */
void ui_on_air_init(void);

/**
 * Make btn a quick on-air toggle: a tap flips the live on-air state only
 * (the saved setting is left alone), and btn turns red while on-air. If
 * label is non-NULL its text is set to on_text or off_text to match.
 */
void ui_on_air_add_toggle(lv_obj_t *btn, lv_obj_t *label, const char *on_text, const char *off_text);

/**
 * Keep obj's LV_STATE_CHECKED in step with the live on-air state (e.g. a
 * switch whose tap handler is the caller's own).
 */
void ui_on_air_track(lv_obj_t *obj);

/** Re-apply the live on-air state to the frame and every toggle now. */
void ui_on_air_refresh(void);

#ifdef __cplusplus
}
#endif
