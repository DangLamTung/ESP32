/**
 * board_config.h — pin / speed / resolution configuration for the
 * 2.8" IPS ESP32-S3 + ILI9341 board (ES3C28P / ES3N28P).
 *
 * Values are taken from the vendor's ESP-IDF LVGL demo (lvgl_spi_conf.h +
 * sdkconfig) but the Kconfig symbols are replaced with plain defines so the
 * driver builds against our ESP-IDF v6 / LVGL 9.3 project.
 */
#pragma once

#include "driver/spi_master.h"
#include "hal/spi_types.h"

/* Display SPI pins — IOMUX-native SPI2 pins on this board */
#define DISP_SPI_MOSI    11
#define DISP_SPI_MISO    13      /* wired but unused (write-only display) */
#define DISP_SPI_CLK     12
#define DISP_SPI_CS      10
#define DISP_SPI_IO2     (-1)
#define DISP_SPI_IO3     (-1)
#define DISP_SPI_INPUT_DELAY_NS  0

#define TFT_SPI_HOST     SPI2_HOST
#define DISP_SPI_HALF_DUPLEX          /* write-only: no MISO phase */

#define SPI_TFT_CLOCK_SPEED_HZ  (40 * 1000 * 1000)  /* 40 MHz; 80 MHz possible */
#define SPI_TFT_SPI_MODE        0                    /* CPOL=0, CPHA=0 */

/* Max transfer = one full 320x240 RGB565 frame */
#define DISP_BUF_SIZE               (320 * 240 * 2)
#define SPI_BUS_MAX_TRANSFER_SZ     DISP_BUF_SIZE

/* Panel geometry (landscape) */
#define BOARD_DISP_HOR_RES  320
#define BOARD_DISP_VER_RES  240

/* ILI9341 control pins (vendor wiring) */
#define BOARD_DISP_DC    46
#define BOARD_DISP_RST   18
#define BOARD_DISP_BL    45          /* backlight, active high */

/* Backlight config (switch mode, active high) */
#define BOARD_DISP_BL_ACTIVE_HIGH  1
