#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Google encoded polyline decoder.
 *
 * Encoded polylines are produced by the Google Maps API (e.g. the
 * `overview_polyline.points` field of the Directions API). This decodes
 * them back into an ordered list of lat/lon coordinates.
 *
 *   https://developers.google.com/maps/documentation/utilities/polylinealgorithm
 */

/**
 * Decode an encoded polyline into parallel lat/lon arrays.
 *
 * @param encoded  The polyline string (may be empty/NULL).
 * @param lats     Output array for latitudes (at least max_pts entries).
 * @param lons     Output array for longitudes (at least max_pts entries).
 * @param max_pts  Capacity of the output arrays.
 * @return Number of decoded points (0 on error / empty input).
 */
int polyline_decode(const char *encoded, double *lats, double *lons, int max_pts);

#ifdef __cplusplus
}
#endif
