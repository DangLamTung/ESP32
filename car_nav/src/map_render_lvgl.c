/**
 * map_render.c — LVGL vector map renderer for ILI9341 320×240.
 *
 * Parses the compact map XML packet (see map_render.h) and draws it with
 * LVGL 9 into a full-screen RGB565 canvas:
 *   - filled area polygons (scanline fill via canvas pixels)
 *   - anti-aliased road lines (OSM casing + class color) drawn into a canvas
 *     layer with lv_draw_line (LVGL's SW renderer)
 *   - the route, the car arrow (filled triangle), POI dots + labels
 *   - a top navigation banner + bottom speed HUD as LVGL widgets
 *
 * Single-threaded: rendering happens on the app task and lvgl_refresh()
 * flushes synchronously through the LVGL display driver (ili9341).
 *
 * Buffer usage: the canvas layer draws directly into the canvas buffer
 * (no extra layer allocation on the ESP32-S3).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "lvgl.h"
#include "lvgl_port.h"
#include "map_render.h"
#include "ili9341.h"

#define W ILI9341_WIDTH
#define H ILI9341_HEIGHT

static const char *TAG = "map_render";

/* ---- RGB565 palette: OSM standard ("openstreetmap.org" carto) ---- */
#define C_DARK        0x0000 // device frame (outside circle)
#define C_MAP_BG      0xF77D // OSM land       #F2EFE9
#define C_WATER       0xAEDF // water          #AADAFF
#define C_PARK        0xCF56 // park/grass     #CDEBB0
#define C_FOREST      0xBED5 // forest         #BFD9A9
#define C_BUILDING    0xDE99 // building       #D9D0C9
#define C_BEACH       0xF6F6 // beach/sand     #F2DCB6

/* Road casing (outline) — light grey for most, blue-grey for motorways. */
#define C_CASING      0xCE59 // #C9C9C9
#define C_CASING_MW   0x84D8 // #809BC0 motorway casing

/* Road fills by class (OSM carto colors). */
#define C_ROAD_MW     0xE493 // motorway  #E79298
#define C_ROAD_TRUNK  0xF5AD // trunk     #F7B66D
#define C_ROAD_PRIM   0xF5EB // primary   #FBBD5C
#define C_ROAD_SEC    0xFEB1 // secondary #FDD78B
#define C_ROAD_TER    0xFFF6 // tertiary  #FFFFB3
#define C_ROAD_MIN    0xFFFF // minor/residential (white)

#define C_ROUTE       0x1B1D // directions route blue #1B63E8
#define C_ROUTE_CASING 0xFFFF // white casing under route
#define C_MARKER      0xF800 // red car
#define C_POI         0x001F // blue POI dot
#define C_TEXT        0x0000 // black
#define C_HUD_TEXT    0xFFFF // white (speed on dark band)
#define C_HUD_BAND    0x39E7 // dark grey band
#define C_BORDER      0x8410 // grey divider

/* Local-map viewport geometry: big circular map + speed band.
 * VY() scales a layout coordinate designed for the old 198-high screen
 * up to the current H (240 on the ILI9341 board). */
#define VY(y)   ((int)((y) * H / 198))
#define MAP_CX  (W / 2)
#define MAP_CY  (H * 90 / 198)   /* ~45% down: old 90 on 198 -> 109 on 240 */
#define MAP_R   (H * 92 / 198)   /* old 92 -> 111 */
#define HUD_Y   (H - 34)

/* One canvas layer shared by all draw calls (draws into the canvas buffer). */
static lv_layer_t g_layer;
static lv_obj_t *g_cv = NULL;
static uint16_t *g_canvas_buf = NULL;

/* ---- LVGL color helpers ---- */

static lv_color_t col565(uint16_t c)
{
    uint8_t r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
    uint8_t g = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
    uint8_t b = (uint8_t)((c & 0x1F) * 255 / 31);
    return lv_color_make(r, g, b);
}

/* ---- Canvas primitives (LVGL) ---- */

static inline void fb_px(int x, int y, uint16_t c)
{
    if (g_cv && x >= 0 && x < W && y >= 0 && y < H)
        lv_canvas_set_px(g_cv, x, y, col565(c), LV_OPA_COVER);
}

static void fb_fill(uint16_t c)
{
    if (g_cv)
        lv_canvas_fill_bg(g_cv, col565(c), LV_OPA_COVER);
}

static void fb_line(int x0, int y0, int x1, int y1, uint16_t c)
{
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.p1.x = x0;
    dsc.p1.y = y0;
    dsc.p2.x = x1;
    dsc.p2.y = y1;
    dsc.width = 1;
    dsc.color = col565(c);
    dsc.opa = LV_OPA_COVER;
    lv_draw_line(&g_layer, &dsc);
}

/* Draw a line of `th` pixels width (LVGL rounded caps). */
static void fb_thick_line(int x0, int y0, int x1, int y1, int th, uint16_t c)
{
    if (th <= 1) {
        fb_line(x0, y0, x1, y1, c);
        return;
    }
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.p1.x = x0;
    dsc.p1.y = y0;
    dsc.p2.x = x1;
    dsc.p2.y = y1;
    dsc.width = th;
    dsc.color = col565(c);
    dsc.opa = LV_OPA_COVER;
    dsc.round_start = 1;
    dsc.round_end = 1;
    lv_draw_line(&g_layer, &dsc);
}

static void fb_rect(int x0, int y0, int x1, int y1, uint16_t c)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = col565(c);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = 0;
    lv_area_t a = { x0, y0, x1, y1 };
    lv_draw_rect(&g_layer, &dsc, &a);
}

/* Filled disc (small markers) via a full 360° arc. */
static void fb_disc(int cx, int cy, int r, uint16_t c)
{
    if (r <= 0)
        return;
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.radius = r;
    dsc.start_angle = 0;
    dsc.end_angle = 360;
    dsc.width = r * 2;
    dsc.color = col565(c);
    dsc.opa = LV_OPA_COVER;
    lv_draw_arc(&g_layer, &dsc);
}

/* Even-odd scanline fill of a polygon (screen px) — writes canvas pixels
 * directly (areas are drawn before the roads, which paint over them). */
static void fb_fill_poly(const int *xs, const int *ys, int n, uint16_t c)
{
    if (n < 3)
        return;
    int miny = INT_MAX, maxy = -INT_MAX;
    for (int i = 0; i < n; i++) {
        if (ys[i] < miny) miny = ys[i];
        if (ys[i] > maxy) maxy = ys[i];
    }
    if (miny < 0) miny = 0;
    if (maxy >= H) maxy = H - 1;
    static int ix[128];
    for (int y = miny; y <= maxy; y++) {
        int cnt = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            int yi = ys[i], yj = ys[j];
            if ((yi <= y && yj > y) || (yj <= y && yi > y)) {
                double t = (double)(y - yi) / (double)(yj - yi);
                double x = (double)xs[i] + t * (double)(xs[j] - xs[i]);
                if (cnt < 128)
                    ix[cnt++] = (int)lround(x);
            }
        }
        for (int a = 1; a < cnt; a++) {
            int v = ix[a], b = a - 1;
            while (b >= 0 && ix[b] > v) { ix[b + 1] = ix[b]; b--; }
            ix[b + 1] = v;
        }
        for (int k = 0; k + 1 < cnt; k += 2) {
            int x0 = ix[k], x1 = ix[k + 1];
            if (x0 < 0) x0 = 0;
            if (x1 >= W) x1 = W - 1;
            for (int x = x0; x <= x1; x++)
                fb_px(x, y, c);
        }
    }
}

/* Text drawn into the canvas layer (LVGL font, transparent bg). */
static void cv_text(int x, int y, const char *s, const lv_font_t *font,
                    lv_color_t color)
{
    if (!s || !s[0])
        return;
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.text = s;
    dsc.font = font;
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    lv_area_t a = { x, y, x + W, y + 40 };
    lv_draw_label(&g_layer, &dsc, &a);
}

/* ---- Layer helpers ---- */
static void layer_begin(void)
{
    lv_canvas_init_layer(g_cv, &g_layer);
}
static void layer_end(void)
{
    lv_canvas_finish_layer(g_cv, &g_layer);
}

/* ---- Web Mercator projection ---- */
static double world_y(double lat, int z)
{
    double r = lat * M_PI / 180.0;
    double t = tan(M_PI / 4 + r / 2.0);
    return (1.0 - log(t) / M_PI) / 2.0 * (256.0 * (1 << z));
}

/* Project to screen pixels given center + zoom. */
static void project(double lat, double lon, int z, double cx, double cy,
                    int *sx, int *sy)
{
    double scale = 256.0 * (1 << z);
    double px = (lon + 180.0) / 360.0 * scale - (cy + 180.0) / 360.0 * scale;
    double py = world_y(lat, z) - world_y(cx, z);
    *sx = (int)lround(px + W / 2.0);
    *sy = (int)lround(py + H / 2.0);
}

/* ============ Heading-up range view (mini-map) ============
 * When the <map> packet carries range="<meters>" the raw road vectors are
 * projected into a local, heading-up mini-map: car at bottom-center, `range`
 * meters ahead to the top of the circle, rotated so heading points up. */

static int    rng_enabled = 0;
static double rng_m = 100.0;
static double rng_heading = 0.0;

static double range_mpp(void)
{
    int car_y = MAP_CY + VY(42);
    int px_to_top = car_y - (MAP_CY - MAP_R);
    return rng_m / (double)px_to_top;
}

static void project_range(double lat, double lon, double cx, double cy,
                          int *sx, int *sy)
{
    double mpp = range_mpp();
    double north_m = (lat - cx) * 111320.0;
    double east_m  = (lon - cy) * 111320.0 * cos(cx * M_PI / 180.0);
    double h = rng_heading * M_PI / 180.0;
    double right_px = ( east_m * cos(h) - north_m * sin(h)) / mpp;
    double ahead_px = ( east_m * sin(h) + north_m * cos(h)) / mpp;
    *sx = MAP_CX + (int)lround(right_px);
    *sy = (MAP_CY + VY(42)) - (int)lround(ahead_px);
}

static int out_of_range(double lat, double lon, double cx, double cy)
{
    if (!rng_enabled)
        return 0;
    double dy = (lat - cx) * 111320.0;
    double dx = (lon - cy) * 111320.0 * cos(cx * M_PI / 180.0);
    double lim = rng_m * 1.2;
    return (dx * dx + dy * dy) > lim * lim;
}

static void proj_pt(double lat, double lon, int z, double cx, double cy,
                    int *sx, int *sy)
{
    if (rng_enabled)
        project_range(lat, lon, cx, cy, sx, sy);
    else
        project(lat, lon, z, cx, cy, sx, sy);
}

/* ---- Tiny XML parsing (our own compact schema) ---- */
static const char *attr(const char *s, const char *name, char *out, size_t n)
{
    char pat[48];
    snprintf(pat, sizeof(pat), "%s=\"", name);
    const char *p = strstr(s, pat);
    if (!p)
        return NULL;
    p += strlen(pat);
    const char *q = strchr(p, '"');
    if (!q)
        return NULL;
    size_t len = (size_t)(q - p);
    if (len >= n)
        len = n - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return q + 1;
}

static int parse_pts(const char *s, double *lat, double *lon, int maxn)
{
    int n = 0;
    const char *p = s;
    while (n < maxn && *p) {
        char *end = NULL;
        lat[n] = strtod(p, &end);
        if (end == p)
            break;
        p = end;
        if (*p == ',')
            p++;
        lon[n] = strtod(p, &end);
        if (end == p)
            break;
        p = end;
        n++;
        while (*p == ' ' || *p == '\t' || *p == '\n')
            p++;
    }
    return n;
}

/* Darken an RGB565 color by `pct`% (OSM casing tone). */
static uint16_t darken565(uint16_t c, int pct)
{
    int r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    r = r * pct / 100;
    g = g * pct / 100;
    b = b * pct / 100;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* OSM road hierarchy → (casing color, fill color, width in px). */
static void road_style(const char *cls, uint16_t *casing, uint16_t *fill, int *w)
{
    *casing = C_CASING;
    *fill = C_ROAD_MIN;
    *w = 2;
    if (strcmp(cls, "motorway") == 0) {
        *casing = C_CASING_MW;
        *fill = C_ROAD_MW;
        *w = 5;
    } else if (strcmp(cls, "trunk") == 0) {
        *fill = C_ROAD_TRUNK;
        *w = 5;
        *casing = darken565(C_ROAD_TRUNK, 55);
    } else if (strcmp(cls, "primary") == 0 || strcmp(cls, "major") == 0) {
        *fill = C_ROAD_PRIM;
        *w = 4;
        *casing = darken565(C_ROAD_PRIM, 55);
    } else if (strcmp(cls, "secondary") == 0) {
        *fill = C_ROAD_SEC;
        *w = 3;
        *casing = darken565(C_ROAD_SEC, 50);
    } else if (strcmp(cls, "tertiary") == 0) {
        *fill = C_ROAD_TER;
        *w = 3;
        *casing = darken565(C_ROAD_TER, 45);
    }
}

static uint16_t area_color(const char *cls)
{
    if (strcmp(cls, "park") == 0 || strcmp(cls, "grass") == 0)
        return C_PARK;
    if (strcmp(cls, "forest") == 0)
        return C_FOREST;
    if (strcmp(cls, "building") == 0)
        return C_BUILDING;
    if (strcmp(cls, "beach") == 0 || strcmp(cls, "sand") == 0)
        return C_BEACH;
    return C_WATER;
}

/* ================= Turn-by-turn maneuver screen ================= */

#define C_NAV_BG 0x0885 /* dark navy behind maneuver content */

/* UTF-8 -> ASCII with Vietnamese diacritics stripped (for LVGL's Latin
 * Montserrat fonts, which have no diacritics). */
static uint32_t utf8_decode(const char **p)
{
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t cp;
    if (s[0] < 0x80) {
        cp = s[0];
        (*p) += 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        (*p) += 2;
    } else if ((s[0] & 0xF0) == 0xE0) {
        cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        (*p) += 3;
    } else {
        cp = s[0];
        (*p) += 1;
    }
    return cp;
}

static char vn_base(uint32_t cp)
{
    char base = 0;
    int upper = 0;
    if (cp >= 0x1EA0 && cp <= 0x1EF9) {
        upper = ((cp & 1) == 0);
        if (cp <= 0x1EB7)      base = 'a';
        else if (cp <= 0x1EC7) base = 'e';
        else if (cp <= 0x1ECB) base = 'i';
        else if (cp <= 0x1EE3) base = 'o';
        else if (cp <= 0x1EF1) base = 'u';
        else                   base = 'y';
    } else {
        switch (cp) {
        case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3:
        case 0x00E4: case 0x00E5: case 0x0103: base = 'a'; break;
        case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3:
        case 0x00C4: case 0x00C5: case 0x0102: base = 'A'; break;
        case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: base = 'e'; break;
        case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB: base = 'E'; break;
        case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: base = 'i'; break;
        case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF: base = 'I'; break;
        case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5:
        case 0x00F6: case 0x01A1: base = 'o'; break;
        case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5:
        case 0x00D6: case 0x01A0: base = 'O'; break;
        case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: case 0x01B0: base = 'u'; break;
        case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC: case 0x01AF: base = 'U'; break;
        case 0x00FD: case 0x00FF: base = 'y'; break;
        case 0x00DD: base = 'Y'; break;
        case 0x0111: base = 'd'; break;
        case 0x0110: base = 'D'; break;
        case 0x0129: base = 'i'; break;
        case 0x0128: base = 'I'; break;
        case 0x0169: base = 'u'; break;
        case 0x0168: base = 'U'; break;
        default:
            return (cp < 0x80) ? (char)cp : '?';
        }
    }
    if (upper)
        base = (char)(base - 32);
    return base;
}

static void utf8_strip(const char *src, char *dst, size_t n)
{
    if (!src || n == 0)
        return;
    size_t o = 0;
    while (*src && o + 1 < n) {
        const char *p = src;
        uint32_t cp = utf8_decode(&p);
        char c = vn_base(cp);
        if (c)
            dst[o++] = c;
        src = p;
    }
    dst[o] = '\0';
}

static nav_maneuver_t maneuver_from_str(const char *s)
{
    if (!s) return NAV_STRAIGHT;
    if (!strcmp(s, "left")) return NAV_LEFT;
    if (!strcmp(s, "right")) return NAV_RIGHT;
    if (!strcmp(s, "slight_left") || !strcmp(s, "slight-left")) return NAV_SLIGHT_LEFT;
    if (!strcmp(s, "slight_right") || !strcmp(s, "slight-right")) return NAV_SLIGHT_RIGHT;
    if (!strcmp(s, "sharp_left") || !strcmp(s, "sharp-left")) return NAV_SHARP_LEFT;
    if (!strcmp(s, "sharp_right") || !strcmp(s, "sharp-right")) return NAV_SHARP_RIGHT;
    if (!strcmp(s, "uturn") || !strcmp(s, "u_turn") || !strcmp(s, "u-turn")) return NAV_UTURN;
    if (!strcmp(s, "arrive")) return NAV_ARRIVE;
    return NAV_STRAIGHT;
}

static double maneuver_angle(nav_maneuver_t m)
{
    switch (m) {
    case NAV_LEFT:         return M_PI / 2;
    case NAV_RIGHT:        return -M_PI / 2;
    case NAV_SLIGHT_LEFT:  return M_PI / 6;
    case NAV_SLIGHT_RIGHT: return -M_PI / 6;
    case NAV_SHARP_LEFT:   return 2 * M_PI / 3;
    case NAV_SHARP_RIGHT:  return -2 * M_PI / 3;
    case NAV_UTURN:        return M_PI;
    default:               return 0;
    }
}

/* Fill a triangle given 3 local points (heading frame), rotated by `hr`. */
static void fill_tri_local(const double loc[3][2], int cx, int cy, double hr,
                           uint16_t c)
{
    double ca = cos(hr), sa = sin(hr);
    lv_point_precise_t p[3];
    for (int i = 0; i < 3; i++) {
        double rx = loc[i][0] * ca - loc[i][1] * sa;
        double ry = loc[i][0] * sa + loc[i][1] * ca;
        p[i].x = cx + (int32_t)lround(rx);
        p[i].y = cy + (int32_t)lround(ry);
    }
    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.color = col565(c);
    dsc.opa = LV_OPA_COVER;
    dsc.p[0] = p[0];
    dsc.p[1] = p[1];
    dsc.p[2] = p[2];
    lv_draw_triangle(&g_layer, &dsc);
}

/* Big navigation arrow: filled head triangle + thick shaft, rotated. */
static const double NAV_HEAD[3][2] = { { 0, -1.85 }, { 1.05, 0.05 }, { -1.05, 0.05 } };
static void draw_nav_arrow(double ang, int cx, int cy, double s, uint16_t c)
{
    double head[3][2];
    for (int i = 0; i < 3; i++) {
        head[i][0] = NAV_HEAD[i][0] * s;
        head[i][1] = NAV_HEAD[i][1] * s;
    }
    fill_tri_local(head, cx, cy, ang, c);
    /* shaft from (0, 0.1s) to (0, 1.7s), width 0.62s */
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.width = (int32_t)(0.62 * s);
    dsc.color = col565(c);
    dsc.opa = LV_OPA_COVER;
    dsc.round_start = 1;
    dsc.round_end = 1;
    double ca = cos(ang), sa = sin(ang);
    double x1 = cx + (0.0 * ca - 0.1 * s * sa);
    double y1 = cy + (0.0 * sa + 0.1 * s * ca);
    double x2 = cx + (0.0 * ca - 1.7 * s * sa);
    double y2 = cy + (0.0 * sa + 1.7 * s * ca);
    dsc.p1.x = (int32_t)lround(x1);
    dsc.p1.y = (int32_t)lround(y1);
    dsc.p2.x = (int32_t)lround(x2);
    dsc.p2.y = (int32_t)lround(y2);
    lv_draw_line(&g_layer, &dsc);
}

/* Render the turn-by-turn screen into the canvas. */
static void render_nav_to_fb(const nav_screen_t *nav)
{
    static char from[64], to[64];
    char dist[16];

    utf8_strip(nav->from, from, sizeof(from));
    utf8_strip(nav->to, to, sizeof(to));

    lvgl_banner_visible(false); /* the nav screen draws its own street text */

    fb_fill(C_DARK);
    fb_fill(C_NAV_BG); /* full-screen navy (no circle clip on LVGL) */

    layer_begin();

    /* current street (top, below where the banner would be) */
    int nf = (int)strlen(from);
    cv_text((W - nf * 8) / 2, VY(8), from, &lv_font_montserrat_14, lv_color_white());

    if (nav->maneuver == NAV_ARRIVE) {
        fb_disc(MAP_CX, VY(62), 16, 0xFFFF);
        fb_disc(MAP_CX, VY(62), 13, C_ROUTE);
        fb_disc(MAP_CX, VY(62), 4, 0xFFFF);
        fb_thick_line(MAP_CX, VY(78), MAP_CX, VY(96), 4, 0xFFFF);
    } else {
        double ang = maneuver_angle(nav->maneuver);
        draw_nav_arrow(ang, MAP_CX, VY(62), 17.5, C_TEXT);
        draw_nav_arrow(ang, MAP_CX, VY(62), 16.0, 0xFFFF);
        fb_disc(MAP_CX, VY(62), 3, C_ROUTE);
    }

    /* distance to the maneuver (big) */
    if (nav->distance_m >= 1000)
        snprintf(dist, sizeof(dist), "%d.%d km", nav->distance_m / 1000,
                 (nav->distance_m / 100) % 10);
    else if (nav->distance_m > 0)
        snprintf(dist, sizeof(dist), "%dm", nav->distance_m);
    else
        snprintf(dist, sizeof(dist), "DEN NOI");
    int nd = (int)strlen(dist);
    cv_text((W - nd * 11) / 2, VY(96), dist, &lv_font_montserrat_20, lv_color_white());

    /* next street (blue banner) */
    int nt = (int)strlen(to);
    int tw = nt * 8 + 14;
    int tx = (W - tw) / 2;
    if (tx < 0) tx = 0;
    fb_rect(tx, VY(136), tx + tw, VY(155), C_ROUTE);
    cv_text(tx + 7, VY(138), to, &lv_font_montserrat_14, lv_color_white());

    layer_end();

    lvgl_set_speed(nav->speed);
}

/* ================= Map packet renderer ================= */

int map_render_init(void)
{
    if (g_cv)
        return 0;
    if (lvgl_port_init() != 0)
        return -1;
    /* 150 KB canvas — PSRAM first (keeps internal SRAM free), internal fallback */
    g_canvas_buf = heap_caps_malloc((size_t)W * H * 2, MALLOC_CAP_SPIRAM);
    if (!g_canvas_buf)
        g_canvas_buf = heap_caps_malloc((size_t)W * H * 2, MALLOC_CAP_8BIT);
    if (!g_canvas_buf) {
        ESP_LOGE(TAG, "canvas buf alloc failed");
        return -1;
    }
    g_cv = lvgl_port_create_canvas(g_canvas_buf);
    if (!g_cv)
        return -1;
    lvgl_ui_foreground(); /* keep banner + HUD above the canvas */
    return 0;
}

static void map_refresh(void)
{
    if (g_cv)
        lv_obj_invalidate(g_cv);
    lv_refr_now(NULL);
}

void map_render_test_bars(void)
{
    if (map_render_init() != 0)
        return;
    const uint16_t bars[] = { 0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000 };
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            fb_px(x, y, bars[(y * 5) / H]);
    map_refresh();
}

/* Parse + draw a map XML packet into the canvas (no refresh). */
static int render_to_fb(const char *xml, size_t len)
{
    if (map_render_init() != 0)
        return -1;

    /* Turn-by-turn maneuver packet:
     *   <nav dir="left" dist="100" from="Nguyễn Lợi" to="Đức Cảnh" speed="42"/> */
    const char *nav_tag = strstr(xml, "<nav");
    if (nav_tag) {
        static char nfrom[64], nto[64], nd[16], nv[16];
        nav_screen_t nav;
        nav.maneuver = NAV_STRAIGHT;
        nav.distance_m = 100;
        nav.speed = 42;
        nav.from = nfrom;
        nav.to = nto;
        strcpy(nfrom, "Nguyen Loi");
        strcpy(nto, "Duc Canh");
        if (attr(nav_tag, "dir", nd, sizeof(nd)))
            nav.maneuver = maneuver_from_str(nd);
        if (attr(nav_tag, "dist", nv, sizeof(nv)))
            nav.distance_m = atoi(nv);
        if (attr(nav_tag, "speed", nv, sizeof(nv)))
            nav.speed = atoi(nv);
        attr(nav_tag, "from", nfrom, sizeof(nfrom));
        attr(nav_tag, "to", nto, sizeof(nto));
        render_nav_to_fb(&nav);
        return 0;
    }

    static char val[64];
    static char pts[4096];
    static char cls[16];
    static char nm[64];
    static double la[512], lo[512];
    static int xs[512], ys[512];
    static char to_street[64];

    int z = 15;
    double cx = 10.7701, cy = 106.6920;
    int speed = 0;

    rng_enabled = 0;
    rng_heading = 0.0;
    to_street[0] = '\0';

    fb_fill(C_MAP_BG);
    layer_begin();

    const char *map_tag = strstr(xml, "<map");
    if (map_tag) {
        if (attr(map_tag, "z", val, sizeof(val)))
            z = atoi(val);
        if (attr(map_tag, "cx", val, sizeof(val)))
            cx = atof(val);
        if (attr(map_tag, "cy", val, sizeof(val)))
            cy = atof(val);
        if (attr(map_tag, "speed", val, sizeof(val)))
            speed = atoi(val);
        if (attr(map_tag, "range", val, sizeof(val))) {
            rng_m = atof(val);
            rng_enabled = (rng_m > 0);
        }
        if (attr(map_tag, "h", val, sizeof(val)))
            rng_heading = atof(val);
        attr(map_tag, "to", to_street, sizeof(to_street));
        if (z < 1) z = 1;
        if (z > 19) z = 19;
    }
    ESP_LOGI(TAG, "map z=%d cx=%.4f cy=%.4f range=%s len=%u",
             z, cx, cy, rng_enabled ? "on" : "off", (unsigned)len);

    const char *p = xml;
    while (p && *p) {
        const char *t = strstr(p, "<road");
        const char *u = strstr(p, "<route");
        const char *m = strstr(p, "<marker");
        const char *q = strstr(p, "<poi");
        const char *a = strstr(p, "<area");
        const char *c = strstr(p, "<car");

        const char *next = NULL;
        enum { T_ROAD, T_ROUTE, T_MARKER, T_POI, T_AREA, T_CAR } kind = T_ROAD;

        if (a && (!next || a < next)) { next = a; kind = T_AREA; }
        if (c && (!next || c < next)) { next = c; kind = T_CAR; }
        if (t && (!next || t < next)) { next = t; kind = T_ROAD; }
        if (u && (!next || u < next)) { next = u; kind = T_ROUTE; }
        if (m && (!next || m < next)) { next = m; kind = T_MARKER; }
        if (q && (!next || q < next)) { next = q; kind = T_POI; }

        if (!next)
            break;

        if (kind == T_AREA) {
            strcpy(cls, "water");
            pts[0] = '\0';
            attr(next, "cls", cls, 16);
            attr(next, "pts", pts, sizeof(pts));
            int n = parse_pts(pts, la, lo, 256);
            uint16_t col = area_color(cls);
            for (int i = 0; i < n; i++)
                proj_pt(la[i], lo[i], z, cx, cy, &xs[i], &ys[i]);
            fb_fill_poly(xs, ys, n, col);
            if (strcmp(cls, "building") == 0) {
                for (int i = 0; i < n; i++) {
                    int j = (i + 1) % n;
                    fb_line(xs[i], ys[i], xs[j], ys[j], 0xB6B1);
                }
            }
        } else if (kind == T_ROAD) {
            strcpy(cls, "minor");
            pts[0] = '\0';
            attr(next, "cls", cls, 16);
            attr(next, "pts", pts, sizeof(pts));
            int n = parse_pts(pts, la, lo, 256);
            uint16_t casing, col;
            int th;
            road_style(cls, &casing, &col, &th);
            for (int i = 1; i < n; i++) {
                if (out_of_range(la[i - 1], lo[i - 1], cx, cy) &&
                    out_of_range(la[i], lo[i], cx, cy))
                    continue;
                int x0, y0, x1, y1;
                proj_pt(la[i - 1], lo[i - 1], z, cx, cy, &x0, &y0);
                proj_pt(la[i], lo[i], z, cx, cy, &x1, &y1);
                fb_thick_line(x0, y0, x1, y1, th + 2, casing);
                fb_thick_line(x0, y0, x1, y1, th, col);
            }
        } else if (kind == T_ROUTE) {
            pts[0] = '\0';
            attr(next, "pts", pts, sizeof(pts));
            int n = parse_pts(pts, la, lo, 512);
            for (int i = 1; i < n; i++) {
                if (out_of_range(la[i - 1], lo[i - 1], cx, cy) &&
                    out_of_range(la[i], lo[i], cx, cy))
                    continue;
                int x0, y0, x1, y1;
                proj_pt(la[i - 1], lo[i - 1], z, cx, cy, &x0, &y0);
                proj_pt(la[i], lo[i], z, cx, cy, &x1, &y1);
                fb_thick_line(x0, y0, x1, y1, 5, C_ROUTE_CASING);
                fb_thick_line(x0, y0, x1, y1, 3, C_ROUTE);
            }
        } else if (kind == T_MARKER) {
            double mx = cx, my = cy;
            if (attr(next, "x", val, sizeof(val)))
                my = atof(val);
            if (attr(next, "y", val, sizeof(val)))
                mx = atof(val);
            int sx, sy;
            proj_pt(my, mx, z, cx, cy, &sx, &sy);
            fb_disc(sx, sy, 5, 0xFFFF);
            fb_disc(sx, sy, 4, C_MARKER);
            fb_disc(sx, sy, 1, 0xFFFF);
        } else if (kind == T_CAR) {
            double h = 0;
            if (attr(next, "h", val, sizeof(val)))
                h = atof(val);
            if (rng_enabled)
                rng_heading = h;
            double hr = (rng_enabled ? 0.0 : h) * M_PI / 180.0;
            int carx = rng_enabled ? MAP_CX : W / 2;
            int cary = rng_enabled ? (MAP_CY + VY(42)) : H / 2;
            /* filled heading arrow: dark outline + white body + red hub */
            static const double OUT[3][2] = { { 0, -9 }, { 6, 7 }, { -6, 7 } };
            static const double IN[3][2]  = { { 0, -5 }, { 4, 5 }, { -4, 5 } };
            fill_tri_local(OUT, carx, cary, hr, C_TEXT);
            fill_tri_local(IN, carx, cary, hr, 0xFFFF);
            fb_disc(carx, cary, 2, C_MARKER);
        } else { /* POI */
            double px = cy, py = cx;
            nm[0] = '\0';
            if (attr(next, "x", val, sizeof(val)))
                py = atof(val);
            if (attr(next, "y", val, sizeof(val)))
                px = atof(val);
            attr(next, "name", nm, sizeof(nm));
            int sx, sy;
            proj_pt(py, px, z, cx, cy, &sx, &sy);
            fb_disc(sx, sy, 2, C_POI);
            if (nm[0])
                cv_text(sx + 4, sy - 12, nm, &lv_font_montserrat_14, lv_color_black());
        }

        p = next + 1;
    }

    layer_end();

    /* navigation-style top banner: the street we're heading to */
    static char banner[48];
    utf8_strip(to_street, banner, sizeof(banner));
    lvgl_banner_visible(true);
    lvgl_set_banner(banner);
    lvgl_set_speed(speed);

    return 0;
}

int map_render_show(const char *xml, size_t len)
{
    int r = render_to_fb(xml, len);
    if (r != 0)
        return r;
    map_refresh();
    return 0;
}

void map_render_nav_show(const nav_screen_t *nav)
{
    if (map_render_init() != 0)
        return;
    if (!nav)
        return;
    render_nav_to_fb(nav);
    map_refresh();
}

void map_render_fps_test(const char *xml, size_t len, int frames)
{
    if (map_render_init() != 0)
        return;
    if (frames < 1)
        frames = 1;

    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < frames; i++)
        render_to_fb(xml, len);
    int64_t t1 = esp_timer_get_time();
    for (int i = 0; i < frames; i++)
        map_refresh();
    int64_t t2 = esp_timer_get_time();

    double draw_ms = (double)(t1 - t0) / frames / 1000.0;
    double blit_ms = (double)(t2 - t1) / frames / 1000.0;
    double tot_ms = draw_ms + blit_ms;
    ESP_LOGI(TAG, "fps test (%d frames): draw %.2f ms, flush %.2f ms, "
                  "total %.2f ms/frame => %.1f fps",
             frames, draw_ms, blit_ms, tot_ms, 1000.0 / tot_ms);
}

void map_render_deinit(void)
{
    /* The LVGL canvas/display buffers are owned by lvgl_port. */
}
