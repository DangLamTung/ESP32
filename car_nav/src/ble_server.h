#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BLE GATT server (NimBLE peripheral) for the car-nav display.
 *
 * Advertises as "EINK-MAP". Exposes one write characteristic that accepts
 * the compact map XML packet (same schema as map_render). The packet is
 * finalized when `</map>` arrives (or a 0x00 terminator), then rendered.
 */

/** Initialize the BLE GATT server. Call once after boot. */
void ble_server_init(void);

/** True once a map packet has been received from the phone. */
extern volatile bool ble_got_data;

/** True while a BLE client is connected. */
extern volatile bool ble_connected;

#ifdef __cplusplus
}
#endif
