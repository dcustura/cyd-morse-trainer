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
#include "touch_calibration.h"

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

/*
 * LCD_RGB_ORDER=BGR + LCD_INVERT_COLOR=true combine, for this panel, into an
 * end-to-end R/B swap plus per-channel invert on anything drawn through
 * LVGL. Confirmed on hardware: a pure-red lv_obj background rendered as
 * pure yellow. Black/white/grey content (the only colors used before the
 * keying dot in ui_practice.c) is a fixed point of that transform, so it
 * went unnoticed until the first saturated color was added. Use
 * display_compensate_color() below to get a given logical color to actually
 * appear on screen. Don't "fix" this by flipping these two defines without
 * re-testing swap_xy/mirror/touch calibration, which were tuned against the
 * current values.
 */

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
 * ignored. Empirically confirmed on real hardware instead (6-point sweep,
 * touch_cfg.x_max=LCD_H_RES=320, y_max=LCD_V_RES=240 unchanged): the
 * driver's raw "x" output (from the X_POSITION register) actually tracks
 * the display's VERTICAL position; its raw "y" output (Y_POSITION) tracks
 * the display's HORIZONTAL position. This end-to-end axis swap is a fixed
 * property of the driver/hardware combination, not something recalibration
 * should touch - only the min/max raw range per axis (which can vary
 * per-unit and drift) is user-calibrated, via touch_calibration_t below.
 * Fix the swap via the process_coordinates hook, which the esp_lcd_touch
 * base layer *does* invoke (see esp_lcd_touch_get_data in
 * esp_lcd_touch.c). */
static touch_calibration_t s_touch_cal;
static bool s_touch_raw_mode = false;

static void touch_process_coordinates(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                                       uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    (void)tp;
    (void)strength;
    (void)max_point_num;

    for (uint8_t i = 0; i < *point_num; i++) {
        int32_t raw_vertical = x[i];   /* driver's "x" actually tracks screen Y */
        int32_t raw_horizontal = y[i]; /* driver's "y" actually tracks screen X */

        if (s_touch_raw_mode) {
            /* Calibration screen wants the true raw sample, axis-swap only. */
            x[i] = (uint16_t)raw_horizontal;
            y[i] = (uint16_t)raw_vertical;
            continue;
        }

        uint16_t mapped_x, mapped_y;
        touch_calibration_apply(&s_touch_cal, raw_horizontal, raw_vertical, LCD_H_RES, LCD_V_RES,
                                 &mapped_x, &mapped_y);
        x[i] = mapped_x;
        y[i] = mapped_y;
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
    touch_calibration_set_defaults(&s_touch_cal);

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

void display_touch_apply_calibration(const touch_calibration_t *cal)
{
    s_touch_cal = *cal;
}

void display_touch_set_raw_mode(bool enable)
{
    s_touch_raw_mode = enable;
}

bool display_touch_read_point(uint16_t *x, uint16_t *y)
{
    (void)esp_lcd_touch_read_data(s_touch_handle);

    esp_lcd_touch_point_data_t point;
    uint8_t point_cnt = 0;
    esp_err_t err = esp_lcd_touch_get_data(s_touch_handle, &point, &point_cnt, 1);
    if (err != ESP_OK || point_cnt == 0) {
        return false;
    }

    *x = point.x;
    *y = point.y;
    return true;
}

void display_touch_map_raw_to_screen(int32_t raw_horiz, int32_t raw_vert, uint16_t *x, uint16_t *y)
{
    touch_calibration_apply(&s_touch_cal, raw_horiz, raw_vert, LCD_H_RES, LCD_V_RES, x, y);
}

lv_color_t display_compensate_color(lv_color_t c)
{
    /* Swap R/B and invert each channel; this transform is its own inverse. */
    return lv_color_make(255 - c.blue, 255 - c.green, 255 - c.red);
}

void display_style_button_teal(lv_obj_t *btn)
{
    static lv_style_t s_style;
    static bool s_style_inited = false;

    if (!s_style_inited) {
        lv_style_init(&s_style);
        lv_style_set_bg_color(&s_style, display_compensate_color(lv_palette_main(LV_PALETTE_TEAL)));
        s_style_inited = true;
    }
    lv_obj_add_style(btn, &s_style, 0);
}

void display_style_tile(lv_obj_t *tile)
{
    static lv_style_t s_style;
    static bool s_style_inited = false;

    if (!s_style_inited) {
        lv_style_init(&s_style);
        lv_style_set_bg_color(&s_style, display_compensate_color(lv_palette_darken(LV_PALETTE_TEAL, 4)));
        lv_style_set_text_color(&s_style, display_compensate_color(lv_color_white()));
        s_style_inited = true;
    }
    lv_obj_add_style(tile, &s_style, 0);
}

void display_style_button_dismiss(lv_obj_t *btn)
{
    static lv_style_t s_style;
    static bool s_style_inited = false;

    if (!s_style_inited) {
        lv_style_init(&s_style);
        lv_style_set_bg_color(&s_style, display_compensate_color(lv_palette_darken(LV_PALETTE_GREY, 3)));
        lv_style_set_text_color(&s_style, display_compensate_color(lv_color_white()));
        s_style_inited = true;
    }
    lv_obj_add_style(btn, &s_style, 0);
}
