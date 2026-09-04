#include "display_init.h"

#include "board_pins.h"

#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"
#include "esp_lvgl_port.h"

static const char *TAG = "display_init";

#define LCD_H_RES 320
#define LCD_V_RES 240

/* Partial draw buffer sized to fit internal SRAM (no PSRAM assumed). */
#define LVGL_DRAW_BUFF_LINES (LCD_V_RES / 10)

/* CYD units in the field are wired/oriented consistently enough that these
 * are the commonly-reported-correct settings, but display orientation and
 * color order are the two things most likely to need a tweak per unit -
 * if the picture is mirrored/rotated or colors look swapped, adjust these. */
#define LCD_SWAP_XY   true
#define LCD_MIRROR_X  true
#define LCD_MIRROR_Y  false
#define LCD_RGB_ORDER LCD_RGB_ELEMENT_ORDER_BGR
#define LCD_INVERT_COLOR true

static esp_lcd_panel_io_handle_t s_tft_io_handle;
static esp_lcd_panel_handle_t s_tft_panel_handle;

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
    spi_bus_config_t bus_cfg = ILI9341_PANEL_BUS_SPI_CONFIG(
        BOARD_TFT_SCLK_GPIO, BOARD_TFT_MOSI_GPIO,
        LCD_H_RES * LVGL_DRAW_BUFF_LINES * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg =
        ILI9341_PANEL_IO_SPI_CONFIG(BOARD_TFT_CS_GPIO, BOARD_TFT_DC_GPIO, NULL, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &s_tft_io_handle));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_TFT_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ORDER,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(s_tft_io_handle, &panel_cfg, &s_tft_panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_tft_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_tft_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_tft_panel_handle, LCD_INVERT_COLOR));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_tft_panel_handle, LCD_SWAP_XY));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_tft_panel_handle, LCD_MIRROR_X, LCD_MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_tft_panel_handle, true));

    ESP_LOGI(TAG, "TFT panel initialized");
}

lv_display_t *display_init(void)
{
    init_backlight();
    init_tft_panel();

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

    ESP_LOGI(TAG, "LVGL display ready");
    return disp;
}
