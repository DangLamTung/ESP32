#include "map_fetch.h"
#include "ili9341.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "rom/tjpgd.h"

static const char *TAG = "map";

static uint8_t  *jpeg_buf = NULL;
static uint16_t *line_buf = NULL;
static uint8_t  *work_buf = NULL;
static size_t    jpeg_len = 0;
static size_t    jpeg_pos = 0;

static size_t tjd_input(JDEC *jd, uint8_t *buf, size_t n) {
    (void)jd;
    size_t r = jpeg_len - jpeg_pos;
    if (r == 0) return 0;
    if (n > r) n = r;
    memcpy(buf, jpeg_buf + jpeg_pos, n);
    jpeg_pos += n;
    return n;
}

static unsigned int tjd_output(JDEC *jd, void *bitmap, JRECT *rect) {
    (void)jd;
    uint8_t *src = (uint8_t *)bitmap;
    int x0 = rect->left, y0 = rect->top;
    int w = rect->right - rect->left + 1, h = rect->bottom - rect->top + 1;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint8_t r = src[(row*w+col)*3+0];
            uint8_t g = src[(row*w+col)*3+1];
            uint8_t b = src[(row*w+col)*3+2];
            line_buf[col] = ( ((r&0xF8)<<8) | ((g&0xFC)<<3) | (b>>3) );
        }
        uint16_t c = line_buf[0];
        line_buf[0] = (c>>8)|(c<<8); // HACK: byte-swap to big-endian for SPI DMA
        c = line_buf[w-1];
        line_buf[w-1] = (c>>8)|(c<<8); // byte-swap last pixel
        // Actually need to swap ALL pixels. But for speed, swap inline:
        for (int i = 1; i < w-1; i++) { uint16_t x = line_buf[i]; line_buf[i] = (x>>8)|(x<<8); }
        st7789_wait_dma();
        st7789_set_window(x0, y0+row, x0+w-1, y0+row);
        st7789_write_pixels_dma(line_buf, w * sizeof(uint16_t));
    }
    return 1;
}

static esp_err_t http_handler(esp_http_client_event_t *e) {
    if (e->event_id == HTTP_EVENT_ON_DATA) {
        size_t remain = 65536 - jpeg_len;
        size_t copy = e->data_len < remain ? e->data_len : remain;
        if (copy > 0) { memcpy(jpeg_buf + jpeg_len, e->data, copy); jpeg_len += copy; }
    }
    return ESP_OK;
}

esp_err_t map_fetch(double lat, double lon, int zoom, int w, int h) {
    if (!jpeg_buf) {
        jpeg_buf = malloc(65536);
        line_buf = malloc(ILI9341_WIDTH * sizeof(uint16_t));
        work_buf = malloc(4000);
        if (!jpeg_buf || !line_buf || !work_buf) { ESP_LOGE(TAG, "malloc"); return ESP_ERR_NO_MEM; }
    }
    jpeg_len = 0; jpeg_pos = 0;

    char url[512];
    snprintf(url, sizeof(url),
        "https://maps.googleapis.com/maps/api/staticmap"
        "?center=%.6f,%.6f&zoom=%d&size=%dx%d&format=jpg&maptype=roadmap&key=%s",
        lat, lon, zoom, w, h, GOOGLE_MAPS_API_KEY);
    ESP_LOGI(TAG, "Fetching map...");

    esp_http_client_config_t cfg = {
        .url = url, .event_handler = http_handler,
        .timeout_ms = 15000, .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "User-Agent", "ESP32-CarNav/1.0");
    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (ret != ESP_OK || status != 200) { ESP_LOGE(TAG, "HTTP %d", status); return ESP_FAIL; }
    if (jpeg_len < 100) { ESP_LOGE(TAG, "JPEG too small"); return ESP_FAIL; }

    ESP_LOGI(TAG, "Decoding %u bytes...", (unsigned)jpeg_len);
    JDEC jd;
    JRESULT rc = jd_prepare(&jd, tjd_input, work_buf, 4000, NULL);
    if (rc != JDR_OK) { ESP_LOGE(TAG, "jd_prepare: %d", rc); return ESP_FAIL; }
    rc = jd_decomp(&jd, tjd_output, 0);
    if (rc != JDR_OK) { ESP_LOGE(TAG, "jd_decomp: %d", rc); return ESP_FAIL; }
    ESP_LOGI(TAG, "Map rendered!");
    return ESP_OK;
}
