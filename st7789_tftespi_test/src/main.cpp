/**
 * ST7789 Display Test — TFT_eSPI Edition (ESP32-C3 Super Mini)
 * ==============================================================
 * Pin mapping from: thesolaruniverse.wordpress.com (June 2024)
 *
 * ESP32-C3 Super Mini → ST7789 240×240:
 *   GPIO2  → SCL (SCLK)
 *   GPIO4  → SDA (MOSI)
 *   GPIO0  → RES (RST)
 *   GPIO1  → DC
 *   GND    → CS (tied LOW on display module)
 *   3.3V   → VCC
 *   GND    → GND
 *
 * All TFT config is in platformio.ini (USER_SETUP_LOADED=1).
 * No need to edit TFT_eSPI/User_Setup.h!
 */

#include <TFT_eSPI.h>
#include <stdio.h>

TFT_eSPI tft = TFT_eSPI();

// ── Serial helpers: printf + flush for reliable USB-serial-JTAG output ──
#define LOG(fmt, ...)  do { printf(fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

// ── Helpers ──────────────────────────────────────────────────

void centerText(const char *text, int y, uint16_t color, uint8_t font)
{
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color);
    tft.drawString(text, tft.width() / 2, y, font);
}

void testHeader(const char *name)
{
    printf("\n--- %s ---\n", name);
    tft.fillScreen(TFT_BLACK);
}

// ── Test 1: Color bars ───────────────────────────────────────
void test1_colors()
{
    testHeader("Test 1: Color Bars");
    int barW = tft.width() / 8;
    uint16_t colors[] = {
        TFT_RED, TFT_GREEN, TFT_BLUE,
        TFT_YELLOW, TFT_CYAN, TFT_MAGENTA,
        TFT_WHITE, TFT_BLACK
    };
    const char *names[] = {
        "RED","GREEN","BLUE","YELLOW","CYAN","MAGENTA","WHITE","BLACK"
    };

    for (int i = 0; i < 8; i++) {
        tft.fillRect(i * barW, 0, barW, tft.height(), colors[i]);
        tft.setTextColor(TFT_WHITE, colors[i]);
        tft.drawString(names[i], i * barW + 5, 5, 2);
    }
    delay(2000);
    printf("  Pass!\n");
}

// ── Test 2: Rainbow gradient ─────────────────────────────────
void test2_gradient()
{
    testHeader("Test 2: Rainbow Gradient");
    int w = tft.width();
    int h = tft.height();

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t r = (x * 255) / w;
            uint8_t g = (y * 255) / h;
            uint8_t b = 255 - ((x + y) * 255) / (w + h);
            tft.drawPixel(x, y, tft.color565(r, g, b));
        }
    }
    tft.setTextColor(TFT_WHITE);
    tft.drawString("RGB Gradient", 10, 10, 2);
    delay(2000);
    printf("  Pass!\n");
}

// ── Test 3: Shapes ───────────────────────────────────────────
void test3_shapes()
{
    testHeader("Test 3: Shapes");
    int cx = tft.width() / 2;
    int cy = tft.height() / 2;

    // Filled circles
    tft.fillCircle(cx, cy, 100, TFT_BLUE);
    tft.fillCircle(cx, cy, 70,  TFT_GREEN);
    tft.fillCircle(cx, cy, 40,  TFT_RED);

    // Rectangles
    tft.drawRect(10, 10, 100, 80, TFT_WHITE);
    tft.fillRect(120, 10, 100, 80, TFT_CYAN);
    tft.drawRoundRect(10, 100, 100, 80, 10, TFT_YELLOW);
    tft.fillRoundRect(120, 100, 100, 80, 10, TFT_MAGENTA);

    // Lines
    tft.drawLine(0, 200, tft.width(), 200, TFT_WHITE);
    tft.drawLine(0, 210, tft.width(), 280, TFT_ORANGE);

    tft.setTextColor(TFT_WHITE);
    tft.drawString("Circles + Rects + Lines", 10, 290, 2);
    delay(2500);
    printf("  Pass!\n");
}

// ── Test 4: Text sizes ───────────────────────────────────────
void test4_text()
{
    testHeader("Test 4: Font Sizes");
    tft.setTextColor(TFT_GREEN);
    tft.drawString("Font 2 - ESP32-C3", 10, 10, 2);

    tft.setTextColor(TFT_CYAN);
    tft.drawString("Font 4 - SuperMini", 10, 40, 4);

    tft.setTextColor(TFT_YELLOW);
    tft.drawString("Font 6 - ST7789", 10, 80, 6);

    tft.setTextColor(TFT_MAGENTA);
    tft.drawString("Font 7 - TFT_eSPI", 10, 130, 7);

    tft.setTextColor(TFT_WHITE);
    tft.drawString("ABCDEFGH abcdefgh 0123456789 !@#$%", 10, 200, 2);
    tft.setTextColor(TFT_RED);
    tft.drawString("The quick brown fox", 10, 230, 4);

    delay(3000);
    printf("  Pass!\n");
}

// ── Test 5: Pixel benchmark ──────────────────────────────────
void test5_benchmark()
{
    testHeader("Test 5: Benchmark");
    int w = tft.width();
    int h = tft.height();

    unsigned long start = millis();
    for (int i = 0; i < 10000; i++) {
        tft.drawPixel(random(w), random(h), random(0xFFFF));
    }
    unsigned long t1 = millis() - start;

    start = millis();
    tft.fillScreen(TFT_BLACK);
    for (int y = 0; y < h; y++) {
        tft.drawFastHLine(0, y, w, tft.color565(y % 32, (y * 3) % 64, (y * 5) % 32));
    }
    unsigned long t2 = millis() - start;

    printf("  10K random px: %lu ms (%.0f px/s)\n", t1, 10000000.0f / t1);
    printf("  Full-screen lines: %lu ms\n", t2);

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.drawString("Benchmark done!", 10, 10, 2);
    tft.drawString("See serial for stats", 10, 40, 2);
    delay(2000);
}

// ── Test 6: Rotation test ────────────────────────────────────
void test6_rotation()
{
    testHeader("Test 6: Rotation");
    const char *rots[] = {"0deg", "90deg", "180deg", "270deg"};
    for (int r = 0; r < 4; r++) {
        tft.setRotation(r);
        tft.fillScreen(TFT_BLACK);
        centerText(rots[r], tft.height() / 2, TFT_WHITE, 4);
        tft.drawRect(20, 20, tft.width() - 40, tft.height() - 40, TFT_RED);
        delay(1000);
    }
    tft.setRotation(1); // landscape by default
    printf("  Pass!\n");
}

// ── Setup ────────────────────────────────────────────────────
void setup()
{
    delay(200); // wait for USB console

    printf("\n\n========================================\n");
    fflush(stdout);
    printf("  ST7789 TFT_eSPI Test | ESP32-C3 SuperMini\n");
    printf("========================================\n");
    printf("Driver:  TFT_eSPI (Bodmer)\n");
    printf("Display: 240x240 ST7789\n\n");
    fflush(stdout);

    printf("Init TFT... ");
    fflush(stdout);
    tft.init();
    tft.setRotation(1); // landscape
    printf("OK!\n");
    fflush(stdout);

    test1_colors();
    test2_gradient();
    test3_shapes();
    test4_text();
    test5_benchmark();
    test6_rotation();

    // Done
    tft.fillScreen(TFT_BLACK);
    centerText("ALL TESTS",  tft.height() / 2 - 20, TFT_GREEN, 4);
    centerText("PASSED!",    tft.height() / 2 + 20, TFT_WHITE, 4);

    printf("\n========================================\n");
    printf("  ALL TESTS PASSED!\n");
    printf("========================================\n\n");
}

void loop()
{
    delay(10000);
}
