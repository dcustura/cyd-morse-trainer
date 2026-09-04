idf_component_register(SRCS "main.c"
                             "display_init.c"
                       INCLUDE_DIRS "."
                       REQUIRES example_component
                                driver
                                esp_lcd
                                esp_lcd_ili9341
                                esp_lvgl_port
                                lvgl)
