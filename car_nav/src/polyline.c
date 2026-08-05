#include "polyline.h"
#include <string.h>

int polyline_decode(const char *encoded, double *lats, double *lons, int max_pts)
{
    if (!encoded || !lats || !lons || max_pts <= 0)
        return 0;

    int n = 0;
    int i = 0;
    const int len = (int)strlen(encoded);
    int plat = 0, plon = 0;

    while (i < len && n < max_pts) {
        /* Decode a signed 32-bit delta (5-bit chunks, +63 bias). */
        int shift = 0, result = 0;
        int b;
        do {
            if (i >= len)
                return n;
            b = encoded[i++] - 63;
            result |= (b & 0x1F) << shift;
            shift += 5;
        } while (b >= 0x20);
        int dlat = (result & 1) ? ~(result >> 1) : (result >> 1);
        plat += dlat;

        shift = 0;
        result = 0;
        do {
            if (i >= len)
                return n;
            b = encoded[i++] - 63;
            result |= (b & 0x1F) << shift;
            shift += 5;
        } while (b >= 0x20);
        int dlon = (result & 1) ? ~(result >> 1) : (result >> 1);
        plon += dlon;

        lats[n] = plat / 1e5;
        lons[n] = plon / 1e5;
        n++;
    }
    return n;
}
