idf_component_register(SRCS "main.c"
                             "auto_brightness.c"
                             "display_init.c"
                             "fonts/lv_font_dejavu_mono_16_12.c"
                             "sidetone.c"
                             "paddle_input.c"
                             "settings_store.c"
                             "touch_cal_store.c"
                             "ui_calibration.c"
                             "ui_help.c"
                             "ui_menu.c"
                             "ui_practice.c"
                             "ui_settings.c"
                             "ui_touch_test.c"
                       INCLUDE_DIRS "."
                       REQUIRES debouncer
                                iambic_keyer
                                morse_codec
                                settings
                                touch_calibration
                                driver
                                esp_adc
                                esp_driver_dac
                                esp_driver_ledc
                                esp_lcd
                                esp_lcd_touch
                                esp_lcd_touch_xpt2046
                                esp_lvgl_port
                                esp_timer
                                lvgl
                                nvs_flash)
