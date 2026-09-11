idf_component_register(SRCS "main.c"
                             "display_init.c"
                             "sidetone.c"
                             "paddle_input.c"
                             "settings_store.c"
                             "touch_cal_store.c"
                             "ui_calibration.c"
                             "ui_menu.c"
                             "ui_practice.c"
                             "ui_settings.c"
                             "ui_touch_test.c"
                       INCLUDE_DIRS "."
                       REQUIRES example_component
                                iambic_keyer
                                morse_codec
                                morse_settings
                                touch_calibration
                                driver
                                esp_driver_dac
                                esp_driver_ledc
                                esp_lcd
                                esp_lcd_touch
                                esp_lcd_touch_xpt2046
                                esp_lvgl_port
                                esp_timer
                                lvgl
                                nvs_flash)
