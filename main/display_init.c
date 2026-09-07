#include "display_init.h"

#include "board_pins.h"

#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_xpt2046.h"
#include "esp_lvgl_port.h"

static const char *TAG = "display_init";

#define LCD_H_RES 320
#define LCD_V_RES 240

/* Partial draw buffer sized to fit internal SRAM (no PSRAM assumed). */
#define LVGL_DRAW_BUFF_LINES (LCD_V_RES / 10)

/* Confirmed on real hardware: despite this board being universally marketed
 * as shipping an ILI9341, this specific unit's TFT controller identifies
 * (and only renders correctly) as an ST7789 - the ILI9341 driver produced a
 * fully garbled/noisy screen regardless of SPI clock or reset handling.
 * Community reports confirm this board ships with ILI9341, ILI9342, or
 * ST7789 controllers interchangeably across production batches, visually
 * indistinguishable. If a future unit needs ILI9341 again, swap
 * esp_lcd_new_panel_st7789() below for esp_lcd_new_panel_ili9341() (from
 * the espressif/esp_lcd_ili9341 managed component) - the rest of the SPI
 * bus/IO setup is vendor-agnostic. */
#define LCD_SWAP_XY   true
#define LCD_MIRROR_X  true
#define LCD_MIRROR_Y  false
#define LCD_RGB_ORDER LCD_RGB_ELEMENT_ORDER_BGR
#define LCD_INVERT_COLOR true

static esp_lcd_panel_io_handle_t s_tft_io_handle;
static esp_lcd_panel_handle_t s_tft_panel_handle;
static esp_lcd_panel_io_handle_t s_touch_io_handle;
static esp_lcd_touch_handle_t s_touch_handle;

static void init_backlight(void)
{
    gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << BOARD_TFT_BL_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));
    gpio_set_level(BOARD_TFT_BL_GPIO, 1);
}

static void init_tft_panel(void)
{
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = BOARD_TFT_SCLK_GPIO,
        .mosi_io_num = BOARD_TFT_MOSI_GPIO,
        .miso_io_num = BOARD_TFT_MISO_GPIO, /* not used by normal operation; wired for diagnostics */
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LVGL_DRAW_BUFF_LINES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = BOARD_TFT_CS_GPIO,
        .dc_gpio_num = BOARD_TFT_DC_GPIO,
        .spi_mode = 0,
        /* 40MHz (this panel's nominal max) produced garbled/noisy output on
         * this unit; 20MHz is a confirmed-working fallback. */
        .pclk_hz = 20 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &s_tft_io_handle));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_TFT_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ORDER,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_tft_io_handle, &panel_cfg, &s_tft_panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_tft_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_tft_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_tft_panel_handle, LCD_INVERT_COLOR));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_tft_panel_handle, LCD_SWAP_XY));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_tft_panel_handle, LCD_MIRROR_X, LCD_MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_tft_panel_handle, true));

    ESP_LOGI(TAG, "TFT panel initialized");
}

/* The atanisoft XPT2046 driver stores esp_lcd_touch_config_t.flags but never
 * reads swap_xy/mirror_x/mirror_y from it - those settings are silently
 * ignored. Empirically calibrated on real hardware instead (6-point sweep,
 * touch_cfg.x_max=LCD_H_RES=320, y_max=LCD_V_RES=240 unchanged): the
 * driver's raw "x" output (from the X_POSITION register) actually tracks
 * the display's VERTICAL position (~31 at physical top, ~283 at physical
 * bottom, barely moving left-right); its raw "y" output (Y_POSITION)
 * tracks the display's HORIZONTAL position (~17 at physical left, ~217 at
 * physical right, barely moving top-bottom). The axes are swapped end to
 * end, and neither spans the full theoretical ADC range. Fix both via the
 * process_coordinates hook, which the esp_lcd_touch base layer *does*
 * invoke (see esp_lcd_touch_get_data in esp_lcd_touch.c). Verified against
 * all four corners plus top/bottom midpoints: exact (0,0)/(319,0)/(0,239)/
 * (319,239) at the corners, ~160 at the midpoints. */
#define TOUCH_CAL_HORIZ_MIN 17
#define TOUCH_CAL_HORIZ_MAX 217
#define TOUCH_CAL_VERT_MIN 31
#define TOUCH_CAL_VERT_MAX 283

static void touch_process_coordinates(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                                       uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    (void)tp;
    (void)strength;
    (void)max_point_num;

    for (uint8_t i = 0; i < *point_num; i++) {
        int32_t raw_vertical = x[i];   /* driver's "x" actually tracks screen Y */
        int32_t raw_horizontal = y[i]; /* driver's "y" actually tracks screen X */

        int32_t new_x = (raw_horizontal - TOUCH_CAL_HORIZ_MIN) * (LCD_H_RES - 1)
                         / (TOUCH_CAL_HORIZ_MAX - TOUCH_CAL_HORIZ_MIN);
        int32_t new_y = (raw_vertical - TOUCH_CAL_VERT_MIN) * (LCD_V_RES - 1)
                         / (TOUCH_CAL_VERT_MAX - TOUCH_CAL_VERT_MIN);

        if (new_x < 0) {
            new_x = 0;
        } else if (new_x > LCD_H_RES - 1) {
            new_x = LCD_H_RES - 1;
        }
        if (new_y < 0) {
            new_y = 0;
        } else if (new_y > LCD_V_RES - 1) {
            new_y = LCD_V_RES - 1;
        }

        x[i] = (uint16_t)new_x;
        y[i] = (uint16_t)new_y;
    }
}

static void init_touch(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BOARD_TOUCH_MOSI_GPIO,
        .miso_io_num = BOARD_TOUCH_MISO_GPIO,
        .sclk_io_num = BOARD_TOUCH_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_DISABLED));

    esp_lcd_panel_io_spi_config_t io_cfg = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(BOARD_TOUCH_CS_GPIO);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_cfg, &s_touch_io_handle));

    esp_lcd_touch_config_t touch_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        /* Interrupt mode confirmed broken on this unit: esp_lvgl_port
         * switches to event-driven touch reads whenever int_gpio_num isn't
         * GPIO_NUM_NC, and it only reads the controller when that interrupt
         * fires - if the IRQ line isn't behaving as expected, touch reads
         * never happen at all. Polling (GPIO_NUM_NC here) works reliably. */
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .process_coordinates = touch_process_coordinates,
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(s_touch_io_handle, &touch_cfg, &s_touch_handle));

    ESP_LOGI(TAG, "Touch controller initialized");
}

lv_display_t *display_init(void)
{
    init_backlight();
    init_tft_panel();
    init_touch();

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_tft_io_handle,
        .panel_handle = s_tft_panel_handle,
        .control_handle = NULL,
        .buffer_size = LCD_H_RES * LVGL_DRAW_BUFF_LINES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = LCD_SWAP_XY,
            .mirror_x = LCD_MIRROR_X,
            .mirror_y = LCD_MIRROR_Y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .swap_bytes = 1,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return NULL;
    }

    const lvgl_port_touch_cfg_t touch_port_cfg = {
        .disp = disp,
        .handle = s_touch_handle,
    };
    if (lvgl_port_add_touch(&touch_port_cfg) == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_touch failed");
        return NULL;
    }

    ESP_LOGI(TAG, "LVGL display + touch ready");

    return disp;
}
