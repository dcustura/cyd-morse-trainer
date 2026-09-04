idf_component_register(SRCS "main.c"
                             "display_init.c"
                             "sidetone.c"
                             "paddle_input.c"
                             "ui_menu.c"
                             "ui_practice.c"
                       INCLUDE_DIRS "."
                       REQUIRES example_component
                                iambic_keyer
                                morse_codec
                                driver
                                esp_driver_ledc
                                esp_lcd
                                esp_lcd_ili9341
                                esp_lcd_touch
                                esp_lcd_touch_xpt2046
                                esp_lvgl_port
                                esp_timer
                                lvgl)
