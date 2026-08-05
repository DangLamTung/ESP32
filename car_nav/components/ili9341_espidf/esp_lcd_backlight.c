/**
 * @file esp_lcd_backlight.c
 *
 * Display backlight controller — ported from the vendor's
 * lvgl_esp32_drivers component. The GPIO (switch) branch was modernized for
 * ESP-IDF v6: esp_rom_gpio_pad_select_gpio()/gpio_matrix_out() replaced by
 * gpio_config().
 */
#include "esp_lcd_backlight.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdlib.h>


typedef struct {
    bool output_invert; // true: backlight ON = low level
    int index;          // GPIO number
} disp_backlight_t;

static const char *TAG = "disp_backlight";

disp_backlight_h disp_backlight_new(const disp_backlight_config_t *config)
{
    // Check input parameters
    if (config == NULL)
        return NULL;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(config->gpio_num)) {
        ESP_LOGW(TAG, "Invalid GPIO number");
        return NULL;
    }
    disp_backlight_t *bckl_dev = calloc(1, sizeof(disp_backlight_t));
    if (bckl_dev == NULL){
        ESP_LOGW(TAG, "Not enough memory");
        return NULL;
    }

    // Configure the backlight pin as a GPIO output (switch mode).
    // LEDC PWM brightness control was dropped in this port: its headers
    // (soc/ledc_periph.h) were removed in ESP-IDF v6, and this board only
    // needs ON/OFF via an active-high GPIO (BOARD_DISP_BL).
    bckl_dev->index = config->gpio_num;
    bckl_dev->output_invert = config->output_invert;
    gpio_config_t io = {
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pin_bit_mask = (1ULL << config->gpio_num),
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    return (disp_backlight_h)bckl_dev;
}

void disp_backlight_set(disp_backlight_h bckl, int brightness_percent)
{
    // Check input paramters
    if (bckl == NULL)
        return;
    if (brightness_percent > 100)
        brightness_percent = 100;
    if (brightness_percent < 0)
        brightness_percent = 0;

    disp_backlight_t *bckl_dev = (disp_backlight_t *) bckl;
    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);

    // Switch (GPIO) backlight: on for any positive value, off at 0.
    int lvl = (brightness_percent > 0) ? 1 : 0;
    if (bckl_dev->output_invert)
        lvl = !lvl;
    ESP_ERROR_CHECK(gpio_set_level(bckl_dev->index, lvl));
}

void disp_backlight_delete(disp_backlight_h bckl)
{
    if (bckl == NULL)
        return;

    disp_backlight_t *bckl_dev = (disp_backlight_t *) bckl;
    gpio_reset_pin(bckl_dev->index);
    free (bckl);
}
