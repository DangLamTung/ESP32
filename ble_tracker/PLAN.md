# ESP32-C3 Car Navigation Display

ESP32 fetches Google Maps tiles + directions via WiFi, renders on ST7789 via DMA.

## Architecture

```
ESP32-C3 (in car)              Google APIs                   Phone (optional)
───────────────                ───────────                   ────────────────
Sleep 30s                      │                            GPS → BLE send
Wake → WiFi ON                 │
┌─HTTPS GET────────────────→  Static Maps API
│  /staticmap?center=...       │
│  &zoom=15&size=240x198       │
│  &markers=...&path=...       │
│←── JPEG 8-15KB               │
│                              │
│  GET──────────────────────→  Directions API
│  /directions/json?origin=... │
│  &destination=...&mode=drive │
│←── JSON route polyline       │
│                              │
├─Decode JPEG (ROM tjpgd)      │
├─Draw route overlay            │
├─DMA→ST7789 240×198           │
└─WiFi OFF, Sleep 30s          │
5µA                            │
```

## Power Budget — 2×AA 2500mAh

| Mode | Current | Per day | Daily |
|------|---------|---------|-------|
| Deep sleep | 5 µA | 23.6h | 0.12 mAh |
| WiFi + HTTPS | 120 mA | 24s | 0.8 mAh |
| JPEG decode + DMA | 50 mA | 0.3s | 0.4 mAh |
| ST7789 display | 30 mA | 30 min | 15 mAh |
| **Total** | | | **~16 mAh/day → 150 days** |

## APIs (HTTPS)

**Static Map image:**
```
GET https://maps.googleapis.com/maps/api/staticmap
  ?center=10.762,106.660
  &zoom=15
  &size=240x198
  &markers=color:red|10.762,106.660
  &path=color:blue|weight:3|10.762,106.660|10.763,106.661|...
  &key=API_KEY
→ 240×198 JPEG (~8-15KB)
```

**Directions route:**
```
GET https://maps.googleapis.com/maps/api/directions/json
  ?origin=10.762,106.660
  &destination=10.772,106.670
  &mode=driving
  &key=API_KEY
→ JSON → "overview_polyline": "w`jwF~kpbVu@b@..."
```

## What We Already Have (Working)

From `src/`:
- ✅ ST7789 240×198 @ 40MHz SPI DMA
- ✅ WiFi HTTPS with TLS cert bundle
- ✅ JPEG decoder (ROM tjpgd, per-line DMA)
- ✅ Map fetcher (download + render)
- ✅ Per-line DMA GIF player pattern (no canvas)

## What To Add

1. Directions API → decode polyline → draw on map
2. GPS input (BLE from phone, or UART GPS module)
3. Power cycle: sleep → WiFi → fetch → display → sleep
4. BLE command interface for phone control
