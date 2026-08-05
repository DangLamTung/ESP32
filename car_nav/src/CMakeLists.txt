# Car-nav application component (sources in src/).
# Sources are listed explicitly — the framework auto-scans main/ and
# components/ only, so this dir is registered via EXTRA_COMPONENT_DIRS
# in the top-level CMakeLists.txt.

idf_component_register(
    SRCS
        "main.c"
        "map_render_lvgl.c"
        "tilemap.c"
        "lvgl_port.c"
        "ble_server.c"
        "lv_font_viet_20.c"
        "AnimatedGIF.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        ili9341_espidf
        bt
        nvs_flash
        esp_driver_spi
        esp_driver_gpio
        esp_rom
        esp_timer
        lvgl)
