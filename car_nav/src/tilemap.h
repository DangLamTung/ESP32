#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the OSM tilemap (tile image + route line + car marker).
 *  Requires lvgl_port_init() first. Returns 0 on success. */
int tilemap_init(void);

/** Center the map on (lat, lon) at zoom z. Fetches + decodes the OSM raster
 *  tile covering that point (only when the tile index changes), positions it
 *  so the point is at screen center, and overlays the route (space-separated
 *  "lat,lon" points, may be NULL) and the car marker. Also updates the top
 *  banner / bottom speed HUD. */
void tilemap_show(double lat, double lon, int z, const char *route_pts,
                  int speed, const char *to_street);

#ifdef __cplusplus
}
#endif
