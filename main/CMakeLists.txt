idf_component_register(SRCS "cli.c" "cli_mem.c" "main.c"
                             "auto_brightness.c"
                             "display_init.c"
                             "fonts/lv_font_dejavu_mono_16_12.c"
                             "sidetone.c"
                             "memory_store.c"
                             "paddle_input.c"
                             "settings_store.c"
                             "touch_cal_store.c"
                             "ui_calibration.c"
                             "ui_help.c"
                             "ui_memories.c"
                             "ui_menu.c"
                             "ui_on_air.c"
                             "ui_send.c"
                             "ui_settings.c"
                             "ui_touch_test.c"
                       INCLUDE_DIRS "."
                       REQUIRES debouncer
                                iambic_keyer
                                key_gate
                                message_player
                                morse_codec
                                settings
                                touch_calibration
                                console
                                driver
                                esp_adc
                                esp_app_format
                                esp_driver_dac
                                esp_driver_ledc
                                esp_lcd
                                esp_lcd_touch
                                esp_lcd_touch_xpt2046
                                esp_lvgl_port
                                esp_timer
                                lvgl
                                nvs_flash)
