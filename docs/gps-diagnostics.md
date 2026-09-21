# GPS diagnostics toggle

Latest: schema 3 adds `sampled_out_total`. Filter decisions are sampled at
most every 200 ms unless reason/satellite-count/fix-type/acceptance changes.
NMEA diagnostics are sampled once per second per talker/message; GSV pages
and signal IDs and GSA system IDs use distinct keys. Sixteen queue slots
are reserved for filter decisions. `sampled_out_total` counts deliberate
sampling, while `dropped_total` counts queue overflow. Neither counts GPX
loss. Receiver parsing, fusion and GPX rates are unchanged. Queue overflow
remains possible with slow/missing SD or unusually many signal groups.

PVT now accepts four satellites only with explicit valid 3D status, hAcc <=
15 m and sAcc <= 0.6 m/s. NMEA fallback retains five satellites and HDOP <= 2.
Freshness, outlier rejection and two-second qualification remain enabled.
Baseline mode remains on for the next comparison; no receiver power or
constellation settings were changed in this revision.

Update: the acquisition comparison build now writes schema 2 with receiver
fix flags and quoted GSV/GSA/GGA observations. See [GPS baseline test](gps-baseline-test.md)
for the current mode, extra columns and interpretation. The schema 1 details
below describe earlier logs; the same SD safety/size policy still applies.

Diagnostics are off in the default firmware. To enable them, build and flash
the dedicated environment, which defines `GPS_DIAGNOSTICS_ENABLED=1`:

```bash
pio run -e esp32-s3-gps-diag -t upload
```

`GPS_DIAGNOSTICS_ENABLED` is also settable directly in `platformio.ini`
(`=1` to enable, `=0` to disable) if you need a one-off variant, but keep the
shipped `esp32-s3-devkitc-1` env at `=0`. This is a build parameter, not yet a
screen/app switch. Normal GPX recording is unchanged when disabled;
diagnostic code and its buffer compile out.

With debug enabled, copy both:

- `/rides/<ride>.gpx`
- `/debug/gps_boot_<number>.csv`

from the SD card. CSV download through Flutter is not implemented. One CSV
can cover several rides in the same boot. `logger_gpx` links to the open ride;
`logger_ride_state` and filename describe the logger's state at drain time,
not an exact capture-time ride transition. Rows before starting a ride are
important for investigating slow acquisition. CSV and GPX contain private
location data; share only deliberately.

CSV schema 1 records monotonic capture/receiver milliseconds, receiver UTC,
raw fix/speed/accuracy validity, satellites, HDOP, horizontal/speed uncertainty,
age, raw and accepted coordinates/speed, filter reason and total dropped rows.
UTC may be invalid before the receiver knows the date; use uptime then.
`accepted=0` coordinates are diagnostic only, never a usable track point.
Reasons distinguish no-fix, stale, satellites, horizontal-accuracy,
speed-accuracy, HDOP, invalid values, jumps and initial qualification.
These are parsed receiver snapshots, not every UART byte or general firmware
console log. No Bluetooth credentials or sensor pairing data are recorded.

Capture follows GPS queue publication (new observations plus 500 ms
heartbeats), so repeated receiver epochs are expected and identifiable by
`received_ms`. This observes the existing parser/publication pipeline; it
does not guarantee every receiver packet if several arrive in one batch.

## Resource and failure policy

- 64-sample bounded RAM queue; GPS capture never waits for SD.
- Logger writes at most 16 queued samples per tick under the existing SD lock.
- Recording begins once SD mounts, even without a ride/fix. Before mount,
  only 64 samples can be buffered; overflow increments `dropped_total`.
- At 32 MiB per file or a write error, diagnostic writing stops for that boot.
  It never deletes existing files or retries repeatedly on a failing card.
- Existing filenames are never overwritten. Old logs accumulate across boots;
  manually archive/delete them when no longer needed.
- Flush every 10 seconds; orderly power-off closes the file. Abrupt power loss
  may lose buffered rows. Queued rows not yet drained at shutdown can be lost.
- Debug writes add SD traffic/power and may affect timing. Disable for normal
  use once the issue is diagnosed. GPX recording remains the primary ride file.

For the next test, turn the device on outdoors and retain the CSV from before
first fix through ride finish. Send that CSV and the corresponding GPX together
with the iGPSPORT export. The current code has build/host-test coverage, not
yet a hardware SD logging validation.
