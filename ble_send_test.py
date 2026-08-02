#!/usr/bin/env python3
"""BLE send test: scan for the ESP32 "EINK-MAP" peripheral, connect, and push
a map XML packet to its write characteristic. The ESP32 renders it on the LCD.
"""
import asyncio

from bleak import BleakScanner, BleakClient

SVC_UUID = "5a7e1000-2b2f-4f66-9f9a-5c0f8e1a2b3c"
CHAR_UUID = "5a7e1001-2b2f-4f66-9f9a-5c0f8e1a2b3c"

# Compact map XML (same schema as the ESP32 renderer) — Bến Thành, HCMC.
MAP_XML = (
    '<map z="16" cx="10.7720" cy="106.6975" speed="42">'
    '<area cls="water" pts="10.7752,106.7022 10.7766,106.7008 10.7752,106.6992 10.7738,106.7000"/>'
    '<area cls="park" pts="10.7704,106.6948 10.7720,106.6950 10.7723,106.6962 10.7707,106.6961"/>'
    '<road cls="major" pts="10.7735,106.6980 10.7715,106.6978 10.7695,106.6976"/>'
    '<road cls="major" pts="10.7730,106.7000 10.7718,106.6995 10.7705,106.6990"/>'
    '<road cls="secondary" pts="10.7745,106.6940 10.7725,106.6955 10.7705,106.6970"/>'
    '<road cls="minor" pts="10.7710,106.6950 10.7695,106.6970"/>'
    '<road cls="minor" pts="10.7740,106.6990 10.7728,106.6985"/>'
    '<road cls="minor" pts="10.7748,106.6962 10.7730,106.6963 10.7712,106.6965"/>'
    '<route pts="10.7730,106.6975 10.7724,106.6977 10.7718,106.6978 10.7712,106.6982 10.7706,106.6992"/>'
    '<car h="135"/>'
    '</map>\x00'
)


async def main():
    print("Scanning for EINK-MAP (10s)...")
    dev = await BleakScanner.find_device_by_name("EINK-MAP", timeout=10)
    if dev is None:
        print("NOT FOUND — is the ESP32 powered and advertising?")
        return
    print("Found:", dev.address)

    async with BleakClient(dev.address) as client:
        print("Connected. MTU:", client.mtu_size)
        data = MAP_XML.encode("utf-8")
        await client.write_gatt_char(CHAR_UUID, data, response=True)
        print("Wrote %d bytes (map XML)" % len(data))
        await asyncio.sleep(1.5)
    print("Done — check the display.")


asyncio.run(main())
