#pragma once

/**
 * ESP32-2432S028R ("Cheap Yellow Display") pinout.
 *
 * Community-sourced (Random Nerd Tutorials / espboards.dev); clone boards
 * vary across production batches. Verify against your board's silkscreen
 * before wiring anything to it. All values below are Kconfig-configurable
 * (see main/Kconfig.projbuild) in case your unit differs.
 *
 * | Function                          | GPIO | Bus            |
 * |------------------------------------|------|----------------|
 * | TFT MOSI / MISO / SCLK / CS / DC   | 13 / 12 / 14 / 15 / 2 | SPI2 (HSPI) |
 * | TFT RST                            | not wired (-1); some revisions use GPIO4 |
 * | TFT Backlight                      | 21   | GPIO (on/off)  |
 * | Touch MOSI / MISO / SCLK / CS      | 32 / 39 / 25 / 33     | SPI3 (VSPI) |
 * | Touch IRQ                          | 36   | GPIO, input-only, optional |
 * | Speaker (onboard amp)              | 26   | GPIO / LEDC    |
 * | Paddle DIT (also straight-key in)  | 22   | GPIO, input, pull-up |
 * | Paddle DAH                         | 27   | GPIO, input, pull-up |
 */

#define BOARD_TFT_MOSI_GPIO   CONFIG_MORSE_TFT_MOSI_GPIO
#define BOARD_TFT_MISO_GPIO   CONFIG_MORSE_TFT_MISO_GPIO
#define BOARD_TFT_SCLK_GPIO   CONFIG_MORSE_TFT_SCLK_GPIO
#define BOARD_TFT_CS_GPIO     CONFIG_MORSE_TFT_CS_GPIO
#define BOARD_TFT_DC_GPIO     CONFIG_MORSE_TFT_DC_GPIO
#define BOARD_TFT_RST_GPIO    CONFIG_MORSE_TFT_RST_GPIO
#define BOARD_TFT_BL_GPIO     CONFIG_MORSE_TFT_BL_GPIO

#define BOARD_TOUCH_MOSI_GPIO CONFIG_MORSE_TOUCH_MOSI_GPIO
#define BOARD_TOUCH_MISO_GPIO CONFIG_MORSE_TOUCH_MISO_GPIO
#define BOARD_TOUCH_SCLK_GPIO CONFIG_MORSE_TOUCH_SCLK_GPIO
#define BOARD_TOUCH_CS_GPIO   CONFIG_MORSE_TOUCH_CS_GPIO
#define BOARD_TOUCH_IRQ_GPIO  CONFIG_MORSE_TOUCH_IRQ_GPIO

#define BOARD_SPEAKER_GPIO       CONFIG_MORSE_SPEAKER_GPIO
#define BOARD_PADDLE_DIT_GPIO    CONFIG_MORSE_PADDLE_DIT_GPIO
#define BOARD_PADDLE_DAH_GPIO    CONFIG_MORSE_PADDLE_DAH_GPIO
