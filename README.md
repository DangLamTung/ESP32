# ESP32-C3 Super Mini Projects

**Chip:** ESP32-C3 (RISC-V, 160MHz, 4MB Flash, WiFi+BLE5)  
**Display:** ST7789 240×198 landscape  
**Framework:** PlatformIO (ESP-IDF v4.4.3 / Arduino)  
**Port:** `/dev/cu.usbmodem3101`

## Projects

| Project | Description | Framework |
|---------|-------------|-----------|
| `src/` (root) | GIF player with ST7789 + DMA | ESP-IDF |
| `bt_wifi_test/` | WiFi scan test | Arduino |
| `car_nav/` | Car navigation with map renderer | ESP-IDF |
| `blink_test/` | LED blink hello world | Arduino |
| `st7789_test/` | Display test | Arduino |
| `cat_gif/` | Cat GIF player | Arduino |
| `gif_player/` | Alternative GIF player | Arduino |
| `hwspi_test/` | HW SPI test | Arduino |
| `scripts/` | GIF conversion tools (Python) | — |

## Quick Start

```bash
cd ESP32
source .venv/bin/activate

# Build & flash any project
cd bt_wifi_test && pio run --target upload && pio device monitor

# Or the main GIF player
pio run --target upload && pio device monitor
```

## Wiring (Final)

| ST7789 | ESP32-C3 Super Mini |
|--------|---------------------|
| GND    | GND                 |
| VCC    | 3.3V                |
| SCL    | GPIO6               |
| SDA    | GPIO7               |
| RES    | GPIO0               |
| DC     | GPIO1               |
| CS     | GPIO10              |
| BLK    | 3.3V (or NC)        |

## GIF Tools

```bash
# Convert any GIF to C header (240×198, 5fps, 32 colors)
python3 scripts/gif_to_header.py my_gif.gif src/my_gif.h --size 240x198 --fps 5 --colors 32
```

## Performance History

| Resolution | SPI Speed | FPS |
|-----------|-----------|-----|
| 96×96 | 20 MHz | 17 FPS |
| 120×120 | 40 MHz | 43 FPS |
| 140×140 | 40 MHz | 35 FPS |
| 240×198 | 40 MHz | ~30 FPS |

## Troubleshooting

**No display output:** Hold BOOT + tap RST, then reflash  
**White bars:** Check ST7789_WIDTH/HEIGHT in `st7789.h` matches your panel  
**Watchdog reset:** The `esp_task_wdt_delete(NULL)` disables watchdog
