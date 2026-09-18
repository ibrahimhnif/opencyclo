# GPS acquisition comparison mode

Archived comparison procedure: baseline bypass has now been removed from the
firmware. Normal acknowledged receiver configuration and verified PMREQ standby
are restored; `GPS_DIAGNOSTICS_ENABLED=1` remains enabled. The schema-3 CSV
`baseline_mode` column remains for compatibility and is always 0 in new logs.
Instructions below describe the historical test build, not the current firmware.

Latest filter/logging revision: qualified UBX 3D fixes may use four satellites
(NMEA fallback still five). CSV schema 3 adds deliberate sampling counters;
see [diagnostics](gps-diagnostics.md). Prior schema 2 logs remain readable.

For acquisition comparisons, record separately (1) time to receiver 3D fix,
(2) time to filter acceptance, and (3) first saved GPX point. A lower satellite
gate can shorten (2), but does not itself shorten receiver acquisition (1).
Compare a short standby/wake cycle with a documented full GPS power cycle
at the same unobstructed location and wiring. Do not erase backup data to
force a cold start, and do not call an ESP reboot a cold GPS start. Compare
with the previously successful firmware only using a verified historical
revision; no guessed old image should be flashed. Field trials require user
coordination and are not completed by the host tests.

This diagnostic build uses both `GPS_DIAGNOSTICS_ENABLED=1` and
`GPS_BASELINE_MODE=1` in platformio.ini. No factory reset or backup-data erase
is performed. No automatic flash is performed by changing these parameters.

Baseline mode still opens UART at the board's existing baud rate, sends the
wake byte and polls receiver identification. It skips our CFG-VALSET
measurement-rate/navigation-model/output configuration and skips PMREQ
standby. The receiver stays powered during ESP sleep: expect greater battery
drain. Return `GPS_BASELINE_MODE` to 0 after the comparison and rebuild/flash.
GPS quality filtering is unchanged to avoid changing two variables at once.

This is a bypass of OpenCyclo's configuration, NOT verified factory defaults.
Previous receiver RAM/backup configuration may persist. Firmware does not
change constellation settings or automatically clear backup data. Capture a
new log after flashing; note whether GPS power was physically disconnected.
Do not assume an ESP reset alone resets the GPS receiver.

## CSV schema 2

Existing columns remain, with `baseline_mode`, `pvt_fix_type`, `pvt_flags`,
`pvt_flags3` and `nmea` appended. PVT fields are meaningful only on PVT
observations (`accuracy_valid=1`); zero on NMEA-only data means unavailable,
not proof of receiver fix type 0.

Checksum-verified GSV/GSA/GGA sentences are captured individually, including
messages without a fix. `reason=nmea-observation`, `accepted=-1` marks an
observation row, not a filter decision. Do not include these rows when
calculating filter acceptance rate. The raw snapshot in such rows may precede
the just-received sentence. Use the complete quoted `nmea` field for analysis.

GSV gives satellite IDs, elevation, azimuth and C/N0, grouped by constellation
and signal. Preserve message group/count/signal IDs; do not add counts across
signals as if all satellites were distinct. GSA/GGA provide complementary fix
status and DOP fields. These messages are observed if already enabled on the
receiver: baseline mode does not configure output, and absence of GSV is not
proof of zero visible satellites. See the official
[u-blox M10 SPG 5.10 interface description](https://content.u-blox.com/sites/default/files/u-blox-M10-SPG-5.10_InterfaceDescription_UBX-21035062.pdf).

The queue remains bounded at 64 entries. Extra signal messages add traffic;
inspect `dropped_total` before claiming a complete observation history. GPS
debug task stack is 8 KiB and logger stack 12 KiB; CSV formatting scratch is
static rather than on the logger stack. Debug log files stop at the boot
size cap or a write error. Original SD logs are not modified.

## Auto-pause fix

Unknown, invalid or stale speed resets auto-pause/resume timers, without
changing ride state. A valid zero can still auto-pause after five seconds.
Fresh BLE wheel speed can drive auto-pause without GPS. GPS data must be
fresh and valid. A ride already paused before losing speed remains paused;
this change never starts or resumes a ride without a valid trigger.

## Next field check

1. Flash this build and preserve the previous logs.
2. Start a ride without a fix; it must not pause after five seconds merely
   because speed is unavailable (unless a connected wheel sensor reports
   a real zero).
3. Test stationary outdoors with the GPS antenna unobstructed. Record the
   duration and whether a fix appears. Finish/save before removing SD.
4. Compare CSV signal reports, original receiver fix type/flags, accepted
   fixes and baseline_mode. A bypass test alone cannot prove antenna, power
   or configuration is the cause.

Host parser, speed-validity and diagnostic-writer tests cover these changes.
Hardware/field behavior still requires the new firmware and an outdoor test.
