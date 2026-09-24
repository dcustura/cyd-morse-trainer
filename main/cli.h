#pragma once

#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the serial CLI (get/set/save/reset/log commands) on the existing
 * UART console. Must be called once at boot, after every subsystem a
 * "set" command can reach is already initialized: paddle_input_start()
 * (which also brings up the sidetone driver), display_init(),
 * auto_brightness_init(), and the Settings screen (ui_settings_create()).
 */
void cli_start(const settings_t *initial_settings);

#ifdef __cplusplus
}
#endif
