# ili9341_espidf component — vendor lvgl_esp32_drivers ILI9341 subset,
# upgraded for ESP-IDF v6 + LVGL 9.3.
idf_component_register(
    SRCS
        "ili9341.c"
        "disp_spi.c"
        "esp_lcd_backlight.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        esp_driver_spi
        esp_driver_gpio
        heap
        esp_rom)
