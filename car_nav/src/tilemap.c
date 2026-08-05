/**
 * tilemap.c — OSM raster tile map rendered through LVGL, OFFLINE.
 *
 * The 320x240 screen is drawn into a full-screen RGB565 buffer shown as an
 * lv_image (native format -> direct blit). The 2x2 OSM tiles covering the
 * Bến Thành demo area are embedded in flash (src/map_tiles.h) as JPEG and
 * decoded with LVGL's built-in tjpgd decoder — no WiFi, no PNG decoder,
 * minimal flash/RAM. The route is drawn with an lv_line and the car with a
 * marker dot; the top banner + bottom speed HUD are LVGL widgets.
 *
 * RAM: screen buffer 320*240*2 (~150 KB); tiles decode MCU-by-MCU (no full
 * decoded copy).
 */
#include "tilemap.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "map_tiles.h"
#include "ili9341.h"

/* private LVGL image-decoder struct (dsc->decoded, dsc->header, ...) */
#include "src/draw/lv_image_decoder_private.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"

#define W ILI9341_WIDTH
#define H ILI9341_HEIGHT
#define TILE 256            /* OSM tile size */
#define CX (W / 2)          /* car is at screen center */
#define CY 120              /* vertical center of the 240-high screen */
#define MAX_ROUTE 64

static const char *TAG = "tilemap";

static lv_obj_t *g_map_img = NULL;
static lv_image_dsc_t g_map_dsc;
static uint16_t *g_map_buf = NULL;   /* full-screen RGB565, shown via lv_image */
static lv_obj_t *g_route_outline = NULL;
static lv_obj_t *g_route = NULL;
static lv_obj_t *g_car = NULL;

static int g_z = -1;                 /* current visible tile block (lazy) */
static int g_x0, g_y0, g_x1, g_y1;
static int g_last_wx = 0, g_last_wy = 0; /* world-px offset of current map buf */

/* ---- tile decode state ---- */
static int g_tile_ox = 0, g_tile_oy = 0;

/* Look up a tile embedded in flash (map_tiles.h, JPEG). Returns bytes or NULL. */
static const uint8_t *embedded_tile(int z, int tx, int ty, uint32_t *len)
{
    for (int i = 0; i < EMBEDDED_TILES_N; i++) {
        if (EMBEDDED_TILES[i].z == z && EMBEDDED_TILES[i].x == tx &&
            EMBEDDED_TILES[i].y == ty) {
            *len = EMBEDDED_TILES[i].len;
            return EMBEDDED_TILES[i].data;
        }
    }
    return NULL;
}

/* Decode an embedded JPEG tile with LVGL's own tjpgd image decoder and blit
 * it (RGB888 -> RGB565) into the screen buffer. No raw ROM tjpgd calls:
 * LVGL registers the decoder (LV_USE_TJPGD) and wraps the flash bytes as a
 * MEMFS stream, so decoding is fully handled by LVGL. */
static void blit_tile(int z, int tx, int ty, double wx, double wy)
{
    uint32_t len = 0;
    const uint8_t *data = embedded_tile(z, tx, ty, &len);
    if (!data)
        return;

    lv_image_dsc_t tile_dsc;
    lv_memzero(&tile_dsc, sizeof(tile_dsc));
    tile_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    tile_dsc.header.cf = LV_COLOR_FORMAT_UNKNOWN; /* encoded JPEG -> decode on open */
    tile_dsc.data = data;
    tile_dsc.data_size = len;

    lv_image_decoder_dsc_t dec_dsc;
    if (lv_image_decoder_open(&dec_dsc, &tile_dsc, NULL) != LV_RESULT_OK) {
        ESP_LOGE(TAG, "tile %d/%d/%d decode open failed", z, tx, ty);
        return;
    }

    g_tile_ox = tx * TILE - (int)lround(wx) + CX;
    g_tile_oy = ty * TILE - (int)lround(wy) + CY;

    lv_area_t full = {0, 0, (lv_coord_t)dec_dsc.header.w - 1,
                      (lv_coord_t)dec_dsc.header.h - 1};
    lv_area_t area = {LV_COORD_MIN, LV_COORD_MIN, LV_COORD_MIN, LV_COORD_MIN};

    while (lv_image_decoder_get_area(&dec_dsc, &full, &area) == LV_RESULT_OK) {
        const lv_draw_buf_t *db = dec_dsc.decoded;
        if (!db)
            break;
        const uint8_t *px = db->data;
        uint32_t stride = db->header.stride;
        for (lv_coord_t y = area.y1; y <= area.y2; y++) {
            int sy = g_tile_oy + y;
            if (sy < 0 || sy >= H)
                continue;
            for (lv_coord_t x = area.x1; x <= area.x2; x++) {
                uint32_t i = (uint32_t)(y - area.y1) * stride + (uint32_t)(x - area.x1) * 3;
                uint8_t r = px[i], g = px[i + 1], b = px[i + 2];
                uint16_t v = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
                int sx = g_tile_ox + x;
                if (sx >= 0 && sx < W)
                    g_map_buf[(size_t)sy * W + sx] = v;
            }
        }
    }
    lv_image_decoder_close(&dec_dsc);
    ESP_LOGI(TAG, "tile %d/%d/%d drawn", z, tx, ty);
}

/* World pixel (Web Mercator) for lat/lon at zoom z (TILE-px units). */
static void latlon_to_world(double lat, double lon, int z, double *wx, double *wy)
{
    double n = 256.0 * (1 << z);
    *wx = (lon + 180.0) / 360.0 * n;
    double latr = lat * M_PI / 180.0;
    *wy = (1.0 - log(tan(latr) + 1.0 / cos(latr)) / M_PI) / 2.0 * n;
}

/* Draw the route (screen coords) with an lv_line. */
static void set_route(const char *pts, double wx, double wy)
{
    if (!pts || !g_route)
        return;
    static lv_point_precise_t sp[MAX_ROUTE];
    double lat, lon;
    int n = 0;
    const char *p = pts;
    while (n < MAX_ROUTE && p && *p) {
        char *end = NULL;
        lat = strtod(p, &end);
        if (end == p) break;
        p = end;
        if (*p == ',') p++;
        lon = strtod(p, &end);
        if (end == p) break;
        p = end;
        double dwx, dwy;
        latlon_to_world(lat, lon, g_z, &dwx, &dwy);
        sp[n].x = (int32_t)lround(dwx - wx) + CX;
        sp[n].y = (int32_t)lround(dwy - wy) + CY;
        n++;
        while (*p == ' ' || *p == '\t') p++;
    }
    if (n >= 2) {
        lv_line_set_points(g_route_outline, sp, (uint32_t)n);
        lv_line_set_points(g_route, sp, (uint32_t)n);
    }
}

/* Shift the whole screen buffer by a small integer pixel delta so the map
 * scrolls under the (fixed-center) car without re-decoding every tile. The
 * exposed edges are filled with the fallback colour; newly entered tiles are
 * blitted over them afterwards in tilemap_show(). */
static void shift_map_buf(int dx, int dy)
{
    uint16_t *buf = g_map_buf;
    if (dy > 0) { /* content moves down */
        for (int y = H - 1; y >= dy; y--)
            memmove(buf + (size_t)y * W, buf + (size_t)(y - dy) * W, (size_t)W * 2);
        for (int y = 0; y < dy && y < H; y++)
            memset(buf + (size_t)y * W, 0x18, (size_t)W * 2);
    } else if (dy < 0) { /* content moves up */
        for (int y = 0; y < H + dy; y++)
            memmove(buf + (size_t)y * W, buf + (size_t)(y - dy) * W, (size_t)W * 2);
        for (int y = H + dy; y < H; y++)
            memset(buf + (size_t)y * W, 0x18, (size_t)W * 2);
    }
    if (dx > 0) { /* content moves right */
        for (int y = 0; y < H; y++) {
            memmove(buf + (size_t)y * W + dx, buf + (size_t)y * W, (size_t)(W - dx) * 2);
            for (int x = 0; x < dx && x < W; x++)
                buf[(size_t)y * W + x] = 0x1818;
        }
    } else if (dx < 0) { /* content moves left */
        for (int y = 0; y < H; y++) {
            memmove(buf + (size_t)y * W, buf + (size_t)y * W - dx, (size_t)(W + dx) * 2);
            for (int x = W + dx; x < W; x++)
                buf[(size_t)y * W + x] = 0x1818;
        }
    }
}

int tilemap_init(void)
{
    if (g_map_img)
        return 0;
    if (lvgl_port_init() != 0)
        return -1;

    /* 150 KB full-screen buffer — put it in PSRAM (8MB on this board) to keep
     * internal SRAM for LVGL/BLE; fall back to internal RAM if PSRAM is off. */
    g_map_buf = heap_caps_malloc((size_t)W * H * 2, MALLOC_CAP_SPIRAM);
    if (!g_map_buf)
        g_map_buf = malloc((size_t)W * H * 2);
    if (!g_map_buf) {
        ESP_LOGE(TAG, "buf alloc failed");
        return -1;
    }
    memset(g_map_buf, 0x18, (size_t)W * H * 2); /* dark fallback bg */

    g_map_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    g_map_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    g_map_dsc.header.w = W;
    g_map_dsc.header.h = H;
    g_map_dsc.data = (const uint8_t *)g_map_buf;
    g_map_dsc.data_size = (uint32_t)W * H * 2;

    g_map_img = lv_image_create(lv_screen_active());
    lv_obj_remove_flag(g_map_img, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(g_map_img, &g_map_dsc);

    /* route polyline: white outline under a thicker blue line (nav style) */
    g_route_outline = lv_line_create(lv_screen_active());
    lv_obj_set_style_line_color(g_route_outline, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_line_width(g_route_outline, 7, 0);
    lv_obj_set_style_line_rounded(g_route_outline, true, 0);
    lv_obj_remove_flag(g_route_outline, LV_OBJ_FLAG_CLICKABLE);

    g_route = lv_line_create(lv_screen_active());
    lv_obj_set_style_line_color(g_route, lv_color_hex(0x1B63E8), 0);
    lv_obj_set_style_line_width(g_route, 4, 0);
    lv_obj_set_style_line_rounded(g_route, true, 0);
    lv_obj_remove_flag(g_route, LV_OBJ_FLAG_CLICKABLE);

    /* car marker: blue dot with a white ring at the screen center */
    g_car = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_car, 14, 14);
    lv_obj_set_style_bg_color(g_car, lv_color_hex(0x1B63E8), 0);
    lv_obj_set_style_bg_opa(g_car, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_car, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(g_car, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(g_car, 2, 0);
    lv_obj_remove_flag(g_car, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g_car, LV_OBJ_FLAG_SCROLLABLE);

    /* banner + HUD must sit above the full-screen map image */
    lvgl_ui_foreground();

    ESP_LOGI(TAG, "tilemap ready (offline JPEG tiles)");
    return 0;
}

void tilemap_show(double lat, double lon, int z, const char *route_pts,
                  int speed, const char *to_street)
{
    if (!g_map_img)
        return;
    if (z < 1) z = 1;
    if (z > 19) z = 19;

    double wx, wy;
    latlon_to_world(lat, lon, z, &wx, &wy);

    /* the 2x2 tile block covering the 320x240 screen window */
    int x0 = (int)floor((wx - CX) / TILE);
    int x1 = (int)floor((wx + CX - 1) / TILE);
    int y0 = (int)floor((wy - CY) / TILE);
    int y1 = (int)floor((wy + CY - 1) / TILE);

    int iwx = (int)lround(wx), iwy = (int)lround(wy);
    int dx = iwx - g_last_wx, dy = iwy - g_last_wy;

    if (z != g_z || abs(dx) >= TILE || abs(dy) >= TILE) {
        /* zoom / big jump: rebuild the whole visible block */
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                blit_tile(z, x, y, wx, wy);
    } else {
        /* pan: scroll existing pixels, then blit tiles that just entered */
        shift_map_buf(-dx, -dy);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                if (x < g_x0 || x > g_x1 || y < g_y0 || y > g_y1)
                    blit_tile(z, x, y, wx, wy);
    }
    g_last_wx = iwx; g_last_wy = iwy;
    g_z = z; g_x0 = x0; g_x1 = x1; g_y0 = y0; g_y1 = y1;

    set_route(route_pts, wx, wy);
    lv_obj_set_pos(g_car, CX - 7, CY - 7);

    if (to_street)
        lvgl_set_banner(to_street);
    lvgl_set_speed(speed);
    lv_obj_invalidate(g_map_img);
    lv_refr_now(NULL);
}
