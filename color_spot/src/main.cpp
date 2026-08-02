// ESP32C3DEV_ST7789_TFT_6-color_spot
// From: thesolaruniverse.wordpress.com — June 4, 2024
// microcontroller: ESP32 C3 Super Mini
// display: 1.3 inch TFT 240*240 pixels ST7789 controller
//
// pin layout (from article):
//   SCL→GPIO2, SDA→GPIO4, RES→GPIO0, DC→GPIO1, BLK→NC, CS→GND

#include <Arduino.h>
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

#define SCOPE   0x3206
#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define RED         0xF800
#define GREEN       0x07E0
#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define GREY        0x2108
#define ORANGE      0xFBE0
#define AFRICA      0xAB21
#define BORDEAUX    0xA000

int delayTime = 500;
int j, t;

int color[13] = {BORDEAUX, AFRICA, GREEN, BLUE, RED, YELLOW,
                 BORDEAUX, AFRICA, GREEN, BLUE, RED, YELLOW, BORDEAUX};

void drawSpots() {
    tft.fillCircle(120,  40, 8, color[j]);
    tft.fillCircle(190,  80, 8, color[j-1]);
    tft.fillCircle(190, 160, 8, color[j-2]);
    tft.fillCircle(120, 200, 8, color[j-3]);
    tft.fillCircle( 50, 160, 8, color[j-4]);
    tft.fillCircle( 50,  80, 8, color[j-5]);
}

void setup() {
    tft.begin();
    tft.fillScreen(BLACK);
    tft.drawCircle(120, 120, 80, SCOPE);
    j = 12;
}

void loop() {
    drawSpots();
    j--;
    t = j - 5;
    if (j < 6) j = 12;
    delay((t == 6) ? (delayTime / 6) : delayTime);
}
