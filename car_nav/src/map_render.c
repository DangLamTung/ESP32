/**
 * map_render.c — vector map renderer for ST7789 240×198.
 *
 * Draws into an in-memory RGB565 framebuffer (240*198*2 ≈ 95 KB, fits the
 * ESP32-C3 heap), then blits once with st7789_draw_image().
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <limits.h>

#include "map_render.h"
#include "st7789.h"

#define W ST7789_WIDTH
#define H ST7789_HEIGHT

static const char *TAG = "map_render";

static uint16_t *fb = NULL;

/* ---- RGB565 palette (OSM light map inside a dark round device frame) ---- */
#define C_DARK        0x0000 // device frame (outside circle)
#define C_MAP_BG      0xEF7E // OSM land (#F2EFE9)
#define C_CASING      0xB596 // soft grey-brown road outline
#define C_ROUTE_CASING 0xFFFF // white casing under route
#define C_ROAD_MAJ    0xFFE0 // yellow
#define C_ROAD_SEC    0xFD20 // orange
#define C_ROAD_MIN    0xFFFF // white
#define C_WATER       0x4A7F // soft blue
#define C_PARK        0x5EA4 // soft green
#define C_ROUTE       0x001F // blue route
#define C_MARKER      0xF800 // red car
#define C_POI         0x001F // blue dot
#define C_TEXT        0x0000 // black
#define C_HUD_TEXT    0xFFFF // white (speed on dark band)
#define C_HUD_BAND    0x39E7 // dark grey band
#define C_BORDER      0x8410 // grey divider
#define C_WAVE_A      0x780F // purple
#define C_WAVE_B      0x001F // blue
#define C_WAVE_C      0x3DEF // light blue

/* Local-map viewport geometry: big circular map + top waves + speed band */
#define MAP_CX 120
#define MAP_CY 90
#define MAP_R  92
#define HUD_Y  164

/* ---- Framebuffer primitives ---- */
static inline void fb_px(int x, int y, uint16_t c)
{
    if (x >= 0 && x < W && y >= 0 && y < H)
        fb[(size_t)y * W + x] = c;
}

static void fb_fill(uint16_t c)
{
    for (int i = 0; i < W * H; i++)
        fb[i] = c;
}

static void fb_line(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb_px(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* Draw a line of `th` pixels thickness (orthogonal box around the segment). */
static void fb_thick_line(int x0, int y0, int x1, int y1, int th, uint16_t c)
{
    if (th <= 1) {
        fb_line(x0, y0, x1, y1, c);
        return;
    }
    int off = th / 2;
    for (int dy = -off; dy <= off; dy++)
        for (int dx = -off; dx <= off; dx++)
            fb_line(x0 + dx, y0 + dy, x1 + dx, y1 + dy, c);
}

static void fb_rect(int x0, int y0, int x1, int y1, uint16_t c)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            fb_px(x, y, c);
}

/* Filled disc (small markers). */
static void fb_disc(int cx, int cy, int r, uint16_t c)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r)
                fb_px(cx + dx, cy + dy, c);
}

/* Even-odd scanline fill of a polygon (already projected to screen px). */
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

/* ---- Local-map HUD decorations ---- */

/* One wavy band (sine-offset horizontal stripe). Uses float math — the
 * ESP32-C3 has only a single-precision FPU, so double sin() is very slow. */
static void draw_wave(int base, uint16_t c, float ph)
{
    for (int x = 0; x < W; x++) {
        int y = base + (int)(3.0f * sinf(x / 14.0f + ph));
        for (int dy = 0; dy < 5; dy++) {
            int yy = y + dy;
            if (yy >= 0 && yy < H)
                fb[(size_t)yy * W + x] = c;
        }
    }
}

/* Decorative blue/purple waves across the top (reference-device look). */
static void draw_waves(void)
{
    draw_wave(2, C_WAVE_A, 0.0f);  /* purple */
    draw_wave(11, C_WAVE_B, 1.7f); /* blue */
    draw_wave(20, C_WAVE_C, 3.2f); /* light blue */
}

/* Bottom HUD band with a big speed readout. */
static void draw_hud(int speed)
{
    for (int y = HUD_Y; y < H; y++) {
        uint16_t *row = &fb[(size_t)y * W];
        for (int x = 0; x < W; x++)
            row[x] = C_HUD_BAND;
    }
    fb_line(0, HUD_Y - 1, W - 1, HUD_Y - 1, C_BORDER); /* divider */

    char buf[16];
    snprintf(buf, sizeof(buf), "%d km/h", speed);
    int n = (int)strlen(buf);
    int x0 = (W - n * 8 * 2) / 2; /* 2x font: 16 px per char */
    if (x0 < 0) x0 = 0;
    st7789_draw_string_fb_scaled(fb, (uint16_t)x0, (uint16_t)(HUD_Y + 2),
                                 buf, 2, C_HUD_TEXT, C_HUD_BAND);
}

/* ---- Web Mercator projection ---- */
static double world_x(double lon, int z)
{
    return (lon + 180.0) / 360.0 * (256.0 * (1 << z));
}

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

/* ---- Tiny XML parsing (our own compact schema) ---- */

/* Extract an attribute value from `s` (which points at `name="...`). */
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

/* Parse "lat,lon lat,lon ..." into arrays. Returns point count. */
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

static uint16_t class_color(const char *cls)
{
    if (strcmp(cls, "major") == 0 || strcmp(cls, "motorway") == 0 ||
        strcmp(cls, "trunk") == 0)
        return C_ROAD_MAJ;
    if (strcmp(cls, "secondary") == 0 || strcmp(cls, "primary") == 0)
        return C_ROAD_SEC;
    return C_ROAD_MIN;
}

int map_render_init(void)
{
    if (fb)
        return 0;
    fb = heap_caps_malloc((size_t)W * H * 2, MALLOC_CAP_8BIT);
    if (!fb) {
        ESP_LOGE(TAG, "fb alloc failed");
        return -1;
    }
    return 0;
}

/* Blit the framebuffer to the display. The ST7789 wire expects RGB565 HIGH
 * byte first, so we byte-swap in place (the whole frame is redrawn before
 * every blit, so in-place swapping is safe).
 *
 * NOTE: a single 95 KB DMA transfer gets TRUNCATED by the ESP32-C3 SPI DMA
 * (only ~1/4 of the screen fills — the working paths in map_fetch/text all
 * use small per-row transfers). So we send the frame in row-chunks: few
 * enough transactions to stay fast, small enough to be DMA-safe. */
static void fb_blit(void)
{
    for (size_t i = 0; i < (size_t)W * H; i++) {
        uint16_t c = fb[i];
        fb[i] = (uint16_t)((c >> 8) | (c << 8)); /* high byte first */
    }
    const int ROWS_PER_CHUNK = 64;
    for (int y0 = 0; y0 < H; y0 += ROWS_PER_CHUNK) {
        int y1 = y0 + ROWS_PER_CHUNK - 1;
        if (y1 >= H)
            y1 = H - 1;
        st7789_set_window(0, y0, W - 1, y1);
        st7789_write_pixels_dma(&fb[(size_t)y0 * W],
                                (size_t)(y1 - y0 + 1) * W * 2);
    }
}

void map_render_test_bars(void)
{
    if (!fb && map_render_init() != 0)
        return;
    const uint16_t bars[] = { 0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000 }; // R,G,B,W,black
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            fb[(size_t)y * W + x] = bars[(y * 5) / H];
    fb_blit();
}

/* Parse + draw a map XML packet into the framebuffer (no blit). */
static int render_to_fb(const char *xml, size_t len)
{
    if (!fb && map_render_init() != 0)
        return -1;

    /* Buffers are static (BSS) — large stack arrays overflow the ~3.5 KB
     * main task stack and panic the ESP32-C3 (RTC_SW_SYS_RST reset loop). */
    static char val[64];
    static char pts[4096];
    static char cls[16];
    static char nm[64];
    static double la[512], lo[512];
    static int xs[512], ys[512];

    int z = 15;
    double cx = 10.7701, cy = 106.6920; // defaults
    int speed = 0;

    fb_fill(C_MAP_BG);

    /* <map ...> tag attributes */
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
        if (z < 1) z = 1;
        if (z > 19) z = 19;
    }
    ESP_LOGI(TAG, "map z=%d cx=%.4f cy=%.4f len=%u", z, cx, cy, (unsigned)len);

    /* Scan for elements in order: <area>, <road>, <route>, <marker>, <poi> */
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

        /* pick the earliest tag */
        if (a && (!next || a < next)) { next = a; kind = T_AREA; }
        if (c && (!next || c < next)) { next = c; kind = T_CAR; }
        if (t && (!next || t < next)) { next = t; kind = T_ROAD; }
        if (u && (!next || u < next)) { next = u; kind = T_ROUTE; }
        if (m && (!next || m < next)) { next = m; kind = T_MARKER; }
        if (q && (!next || q < next)) { next = q; kind = T_POI; }

        if (!next)
            break;

        if (kind == T_AREA) {
            /* Filled polygon: <area cls="water|park" pts="..."/> */
            strcpy(cls, "water");
            pts[0] = '\0';
            attr(next, "cls", cls, 16);
            attr(next, "pts", pts, sizeof(pts));
            int n = parse_pts(pts, la, lo, 256);
            uint16_t col = (strcmp(cls, "park") == 0 || strcmp(cls, "grass") == 0)
                               ? C_PARK : C_WATER;
            for (int i = 0; i < n; i++)
                project(la[i], lo[i], z, cx, cy, &xs[i], &ys[i]);
            fb_fill_poly(xs, ys, n, col);
        } else if (kind == T_ROAD) {
            strcpy(cls, "minor");
            pts[0] = '\0';
            attr(next, "cls", cls, 16);
            attr(next, "pts", pts, sizeof(pts));
            int n = parse_pts(pts, la, lo, 256);
            uint16_t col = class_color(cls);
            int th = (col == C_ROAD_MAJ) ? 3 : (col == C_ROAD_SEC ? 2 : 1);
            for (int i = 1; i < n; i++) {
                int x0, y0, x1, y1;
                project(la[i - 1], lo[i - 1], z, cx, cy, &x0, &y0);
                project(la[i], lo[i], z, cx, cy, &x1, &y1);
                fb_thick_line(x0, y0, x1, y1, th + 2, C_CASING); /* outline */
                fb_thick_line(x0, y0, x1, y1, th, col);
            }
        } else if (kind == T_ROUTE) {
            pts[0] = '\0';
            attr(next, "pts", pts, sizeof(pts));
            int n = parse_pts(pts, la, lo, 512);
            for (int i = 1; i < n; i++) {
                int x0, y0, x1, y1;
                project(la[i - 1], lo[i - 1], z, cx, cy, &x0, &y0);
                project(la[i], lo[i], z, cx, cy, &x1, &y1);
                fb_thick_line(x0, y0, x1, y1, 6, C_CASING); /* OSM grey casing */
                fb_thick_line(x0, y0, x1, y1, 4, C_ROUTE);
            }
        } else if (kind == T_MARKER) {
            double mx = cx, my = cy;
            if (attr(next, "x", val, sizeof(val)))
                my = atof(val);
            if (attr(next, "y", val, sizeof(val)))
                mx = atof(val);
            int sx, sy;
            project(my, mx, z, cx, cy, &sx, &sy);
            fb_disc(sx, sy, 5, 0xFFFF);   /* white ring */
            fb_disc(sx, sy, 4, C_MARKER); /* red body */
            fb_disc(sx, sy, 1, 0xFFFF);   /* highlight dot */
        } else if (kind == T_CAR) {
            /* Car at screen center (GPS mode). Optional heading:
             * <car h="deg"/>, 0 = north, clockwise. Navigation arrow. */
            double h = 0;
            if (attr(next, "h", val, sizeof(val)))
                h = atof(val);
            double hr = h * M_PI / 180.0;
            int cx = W / 2, cy = H / 2;
            int ax = cx + (int)lround(8 * sin(hr));
            int ay = cy - (int)lround(8 * cos(hr));
            int bx = cx + (int)lround(5 * sin(hr + 2.4));
            int by = cy - (int)lround(5 * cos(hr + 2.4));
            int dx = cx + (int)lround(5 * sin(hr - 2.4));
            int dy = cy - (int)lround(5 * cos(hr - 2.4));
            fb_line(ax, ay, bx, by, C_TEXT);   /* triangle outline */
            fb_line(bx, by, dx, dy, C_TEXT);
            fb_line(dx, dy, ax, ay, C_TEXT);
            fb_disc(cx, cy, 2, C_MARKER);      /* red center dot */
        } else { /* POI */
            double px = cy, py = cx;
            nm[0] = '\0';
            if (attr(next, "x", val, sizeof(val)))
                py = atof(val);
            if (attr(next, "y", val, sizeof(val)))
                px = atof(val);
            attr(next, "name", nm, sizeof(nm));
            int sx, sy;
            project(py, px, z, cx, cy, &sx, &sy);
            fb_disc(sx, sy, 2, C_MARKER);
            if (nm[0])
                st7789_draw_string_fb(fb, sx + 4, sy - 8, nm, C_TEXT, C_MAP_BG);
        }

        p = next + 1; /* continue scanning after this tag */
    }

    /* Full-screen OSM map with decorative waves + speed band on top. */
    draw_waves();
    draw_hud(speed);

    return 0;
}

int map_render_show(const char *xml, size_t len)
{
    int r = render_to_fb(xml, len);
    if (r != 0)
        return r;
    fb_blit();
    return 0;
}

/* Benchmark: parse+draw vs blit, log per-frame times and effective FPS. */
void map_render_fps_test(const char *xml, size_t len, int frames)
{
    if (!fb && map_render_init() != 0)
        return;
    if (frames < 1)
        frames = 1;

    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < frames; i++)
        render_to_fb(xml, len);
    int64_t t1 = esp_timer_get_time();
    for (int i = 0; i < frames; i++)
        fb_blit();
    int64_t t2 = esp_timer_get_time();

    double draw_ms = (double)(t1 - t0) / frames / 1000.0;
    double blit_ms = (double)(t2 - t1) / frames / 1000.0;
    double tot_ms = draw_ms + blit_ms;
    ESP_LOGI(TAG, "fps test (%d frames): draw %.2f ms, blit %.2f ms, "
                  "total %.2f ms/frame => %.1f fps",
             frames, draw_ms, blit_ms, tot_ms, 1000.0 / tot_ms);
}

void map_render_deinit(void)
{
    if (fb) {
        heap_caps_free(fb);
        fb = NULL;
    }
}
