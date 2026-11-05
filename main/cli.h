#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Console argv slots. The console's argument splitter fills at most
 * CLI_MAX_ARGS - 1 of them and silently drops any further words, so a
 * command that sees that many arguments can't tell whether input was cut. */
#define CLI_MAX_ARGS 32

/**
 * Start the serial CLI (get/set/save/reset/log commands) on the existing
 * UART console. Must be called once at boot, after every subsystem a
 * "set" command can reach is already initialized: paddle_input_start()
 * (which also brings up the sidetone driver), display_init(),
 * auto_brightness_init(), and the Settings screen (ui_settings_create()).
 */
void cli_start(void);

#ifdef __cplusplus
}
#endif
