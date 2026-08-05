#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Vector map renderer for the ESP32-S3 + ILI9341 320×240.
 *
 * Input is a compact map XML packet produced by the phone app:
 *
 *   <map z="15" cx="10.7701" cy="106.6920">
 *     <area cls="water|park" pts="10.7621,106.6601 10.7628,106.6609 ..."/>
 *     <road cls="major" pts="10.7621,106.6601 10.7628,106.6609 10.7635,106.6618"/>
 *     <road cls="minor" pts="..."/>
 *     <route pts="..."/>
 *     <marker x="10.7701" y="106.6920"/>
 *     <poi name="Ben Thanh" x="10.7726" y="106.6981"/>
 *   </map>
 *
 * Renders into an internal RGB565 framebuffer then blits to the display.
 */

/** Initialize (allocates the framebuffer). Call once after ili9341_init(). */
int map_render_init(void);

/** Self-test: fill fb with horizontal color bars and blit (verifies blit path). */
void map_render_test_bars(void);

/** Parse + render a map XML packet. Returns 0 on success, -1 on parse error. */
int map_render_show(const char *xml, size_t len);

/** Benchmark: parse+draw vs blit over `frames` frames, logs ms/frame + FPS. */
void map_render_fps_test(const char *xml, size_t len, int frames);

/** Free the framebuffer (optional). */
void map_render_deinit(void);

/* ================= Turn-by-turn maneuver screen =================
 * The round-device nav UI: a big direction arrow, distance to the maneuver
 * and the current/next street names, with the device speed band on top.
 * Rendered into the same circular viewport as the map. */

typedef enum {
    NAV_STRAIGHT = 0,
    NAV_LEFT,
    NAV_RIGHT,
    NAV_SLIGHT_LEFT,
    NAV_SLIGHT_RIGHT,
    NAV_SHARP_LEFT,
    NAV_SHARP_RIGHT,
    NAV_UTURN,
    NAV_ARRIVE,
} nav_maneuver_t;

typedef struct {
    nav_maneuver_t maneuver; /* direction of the big arrow */
    int distance_m;          /* meters to the maneuver (0 = arrived) */
    const char *from;        /* current street, UTF-8 (diacritics auto-stripped) */
    const char *to;          /* street after the maneuver, UTF-8 */
    int speed;               /* km/h, shown in the bottom HUD band */
} nav_screen_t;

/** Render a full-screen turn-by-turn maneuver into the framebuffer + blit. */
void map_render_nav_show(const nav_screen_t *nav);

#ifdef __cplusplus
}
#endif
