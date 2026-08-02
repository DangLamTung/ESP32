/**
 * ESP32-C3 ST7789 GIF Player — Per-Line DMA, Dirty Rectangle
 * Only updates changed regions between frames — no full screen clear.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "st7789.h"
#include "AnimatedGIF.h"
#include "cat_gif.h"

static const char *TAG = "gif_dma";
static AnimatedGIF gif;

// Line buffer (198 pixels × 2 bytes = 396 bytes)
static uint16_t line_buf[240];

// ── GIF Draw callback: one line at a time ────────────────────
static void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t  *src = pDraw->pPixels;
    uint16_t *pal = (uint16_t *)pDraw->pPalette;
    int sw = pDraw->iWidth;
    int sh = pDraw->iHeight;
    int offX = (ST7789_WIDTH - sw) / 2;
    int offY = (ST7789_HEIGHT - sh) / 2;
    int sx = offX + pDraw->iX;
    int sy = offY + pDraw->iY + pDraw->y;
    uint8_t  transp  = pDraw->ucTransparent;
    bool     has_tr  = pDraw->ucHasTransparency;

    // Convert this line to RGB565 (byte-swapped for SPI)
    for (int i = 0; i < sw; i++) {
        uint8_t idx = src[i];
        if (has_tr && idx == transp) continue;
        line_buf[i] = (pal[idx] >> 8) | (pal[idx] << 8);
    }

    // DMA this line immediately
    st7789_wait_dma();
    st7789_set_window(sx, sy, sx + sw - 1, sy);
    st7789_write_pixels_dma(line_buf, sw * sizeof(uint16_t));

    esp_task_wdt_reset();
}

// ── Entry point ──────────────────────────────────────────────
extern "C" void app_main(void)
{
    esp_task_wdt_delete(NULL);
    ESP_ERROR_CHECK(st7789_init());

    st7789_fill_screen(0xFFFF); // white background
    gif.begin(LITTLE_ENDIAN_PIXELS);

    uint32_t fc = 0;
    int64_t  fps_timer = esp_timer_get_time();
    float    fps = 0;

    while (1) {
        if (gif.open((uint8_t *)cat_gif, sizeof(cat_gif), GIFDraw)) {
            while (gif.playFrame(false, NULL)) {
                fc++;
                int64_t now = esp_timer_get_time();
                if (now - fps_timer >= 2000000) { // every 2 seconds
                    fps = fc * 1000000.0f / (now - fps_timer);
                    fc = 0; fps_timer = now;
                    ESP_LOGI(TAG, "GIF FPS: %.1f", fps);

                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.1f fps", fps);
                    st7789_draw_string(4, 4, buf, 0x0000, 0xFFFF);
                }
            }
            gif.close();
        } else {
            ESP_LOGE(TAG, "GIF open failed");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}
