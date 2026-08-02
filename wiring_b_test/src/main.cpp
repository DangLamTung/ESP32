#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define TFT_CS   10
#define TFT_DC    1
#define TFT_RST   0
#define TFT_MOSI  4
#define TFT_SCLK  2

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void setup() {
    pinMode(8, OUTPUT);

    // Init with full RAM size to avoid library messing with offsets
    tft.init(240, 240, SPI_MODE3);
    tft.setRotation(0);
    tft.fillScreen(0x0000);

    // Draw a grid of small squares to find visible area
    for (int y = 0; y < 240; y += 30) {
        for (int x = 0; x < 240; x += 30) {
            tft.fillRect(x, y, 20, 20, 0x001F);  // blue squares
            delay(50);
        }
    }

    // Now we know the visible area - draw border at visible edges
    tft.fillScreen(0x0000);
    tft.fillCircle(120, 120, 60, 0xFFE0);  // centered
    tft.setTextColor(0xFFFF); tft.setTextSize(2);
    tft.setCursor(20, 100); tft.print("FOUND");
}

void loop() {
    digitalWrite(8, !digitalRead(8));
    delay(1000);
}







