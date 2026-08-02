/**
 * Cat GIF Player — ST7789H2 198×240 — Optimized
 * Uses raw SPI writes for max speed
 */
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <AnimatedGIF.h>
#include "cat_gif.h"

#define TFT_CS   10
#define TFT_DC    1
#define TFT_RST   0
#define TFT_MOSI  7
#define TFT_SCLK  6

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
AnimatedGIF gif;

// Fast scanline write: send raw 16-bit pixels to ST7789 RAM
static void pushLine(uint16_t x, uint16_t y, uint16_t w, uint16_t *buf) {
    tft.startWrite();
    // CASET (column)
    tft.writeCommand(0x2A);
    tft.spiWrite(0); tft.spiWrite(x >> 8); tft.spiWrite(0); tft.spiWrite((x + w - 1) & 0xFF);
    // RASET (row)
    tft.writeCommand(0x2B);
    tft.spiWrite(0); tft.spiWrite(y >> 8); tft.spiWrite(0); tft.spiWrite(y & 0xFF);
    // RAMWR
    tft.writeCommand(0x2C);
    // Send all pixels
    for (int i = 0; i < w; i++) {
        tft.spiWrite(buf[i] >> 8);
        tft.spiWrite(buf[i] & 0xFF);
    }
    tft.endWrite();
}

void GIFDraw(GIFDRAW *pDraw) {
    int x = pDraw->iX;
    int y = pDraw->iY + pDraw->y;
    int w = pDraw->iWidth;
    int h = pDraw->iHeight;
    int offX = (198 - w) / 2;
    int offY = (240 - h) / 2;

    uint8_t *src = pDraw->pPixels;
    uint16_t *pal = (uint16_t *)pDraw->pPalette;

    // Convert palette indices to RGB565
    static uint16_t line[200];
    for (int i = 0; i < w; i++) line[i] = pal[src[i]];

    pushLine(offX + x, offY + y, w, line);
}

void setup() {
    tft.init(198, 240, SPI_MODE3);
    tft.setRotation(0);
    tft.fillScreen(0x0000);

    if (gif.open((uint8_t *)cat_gif, CAT_GIF_SIZE, GIFDraw)) {
        while (gif.playFrame(true, NULL)) {
            static int fc = 0; fc++;
            static unsigned long lt = 0;
            if (millis() - lt > 1000) {
                tft.setTextColor(0xFFFF);
                tft.setCursor(5, 2); tft.print(fc); tft.print("fps");
                fc = 0; lt = millis();
            }
        }
        gif.close();
    } else {
        tft.setTextColor(0xF800); tft.setTextSize(2);
        tft.setCursor(10, 100); tft.print("ERR");
        delay(5000);
    }
}

void loop() { delay(5000); }

