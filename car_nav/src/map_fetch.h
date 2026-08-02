#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GOOGLE_MAPS_API_KEY "YOUR_API_KEY_HERE"

esp_err_t map_fetch(double lat, double lon, int zoom, int w, int h);

#ifdef __cplusplus
}
#endif
