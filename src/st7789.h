/**
 * ST7789 SPI TFT Display Driver for ESP-IDF
 * 240×240 resolution, 16-bit RGB565 color
 *
 * SPEED OPTIMIZATIONS:
 *   - IOMUX native pins (GPIO 6/7/10) → up to 80 MHz SPI
 *   - SPI Mode 0 per datasheet (CPOL=0, CPHA=0)
 *   - Half-duplex (write-only, no MISO)
 *   - DMA async pixel transfers
 *   - pre_cb for race-free DC pin control
 */
#pragma once

#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "hal/spi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- Pin mapping: ESP32-C3 Super Mini → ST7789 240×240 ----
// Current wiring (GPIO matrix — max ~40 MHz):
//
//   ST7789   → ESP32-C3 Super Mini
//   ────────────────────────────────
//   GND      → GND
//   VCC      → 3V3
//   SCL/SCLK → GPIO2
//   SDA/MOSI → GPIO4
//   RES/RST  → GPIO0
//   DC       → GPIO1
//   BLK      → NC (not connected)
//   CS       → GPIO10
//
// NOTE: For max speed (80 MHz), rewire to IOMUX native pins:
//   SCLK → GPIO6, MOSI → GPIO7, then change defines below.
//
#define ST7789_SPI_HOST    SPI2_HOST
#define ST7789_PIN_SCLK    6         // IOMUX native
#define ST7789_PIN_MOSI    7         // IOMUX native
#define ST7789_PIN_MISO    -1        // Not used (write-only)
#define ST7789_PIN_CS      10        // IOMUX native CS0
#define ST7789_PIN_DC      1         // Data/Command
#define ST7789_PIN_RST     0         // Reset
#define ST7789_PIN_BL      -1        // Backlight not connected

// Display dimensions (240×198 landscape panel)
#define ST7789_WIDTH       240
#define ST7789_HEIGHT      198

// ---- Public API ----

/** Initialize SPI bus, ST7789 controller, and turn on display */
esp_err_t st7789_init(void);

/** Set the drawing window (column x0..x1, row y0..y1) */
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/** Blocking pixel write (for small data or when simplicity is needed) */
void st7789_write_pixels(const uint8_t *data, size_t len);

/**
 * Async DMA pixel write – returns immediately, CPU is free.
 * IMPORTANT: data buffer must remain valid until st7789_wait_dma().
 */
void st7789_write_pixels_dma(const void *data, size_t len_bytes);

/** Wait for any pending DMA transfer to complete */
void st7789_wait_dma(void);

/** Fill the entire screen with one RGB565 color */
void st7789_fill_screen(uint16_t color);

/** Draw a full RGB565 image at (x,y); w,h is image dimensions */
void st7789_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       const uint16_t *rgb565);

/** Draw one horizontal line of RGB565 pixels */
void st7789_draw_scanline(uint16_t y, uint16_t x, uint16_t w,
                          const uint16_t *rgb565);

/** Draw a single ASCII character (8x16 font) */
void st7789_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg);

/** Draw a string of text (8x16 font) */
void st7789_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg);

/** Draw a string of text (8x16 font) directly into an in-memory 240x240 RGB565 framebuffer */
void st7789_draw_string_fb(uint16_t *fb, uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg);

/** Set backlight brightness 0..100 (0=off, 100=full) */
void st7789_backlight(uint8_t pct);

/** Reset and sleep the display */
void st7789_deinit(void);

#ifdef __cplusplus
}
#endif
