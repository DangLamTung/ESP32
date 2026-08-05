/**
 * OSM Tile Viewer with Capacitive-Touch Panning
 * Board: 2.8" IPS ESP32-S3 + ILI9341 (ES3C28P / ES3N28P)
 *
 * Uses CelliesProjects/OpenStreetMap-esp32 (LovyanGFX + PNGdec): OSM tiles are
 * fetched concurrently on both ESP32-S3 cores over a reused TLS connection,
 * cached in PSRAM, and composed into a sprite that is pushed to the ILI9341.
 * The FT6336 capacitive touchscreen pans the map around Bến Thành, HCMC.
 *
 * Display pins  : CS=10 DC=46 RST=(none) MOSI=11 SCLK=12 MISO=13 BL=45, 40MHz SPI
 * Touch pins    : SDA=16 SCL=15 INT=17 RST=18  (FT6336, I2C)
 */
#include <Arduino.h>
#include <math.h>
#include <WiFi.h>
#include <OpenStreetMap-esp32.hpp>
#include "LGFX_ILI9341.h"

// >>> FILL IN YOUR NETWORK <<<
#define WIFI_SSID "Phong 501"
#define WIFI_PASS "0982458888"

LGFX display;
OpenStreetMap osm;
LGFX_Sprite mapSprite(&display);

// View center (Bến Thành, HCMC)
double centerLat = 10.7718;
double centerLon = 106.6982;
const int ZOOM = 15;

// 16 cache slots x 128kB (256px tile) = 2MB PSRAM — plenty for a 320x240 map
// (needs only ~4-9 tiles) plus margin so panning rarely re-fetches.
const int TILE_CACHE_SLOTS = 16;

static bool s_mapDirty = true;

// ---- redraw the whole map centered on (centerLon, centerLat) ----
static void redrawMap() {
  uint32_t t0 = millis();
  // fetchMap blocks until tiles are ready; tiles already in cache return
  // instantly. timeoutMS = 0 means "no time budget, fetch everything".
  bool ok = osm.fetchMap(mapSprite, centerLon, centerLat, ZOOM, 0);
  if (ok) {
    mapSprite.pushSprite(0, 0);
    Serial.printf("map drawn (%lu ms)\n", millis() - t0);
  } else {
    Serial.println("fetchMap failed");
  }
}

// ---- pan by a pixel delta (same Web Mercator projection as the library) ----
static void panByPixels(int dx, int dy) {
  if (!dx && !dy) return;
  // longitude: linear in tile units (1 tile = 256 px)
  double txx = (centerLon + 180.0) / 360.0 * (1 << ZOOM) - (double)dx / 256.0;
  centerLon = txx / (1 << ZOOM) * 360.0 - 180.0;
  // latitude: invert the mercator projection
  double latRad = centerLat * M_PI / 180.0;
  double tyy = (1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 *
                   (1 << ZOOM) -
               (double)dy / 256.0;
  double n = M_PI - 2.0 * M_PI * tyy / (1 << ZOOM);
  centerLat = 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
  s_mapDirty = true;
}

// ---- FT6336 touch ----
// This module's touch controller is portrait-native (raw x≈0..239,
// y≈0..319). Verified empirically: screenY = rawX (up/down correct) and
// screenX must be MIRRORED: screenX = (W-1) - rawY (left/right).
static int s_lastTX = -1, s_lastTY = -1;

static void handleTouch() {
  lgfx::touch_point_t tp;
  if (display.getTouchRaw(&tp, 1) != 1) {
    s_lastTX = s_lastTY = -1;
    return;
  }
  int x = (display.width() - 1) - tp.y;   // screenX = 319 - rawY (mirrored)
  int y = tp.x;                           // screenY = rawX (0..239)

  if (s_lastTX < 0) {
    Serial.printf("touch raw (%d,%d)\n", tp.x, tp.y);   // debug: verify mapping
  }

  if (s_lastTX >= 0) {
    panByPixels(x - s_lastTX, y - s_lastTY);
  }
  s_lastTX = x;
  s_lastTY = y;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nOSM touch tile viewer (OpenStreetMap-esp32)");

  display.init();
  display.setRotation(1);        // landscape 320x240
  display.setBrightness(255);    // BL=45 backlight on
  Serial.printf("Display: %dx%d\n", display.width(), display.height());

  // quick visual check: red screen for 1 s (confirms panel + backlight)
  display.fillScreen(display.color565(255, 0, 0));
  Serial.println("RED fill test done");
  delay(800);
  display.fillScreen(display.color565(32, 32, 128)); // OSM background blue

  osm.setSize(320, 240);
  if (!osm.resizeTilesCache(TILE_CACHE_SLOTS)) {
    Serial.println("WARNING: could not allocate tile cache (check PSRAM)");
  }
  Serial.printf("tiles needed for map: %u (cache=%d)\n",
                osm.tilesNeeded(320, 240), TILE_CACHE_SLOTS);

  // connect WiFi
  display.setTextColor(TFT_WHITE, display.color565(32, 32, 128));
  display.setCursor(20, 110);
  display.print("Connecting WiFi...");
  Serial.println("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    tries++;
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected, IP=%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi connect failed - check WIFI_SSID/WIFI_PASS");
    display.fillScreen(TFT_BLACK);
    display.setCursor(20, 110);
    display.print("WiFi connect failed!");
    return;
  }

  display.fillScreen(TFT_BLACK);
  display.setCursor(20, 110);
  display.print("Loading tiles...");
  Serial.println("Loading tiles...");
  redrawMap();
  Serial.println("Drag on the screen to pan the map.");
}

void loop() {
  handleTouch();
  if (s_mapDirty) {
    s_mapDirty = false;
    redrawMap();
  }
  delay(10);
}
