# OpenCyclo

A DIY GPS cycling computer firmware for the [lcdwiki 2.8" ESP32-S3
Display](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display) board
(ES3C28P/ES3N28P, ESP32-S3 N16R8), built with PlatformIO and the
Arduino framework.

## Features (v1)

- GPS speed, distance, trip stats (u-blox M10 via UART)
- Barometer-based altitude, grade %, total ascent (BMP280 via I2C)
- BLE sensor support: Speed/Cadence, Heart Rate, Cycling Power
  (standard GATT profiles — works with most off-the-shelf sensors)
- Touchscreen UI: Ride / Climb / Sensors / Settings pages
- GPX ride logging to microSD

## Status

🚧 In development. See
[`docs/superpowers/specs/2026-08-21-opencyclo-firmware-design.md`](docs/superpowers/specs/2026-08-21-opencyclo-firmware-design.md)
for the full architecture and design rationale.

## Hardware

- Board: lcdwiki 2.8" ESP32-S3 Display (ES3C28P, capacitive touch)
- GPS: u-blox M10 module (UART JST port)
- Barometer: BMP280 (I2C JST port)
- Any BLE cycling sensor advertising CSC (0x1816), HR (0x180D), or
  Cycling Power (0x1818)
- microSD card

Full verified pinout is in the design doc and `src/config/pins.h`.

## Building

```bash
pio run              # build
pio run -t upload    # flash
pio device monitor   # serial monitor (native USB CDC)
```

## License

TBD.
