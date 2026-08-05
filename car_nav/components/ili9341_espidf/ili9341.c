/**
 * @file ili9341.c
 *
 * ILI9341 display controller driver — ported from the vendor's
 * lvgl_esp32_drivers component and upgraded for ESP-IDF v6 / LVGL 9.3:
 *   - Kconfig pin defines  -> board_config.h
 *   - LVGL v8 flush (lv_disp_drv_t / lv_color_t) -> LVGL v9
 *     (lv_display_t / uint8_t *px_map)
 *   - esp_rom_gpio_pad_select_gpio() -> gpio_config()
 *   - SPI bus + DMA pool + backlight setup folded into ili9341_init()
 */

/*********************
 *      INCLUDES
 *********************/
#include "ili9341.h"
#include "disp_spi.h"
#include "esp_lcd_backlight.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "ILI9341"

/**********************
 *      TYPEDEFS
 **********************/

/*The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct. */
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void ili9341_set_orientation(uint8_t orientation);

static void ili9341_send_cmd(uint8_t cmd);
static void ili9341_send_data(void * data, uint16_t length);
static void ili9341_send_color(void * data, uint16_t length);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

esp_err_t ili9341_init(void)
{
    /* ── GPIO: DC + RST ── */
    gpio_config_t io = {
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pin_bit_mask = (1ULL << ILI9341_DC),
    };
    gpio_config(&io);

#if ILI9341_USE_RST
    io.pin_bit_mask = (1ULL << ILI9341_RST);
    gpio_config(&io);

    /* Reset the display */
    gpio_set_level(ILI9341_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(ILI9341_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
#endif

    /* ── SPI bus (write-only display, half-duplex, DMA) ── */
    spi_bus_config_t bus = {
        .mosi_io_num     = DISP_SPI_MOSI,
        .miso_io_num     = -1,               /* write-only display */
        .sclk_io_num     = DISP_SPI_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = SPI_BUS_MAX_TRANSFER_SZ,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TFT_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    /* Add the SPI device and create the DMA transaction pool */
    disp_spi_add_device(TFT_SPI_HOST);

    ESP_LOGI(TAG, "Initialization.");

    lcd_init_cmd_t ili_init_cmds[]={
        {0xCF, {0x00, 0xC1, 0X30}, 3},
        {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
        {0xE8, {0x85, 0x00, 0x78}, 3},
        {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
        {0xF7, {0x20}, 1},
        {0xEA, {0x00, 0x00}, 2},
        {0xC0, {0x13}, 1},          /*Power control*/
        {0xC1, {0x13}, 1},          /*Power control */
        {0xC5, {0x22, 0x35}, 2},    /*VCOM control*/
        {0xC7, {0xBD}, 1},          /*VCOM control*/
        {0x21, {0}, 0},
        {0x36, {0x08}, 1},          /*Memory Access Control (overridden below)*/
        {0xB6, {0x0A, 0x82}, 2},
        {0x3A, {0x55}, 1},          /*Pixel Format Set*/
        {0xF6, {0x01, 0x30}, 2},
        {0xB1, {0x00, 0x1B}, 2},
        {0xF2, {0x00}, 1},
        {0x26, {0x01}, 1},
        {0xE0, {0x0F, 0x35, 0x31, 0x0B, 0x0F, 0x06, 0x49, 0XA7, 0x33, 0x07, 0x0F, 0x03, 0x0C, 0x0A, 0x00}, 15},
        {0XE1, {0x00, 0x0A, 0x0F, 0x04, 0x11, 0x08, 0x36, 0x58, 0x4D, 0x07, 0x10, 0x0C, 0x32, 0x34, 0x0F}, 15},
        {0x11, {0}, 0x80},
        {0x29, {0}, 0x80},
        {0, {0}, 0xff},
    };

    //Send all the commands
    uint16_t cmd = 0;
    while (ili_init_cmds[cmd].databytes!=0xff) {
        ili9341_send_cmd(ili_init_cmds[cmd].cmd);
        ili9341_send_data(ili_init_cmds[cmd].data, ili_init_cmds[cmd].databytes&0x1F);
        if (ili_init_cmds[cmd].databytes & 0x80) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        cmd++;
    }

    ili9341_set_orientation(2); /* LANDSCAPE */

#if ILI9341_INVERT_COLORS == 1
    ili9341_send_cmd(0x21);
#else
    ili9341_send_cmd(0x20);
#endif

    /* ── Backlight (switch mode, active high) ── */
    const disp_backlight_config_t bckl_config = {
        .gpio_num = BOARD_DISP_BL,
        .pwm_control = false,
        .output_invert = (BOARD_DISP_BL_ACTIVE_HIGH ? false : true),
        .timer_idx = 0,
        .channel_idx = 0
    };
    disp_backlight_h bckl_handle = disp_backlight_new(&bckl_config);
    disp_backlight_set(bckl_handle, 100);

    ESP_LOGI(TAG, "ILI9341 %dx%d @%luMHz landscape, SPI%d DMA",
             ILI9341_WIDTH, ILI9341_HEIGHT,
             (unsigned long)(SPI_TFT_CLOCK_SPEED_HZ / 1000000),
             TFT_SPI_HOST);
    return ESP_OK;
}

void ili9341_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint8_t data[4];

    /*Column addresses*/
    ili9341_send_cmd(0x2A);
    data[0] = (area->x1 >> 8) & 0xFF;
    data[1] = area->x1 & 0xFF;
    data[2] = (area->x2 >> 8) & 0xFF;
    data[3] = area->x2 & 0xFF;
    ili9341_send_data(data, 4);

    /*Page addresses*/
    ili9341_send_cmd(0x2B);
    data[0] = (area->y1 >> 8) & 0xFF;
    data[1] = area->y1 & 0xFF;
    data[2] = (area->y2 >> 8) & 0xFF;
    data[3] = area->y2 & 0xFF;
    ili9341_send_data(data, 4);

    /*Memory write*/
    ili9341_send_cmd(0x2C);
    uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
    disp_spi_send_colors(px_map, size * 2);
}

void ili9341_sleep_in()
{
    uint8_t data[] = {0x08};
    ili9341_send_cmd(0x10);
    ili9341_send_data(&data, 1);
}

void ili9341_sleep_out()
{
    uint8_t data[] = {0x08};
    ili9341_send_cmd(0x11);
    ili9341_send_data(&data, 1);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void ili9341_send_cmd(uint8_t cmd)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ILI9341_DC, 0);	 /*Command mode*/
    disp_spi_send_data(&cmd, 1);
}

static void ili9341_send_data(void * data, uint16_t length)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ILI9341_DC, 1);	 /*Data mode*/
    disp_spi_send_data(data, length);
}

static void ili9341_send_color(void * data, uint16_t length)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ILI9341_DC, 1);   /*Data mode*/
    disp_spi_send_colors(data, length);
}

static void ili9341_set_orientation(uint8_t orientation)
{
    const char *orientation_str[] = {
        "PORTRAIT", "PORTRAIT_INVERTED", "LANDSCAPE", "LANDSCAPE_INVERTED"
    };
    ESP_LOGI(TAG, "Display orientation: %s", orientation_str[orientation]);

    /* vendor LANDSCAPE table for this board (CONFIG_LV_PREDEFINED_DISPLAY_NONE) */
    uint8_t data[] = {0x48, 0x88, 0x28, 0xE8};

    ESP_LOGI(TAG, "0x36 command value: 0x%02X", data[orientation]);

    ili9341_send_cmd(0x36);
    ili9341_send_data((void *) &data[orientation], 1);
}
