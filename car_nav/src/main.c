/**
 * Car Navigation Display — ESP32-C3 + ST7789 240×198
 *
 * Phase 1: vector map renderer. Renders a compact map-XML packet into the
 * internal framebuffer and draws it on the ST7789. The XML normally comes
 * from the phone app (BLE/WiFi); for now a sample is embedded so the board
 * shows a map on boot.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "map_render.h"
#include "st7789.h"

static const char *TAG = "car_nav";

/* Sample map packet (Bến Thành, HCMC) — same schema the phone will send. */
static const char sample_map[] =
    "<map z=\"16\" cx=\"10.7720\" cy=\"106.6975\">"
    "<area cls=\"water\" pts=\"10.7752,106.7022 10.7766,106.7008 10.7752,106.6992 10.7738,106.7000\"/>"
    "<area cls=\"park\" pts=\"10.7704,106.6948 10.7720,106.6950 10.7723,106.6962 10.7707,106.6961\"/>"
    "<road cls=\"major\" pts=\"10.7735,106.6980 10.7715,106.6978 10.7695,106.6976\"/>"
    "<road cls=\"major\" pts=\"10.7730,106.7000 10.7718,106.6995 10.7705,106.6990\"/>"
    "<road cls=\"secondary\" pts=\"10.7745,106.6940 10.7725,106.6955 10.7705,106.6970\"/>"
    "<road cls=\"minor\" pts=\"10.7710,106.6950 10.7695,106.6970\"/>"
    "<road cls=\"minor\" pts=\"10.7740,106.6990 10.7728,106.6985\"/>"
    "<road cls=\"minor\" pts=\"10.7748,106.6962 10.7730,106.6963 10.7712,106.6965\"/>"
    "<road cls=\"minor\" pts=\"10.7704,106.6996 10.7696,106.6972\"/>"
    "<route pts=\"10.7730,106.6975 10.7724,106.6977 10.7718,106.6978 10.7712,106.6982 10.7706,106.6992\"/>"
    "<marker x=\"10.7730\" y=\"106.6975\"/>"
    "<poi name=\"Ben Thanh\" x=\"10.7726\" y=\"106.6981\"/>"
    "</map>";

/* Route the simulated car drives along (matches <route> in sample_map). */
static const double sim_route[][2] = {
    { 10.7730, 106.6975 }, { 10.7724, 106.6977 }, { 10.7718, 106.6978 },
    { 10.7712, 106.6982 }, { 10.7706, 106.6992 },
};
#define SIM_N (int)(sizeof(sim_route) / sizeof(sim_route[0]))

/* Rebuild the map packet each frame with the camera centered on a simulated
 * position + a <car> heading marker — so the map pans like a live nav app. */
static void simulate_driving(void)
{
    static char xml[2048];
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
        double dlat = sim_route[i + 1][0] - sim_route[i][0];
        double dlon = sim_route[i + 1][1] - sim_route[i][1];
        double h = atan2(dlon, dlat) * 180.0 / M_PI; /* bearing, 0 = north */
        int speed = 35 + (int)(30 * fabs(sin(frame / 60.0))); /* 35..65 km/h */

        snprintf(xml, sizeof(xml),
                 "<map z=\"16\" cx=\"%.4f\" cy=\"%.4f\" speed=\"%d\">"
                 "<area cls=\"water\" pts=\"10.7752,106.7022 10.7766,106.7008 10.7752,106.6992 10.7738,106.7000\"/>"
                 "<area cls=\"park\" pts=\"10.7704,106.6948 10.7720,106.6950 10.7723,106.6962 10.7707,106.6961\"/>"
                 "<road cls=\"major\" pts=\"10.7735,106.6980 10.7715,106.6978 10.7695,106.6976\"/>"
                 "<road cls=\"major\" pts=\"10.7730,106.7000 10.7718,106.6995 10.7705,106.6990\"/>"
                 "<road cls=\"secondary\" pts=\"10.7745,106.6940 10.7725,106.6955 10.7705,106.6970\"/>"
                 "<road cls=\"minor\" pts=\"10.7710,106.6950 10.7695,106.6970\"/>"
                 "<road cls=\"minor\" pts=\"10.7740,106.6990 10.7728,106.6985\"/>"
                 "<road cls=\"minor\" pts=\"10.7748,106.6962 10.7730,106.6963 10.7712,106.6965\"/>"
                 "<route pts=\"10.7730,106.6975 10.7724,106.6977 10.7718,106.6978 10.7712,106.6982 10.7706,106.6992\"/>"
                 "<car h=\"%.0f\"/>"
                 "</map>",
                 lat, lon, speed, h);

        map_render_show(xml, strlen(xml));
        frame++;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Car Navigation starting...");

    ESP_ERROR_CHECK(st7789_init());
    st7789_fill_screen(0x0000);

    if (map_render_init() != 0) {
        st7789_draw_string(10, 80, "Framebuffer FAIL", 0xF800, 0x0000);
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    map_render_show(sample_map, sizeof(sample_map) - 1);
    ESP_LOGI(TAG, "Vector map rendered (240x198).");

    /* Benchmark the renderer (parse+draw vs blit). */
    map_render_fps_test(sample_map, sizeof(sample_map) - 1, 30);

    ESP_LOGI(TAG, "Moving-map simulation started.");
    simulate_driving();
}
