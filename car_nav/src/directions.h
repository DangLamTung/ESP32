#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Max route points the renderer + decoder handle (matches map_render). */
#define DIRECTIONS_MAX_PTS  512
/* Max size of the Directions JSON response we buffer. */
#define DIRECTIONS_JSON_MAX 32768

/**
 * Build a `<route pts="lat,lon lat,lon ..."/>` XML fragment from a raw
 * Google encoded polyline (no network). Returns 0 on success, -1 on error.
 */
int directions_route_from_polyline(const char *encoded,
                                   char *route_xml, size_t route_xml_len);

/**
 * Fetch a driving route between two coordinates from the Google Directions
 * API, decode the overview polyline, and write a `<route>` XML fragment into
 * `route_xml` (ready to drop into a map packet for map_render_show()).
 *
 * Requires GOOGLE_MAPS_API_KEY (see map_fetch.h) + WiFi connected.
 * Returns ESP_OK on success.
 */
esp_err_t directions_get_route(double o_lat, double o_lon,
                               double d_lat, double d_lon,
                               char *route_xml, size_t route_xml_len);

#ifdef __cplusplus
}
#endif
