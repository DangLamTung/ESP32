/**
 * Car Navigation Display — ESP32-S3 + ILI9341 320×240
 *
 * Tilemap demo: fetches REAL OSM raster tiles (tile.openstreetmap.org) over
 * WiFi and renders them through LVGL (see tilemap.c). The car drives along a
 * real street around Bến Thành while the map scrolls beneath it. Vector map
 * packets from the phone (BLE) still render via map_render_*.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "map_render.h"
#include "ili9341.h"
#include "tilemap.h"
#include "ble_server.h"

static const char *TAG = "car_nav";

/* Real route (Bến Thành, HCMC) the simulated car drives along. */
static const char real_route_pts[] =
    "10.772167,106.698905 10.772093,106.698853 10.772016,106.698810 10.771934,106.698773 "
    "10.771847,106.698732 10.771767,106.698680 10.771586,106.698532 10.771549,106.698502 "
    "10.771470,106.698437 10.771446,106.698419";

/* Route the simulated car drives along (a real street in the OSM data). */
static const double sim_route[][2] = {
    { 10.772167, 106.698905 }, { 10.772093, 106.698853 },
    { 10.772016, 106.698810 }, { 10.771934, 106.698773 },
    { 10.771847, 106.698732 }, { 10.771767, 106.698680 },
    { 10.771586, 106.698532 }, { 10.771549, 106.698502 },
    { 10.771470, 106.698437 }, { 10.771446, 106.698419 },
};
#define SIM_N (int)(sizeof(sim_route) / sizeof(sim_route[0]))

/* Drive the simulated car along the real route; the OSM tilemap scrolls
 * beneath it (a new tile is fetched only when crossing a tile boundary). */
static void simulate_driving(void)
{
    int frame = 0;
    for (;;) {
        double total = (double)(SIM_N - 1);
        double t = ((double)(frame % (SIM_N * 40))) / (double)(SIM_N * 40) * total;
        int i = (int)t;
        if (i >= SIM_N - 1)
            i = SIM_N - 2;
        double f = t - i;
        double lat = sim_route[i][0] + f * (sim_route[i + 1][0] - sim_route[i][0]);
        double lon = sim_route[i][1] + f * (sim_route[i + 1][1] - sim_route[i][1]);
        int speed = 35 + (int)(30 * fabs(sin(frame / 60.0))); /* 35..65 km/h */

        tilemap_show(lat, lon, 16, real_route_pts, speed, "Lê Lai");
        frame++;
        vTaskDelay(pdMS_TO_TICKS(100)); /* ~10 fps; tile fetch is infrequent */
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Car Navigation starting...");

    /* Vendor-derived ILI9341 driver: SPI bus + DMA pool + controller + backlight */
    ESP_ERROR_CHECK(ili9341_init());

    if (tilemap_init() != 0) {
        ESP_LOGE(TAG, "LVGL init failed");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    /* Offline demo: tiles are embedded in flash (map_tiles.h), no WiFi. */

    /* BLE GATT server: the phone streams nav/map data to the display. */
    ble_server_init();

    /* Boot: real OSM tile at Bến Thành (zoom 16). */
    tilemap_show(10.7718, 106.6982, 16, real_route_pts, 0, "Bến Thành");
    ESP_LOGI(TAG, "OSM tilemap shown (zoom 16).");
    vTaskDelay(pdMS_TO_TICKS(2500));

    ESP_LOGI(TAG, "Tilemap navigation simulation.");
    simulate_driving();
}
