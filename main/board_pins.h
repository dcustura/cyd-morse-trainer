#pragma once

/**
 * ESP32-2432S028R ("Cheap Yellow Display") pinout.
 *
 * GPIO numbers confirmed by flashing real hardware. All values below are
 * Kconfig-configurable (see main/Kconfig.projbuild) in case your unit
 * differs - clone boards vary across production batches. Two things that
 * did NOT match community-sourced expectations on this unit, found during
 * hardware bring-up:
 *   - The TFT controller identifies as ST7789, not ILI9341 (see
 *     display_init.c) - this board ships with either interchangeably.
 *   - The XPT2046 touch axes are swapped, partially inverted, and don't
 *     span the full ADC range - see the calibration in display_init.c's
 *     touch_process_coordinates(). Also, the touch IRQ line doesn't work
 *     reliably on this unit; touch is polled instead (IRQ GPIO = -1).
 *
 * | Function                          | GPIO | Bus            |
 * |------------------------------------|------|----------------|
 * | TFT MOSI / MISO / SCLK / CS / DC   | 13 / 12 / 14 / 15 / 2 | SPI2 (HSPI) |
 * | TFT RST                            | 4    | GPIO, actively driven |
 * | TFT Backlight                      | 21   | GPIO (on/off)  |
 * | Touch MOSI / MISO / SCLK / CS      | 32 / 39 / 25 / 33     | SPI3 (VSPI) |
 * | Touch IRQ                          | -1 (polled; see above) |
 * | Speaker (onboard amp)              | 26   | GPIO / DAC (DAC_CHAN_1) |
 * | Paddle DIT (also straight-key in)  | 22   | GPIO, input, pull-up |
 * | Paddle DAH                         | 27   | GPIO, input, pull-up |
 * | Onboard LDR (ambient light)        | 34   | ADC1_CH6, input only |
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

#define BOARD_LDR_GPIO           CONFIG_MORSE_LDR_GPIO
