#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Vector map renderer for the ESP32-C3 + ST7789 240×198.
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

/** Initialize (allocates the framebuffer). Call once after st7789_init(). */
int map_render_init(void);

/** Self-test: fill fb with horizontal color bars and blit (verifies blit path). */
void map_render_test_bars(void);

/** Parse + render a map XML packet. Returns 0 on success, -1 on parse error. */
int map_render_show(const char *xml, size_t len);

/** Benchmark: parse+draw vs blit over `frames` frames, logs ms/frame + FPS. */
void map_render_fps_test(const char *xml, size_t len, int frames);

/** Free the framebuffer (optional). */
void map_render_deinit(void);

#ifdef __cplusplus
}
#endif
