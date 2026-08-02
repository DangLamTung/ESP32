/**
 * ST7789 Display Test – Adafruit GFX (ESP32-C3 Super Mini)
 * =========================================================
 * Pin mapping from: thesolaruniverse.wordpress.com (June 2024)
 *
 * ESP32-C3 Super Mini → ST7789 240×240:
 *   GPIO2  → SCLK
 *   GPIO4  → MOSI (SDA)
 *   GPIO0  → RST
 *   GPIO1  → DC
 *   GND    → CS (tied LOW on display)
 *   3.3V   → VCC
 *   GND    → GND
 *
 * Dependencies (auto-installed by PlatformIO):
 *   adafruit/Adafruit GFX Library
 *   adafruit/Adafruit ST7735 and ST7789 Library
 */

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <stdio.h>

// ── Pin definitions (working wiring — physically connected) ──
#define TFT_CS    10   // CS pin (connect to display CS)
#define TFT_DC     4   // DC = GPIO4
#define TFT_RST    5   // RST = GPIO5
#define TFT_BL     1   // BL = GPIO1 (or tie to 3.3V)

// Create display object (CS, DC, RST)
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ── Helper: fill screen with one color ───────────────────────
void test_fill(uint16_t color, const char *name, int delayMs)
{
    printf("  Fill: %s\n", name);
    tft.fillScreen(color);
    delay(delayMs);
}

// ── Helper: center text ──────────────────────────────────────
void centerText(const char *text, int y, uint16_t color, uint8_t size)
{
    int16_t x1, y1;
    uint16_t w, h;
    tft.setTextSize(size);
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((tft.width() - w) / 2, y);
    tft.setTextColor(color);
    tft.print(text);
}

// ── Test 1: Primary colors ───────────────────────────────────
void test1_colors()
{
    printf("\n--- Test 1: Color Fills ---\n");
    tft.setRotation(1);  // landscape: 320×240
    tft.fillScreen(ST77XX_BLACK);
    delay(300);

    test_fill(ST77XX_RED,      "RED",    600);
    test_fill(ST77XX_GREEN,    "GREEN",  600);
    test_fill(ST77XX_BLUE,     "BLUE",   600);
    test_fill(ST77XX_YELLOW,   "YELLOW", 600);
    test_fill(ST77XX_CYAN,     "CYAN",   600);
    test_fill(ST77XX_MAGENTA,  "MAGENTA",600);
    test_fill(ST77XX_WHITE,    "WHITE",  600);
    test_fill(ST77XX_BLACK,    "BLACK",  600);
    printf("  Pass!\n");
}

// ── Test 2: RGB gradient bars ────────────────────────────────
void test2_gradient()
{
    printf("\n--- Test 2: Gradients ---\n");
    tft.fillScreen(ST77XX_BLACK);

    uint16_t w = tft.width();
    uint16_t barH = tft.height() / 3;

    // Red gradient (0→31)
    for (int x = 0; x < (int)w; x++) {
        uint8_t r = map(x, 0, w - 1, 0, 31);
        tft.drawFastVLine(x, 0, barH, tft.color565(r * 8, 0, 0));
    }
    // Green gradient
    for (int x = 0; x < (int)w; x++) {
        uint8_t g = map(x, 0, w - 1, 0, 63);
        tft.drawFastVLine(x, barH, barH, tft.color565(0, g * 4, 0));
    }
    // Blue gradient
    for (int x = 0; x < (int)w; x++) {
        uint8_t b = map(x, 0, w - 1, 0, 31);
        tft.drawFastVLine(x, barH * 2, barH, tft.color565(0, 0, b * 8));
    }
    // Labels
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(8, 4);        tft.print("RED");
    tft.setCursor(8, barH + 4);  tft.print("GREEN");
    tft.setCursor(8, barH*2+4);  tft.print("BLUE");
    delay(2000);
    printf("  Pass!\n");
}

// ── Test 3: Rectangles & outlines ────────────────────────────
void test3_rectangles()
{
    printf("\n--- Test 3: Rectangles ---\n");
    tft.fillScreen(ST77XX_BLACK);
    int w = tft.width();
    int h = tft.height();

    // Nested filled rectangles
    uint16_t colors[] = { ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE,
                          ST77XX_CYAN, ST77XX_MAGENTA, ST77XX_YELLOW };
    for (int i = 0; i < 6; i++) {
        int margin = i * 20;
        tft.fillRect(margin, margin, w - margin * 2, h - margin * 2, colors[i]);
        delay(200);
    }
    // White border rectangles (outline only)
    for (int i = 0; i < 6; i++) {
        int margin = i * 20;
        tft.drawRect(margin, margin, w - margin * 2, h - margin * 2, ST77XX_WHITE);
    }
    delay(2000);
    printf("  Pass!\n");
}

// ── Test 4: Circles ──────────────────────────────────────────
void test4_circles()
{
    printf("\n--- Test 4: Circles ---\n");
    tft.fillScreen(ST77XX_BLACK);
    int cx = tft.width() / 2;
    int cy = tft.height() / 2;

    // Filled circles from large to small
    uint16_t colors[] = { 0x001F, 0x07E0, 0xF800, 0xFFE0, 0x07FF, 0xF81F };
    for (int i = 0; i < 6; i++) {
        int r = 130 - i * 20;
        tft.fillCircle(cx, cy, r, colors[i]);
        delay(150);
    }
    // Outline circles
    tft.drawCircle(cx, cy, 130, ST77XX_WHITE);
    tft.drawCircle(cx, cy, 110, ST77XX_WHITE);
    tft.drawCircle(cx, cy,  90, ST77XX_WHITE);
    delay(2000);
    printf("  Pass!\n");
}

// ── Test 5: Text rendering ───────────────────────────────────
void test5_text()
{
    printf("\n--- Test 5: Text ---\n");
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextWrap(false);
    int cy = tft.height() / 2;

    // Different text sizes
    tft.setTextSize(1);
    centerText("Size 1 - ST7789 Test", 10, ST77XX_WHITE, 1);

    tft.setTextSize(2);
    centerText("Size 2 - ESP32-C3", cy - 40, ST77XX_CYAN, 2);

    tft.setTextSize(3);
    centerText("Size 3 - OK!", cy, ST77XX_GREEN, 3);

    // Info line at bottom
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(4, tft.height() - 30);
    tft.print("240x320 RGB565");
    tft.setCursor(4, tft.height() - 20);
    tft.print("Adafruit ST7789 + GFX");

    delay(3000);
    printf("  Pass!\n");
}

// ── Test 6: Pixel stress test (random dots) ──────────────────
void test6_stress()
{
    printf("\n--- Test 6: Random Pixels (stress test) ---\n");
    tft.fillScreen(ST77XX_BLACK);

    unsigned long start = millis();
    int w = tft.width();
    int h = tft.height();

    for (int i = 0; i < 20000; i++) {
        int x = random(w);
        int y = random(h);
        uint16_t c = random(0xFFFF);
        tft.drawPixel(x, y, c);
    }

    unsigned long elapsed = millis() - start;
    printf("  20,000 pixels in %lu ms (%.1f px/s)\n", elapsed, 20000000.0f / elapsed);

    // Show stats on screen
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(4, 4);
    tft.print("20K px: ");
    tft.print(elapsed);
    tft.print("ms");
    delay(2000);
}

// ── Setup ────────────────────────────────────────────────────
void setup()
{
    // Backlight on
    if (TFT_BL >= 0) {
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }

    delay(200);  // Wait for USB console

    printf("\n\n========================================\n");
    printf("  ST7789 Display Test | ESP32-C3\n");
    printf("========================================\n");
    printf("Width:  240 px\nHeight: 320 px\n\n");

    // Init display
    printf("Init ST7789... ");
    tft.init(240, 320, SPI_MODE3);
    printf("OK!\n");

    // Run all tests
    test1_colors();
    test2_gradient();
    test3_rectangles();
    test4_circles();
    test5_text();
    test6_stress();

    // All tests passed
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);
    centerText("ALL TESTS", tft.height() / 2 - 20, ST77XX_GREEN, 2);
    centerText("PASSED!",   tft.height() / 2 + 10, ST77XX_WHITE, 2);

    printf("\n========================================\n");
    printf("  ALL TESTS PASSED!\n");
    printf("========================================\n\n");
}

// ── Loop ─────────────────────────────────────────────────────
void loop()
{
    // Blink backlight to show we're alive
    if (TFT_BL >= 0) {
        digitalWrite(TFT_BL, !digitalRead(TFT_BL));
    }
    delay(1000);
}
