#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize LVGL + display driver (flush -> ili9341) + UI objects
 *  (top banner, bottom speed HUD). Call once after ili9341_init(). Idempotent. */
int lvgl_port_init(void);

/** Create the full-screen map canvas backed by `buf` (RGB565, W*H*2 bytes).
 *  Call after lvgl_port_init(). Returns the canvas object or NULL. Used by
 *  the vector renderer; the tilemap path does not allocate it (saves RAM). */
lv_obj_t *lvgl_port_create_canvas(void *buf);

/** Update the top navigation banner (next street, pre-stripped ASCII). */
void lvgl_set_banner(const char *ascii);

/** Show/hide the top banner card (the map view shows it, the nav screen hides it). */
void lvgl_banner_visible(bool on);

/** Bring the top banner + bottom HUD to the front of the screen. Call after
 *  creating the full-screen map image so it doesn't cover the HUD. */
void lvgl_ui_foreground(void);

/** Update the bottom speed HUD. */
void lvgl_set_speed(int kmh);

/** Force a synchronous render of the current frame. Call from the app
 *  task only (LVGL is not thread-safe). */
void lvgl_refresh(void);

#ifdef __cplusplus
}
#endif
