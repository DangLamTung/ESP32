/**
 * @file ili9341.h
 *
 * ILI9341 display controller driver — ported from the vendor's
 * lvgl_esp32_drivers component and upgraded for ESP-IDF v6 / LVGL 9.3.
 */
#ifndef ILI9341_H
#define ILI9341_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "lvgl.h"
#include "board_config.h"

/*********************
 *      DEFINES
 *********************/
#define ILI9341_DC             BOARD_DISP_DC
#define ILI9341_USE_RST        (BOARD_DISP_RST >= 0)
#define ILI9341_RST            BOARD_DISP_RST
#define ILI9341_INVERT_COLORS  1

/* Panel dimensions (landscape) */
#define ILI9341_WIDTH          BOARD_DISP_HOR_RES
#define ILI9341_HEIGHT         BOARD_DISP_VER_RES

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/** Initialize SPI bus + DMA pool, ILI9341 controller and backlight. */
esp_err_t ili9341_init(void);

/** LVGL v9 flush callback: set window and queue the pixel area via DMA.
 *  The caller (lvgl_port) byte-swaps the buffer and waits for the DMA. */
void ili9341_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

void ili9341_sleep_in(void);
void ili9341_sleep_out(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*ILI9341_H*/
