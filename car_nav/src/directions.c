#include "directions.h"
#include "polyline.h"
#include "map_fetch.h" /* GOOGLE_MAPS_API_KEY */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "directions";

static char *json_buf = NULL;
static size_t json_len = 0;

static esp_err_t http_handler(esp_http_client_event_t *e)
{
    if (e->event_id == HTTP_EVENT_ON_DATA) {
        if (json_len + e->data_len + 1 > DIRECTIONS_JSON_MAX) {
            ESP_LOGW(TAG, "directions response too large (%u bytes)",
                     (unsigned)(json_len + e->data_len));
            return ESP_ERR_HTTP_EAGAIN;
        }
        memcpy(json_buf + json_len, e->data, e->data_len);
        json_len += e->data_len;
        json_buf[json_len] = '\0';
    }
    return ESP_OK;
}

int directions_route_from_polyline(const char *encoded,
                                   char *route_xml, size_t route_xml_len)
{
    static double lats[DIRECTIONS_MAX_PTS], lons[DIRECTIONS_MAX_PTS];

    if (!encoded || !route_xml || route_xml_len < 16)
        return -1;

    int n = polyline_decode(encoded, lats, lons, DIRECTIONS_MAX_PTS);
    if (n < 2) {
        ESP_LOGE(TAG, "polyline decode failed (%d pts)", n);
        return -1;
    }
    ESP_LOGI(TAG, "decoded %d route points", n);

    int used = snprintf(route_xml, route_xml_len, "<route pts=\"");
    if (used < 0)
        return -1;
    for (int i = 0; i < n; i++) {
        int r = snprintf(route_xml + used, route_xml_len - (size_t)used,
                         "%s%.5f,%.5f", i ? " " : "", lats[i], lons[i]);
        if (r < 0 || used + r >= (int)route_xml_len)
            break;
        used += r;
    }
    snprintf(route_xml + used, route_xml_len - (size_t)used, "\"/>");
    return 0;
}

esp_err_t directions_get_route(double o_lat, double o_lon,
                               double d_lat, double d_lon,
                               char *route_xml, size_t route_xml_len)
{
    if (!json_buf) {
        json_buf = malloc(DIRECTIONS_JSON_MAX);
        if (!json_buf) {
            ESP_LOGE(TAG, "malloc");
            return ESP_ERR_NO_MEM;
        }
    }
    json_len = 0;
    json_buf[0] = '\0';

    char url[512];
    snprintf(url, sizeof(url),
             "https://maps.googleapis.com/maps/api/directions/json"
             "?origin=%.6f,%.6f&destination=%.6f,%.6f"
             "&mode=driving&key=%s",
             o_lat, o_lon, d_lat, d_lon, GOOGLE_MAPS_API_KEY);
    ESP_LOGI(TAG, "Fetching directions...");

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_handler,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "User-Agent", "ESP32-CarNav/1.0");
    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (ret != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "HTTP %d (err %d)", status, ret);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(json_buf);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse failed");
        return ESP_FAIL;
    }

    const char *encoded = NULL;
    cJSON *routes = cJSON_GetObjectItem(root, "routes");
    if (routes && cJSON_GetArraySize(routes) > 0) {
        cJSON *r0 = cJSON_GetArrayItem(routes, 0);
        cJSON *poly = cJSON_GetObjectItem(r0, "overview_polyline");
        cJSON *pts = poly ? cJSON_GetObjectItem(poly, "points") : NULL;
        if (pts && cJSON_IsString(pts) && pts->valuestring)
            encoded = pts->valuestring;
    }

    int rc = -1;
    if (encoded) {
        rc = directions_route_from_polyline(encoded, route_xml, route_xml_len);
    } else {
        ESP_LOGE(TAG, "no overview_polyline in response");
    }

    cJSON_Delete(root);
    return (rc == 0) ? ESP_OK : ESP_FAIL;
}
