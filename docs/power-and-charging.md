# Power and charging

These controls target the LCDWiki **ES3C28P / ES3N28P** board in this
repository. Its BOOT/download button (KEY2) connects GPIO0 to ground.

| Action | Result |
| --- | --- |
| Apply USB or battery power | Boot normally |
| Short press BOOT | Turn the screen off/on; recording and BLE continue |
| Touch a sleeping screen | Wake the screen; the wake touch does not press a ride button |
| Hold BOOT for 2 seconds | Open Power & Battery |
| Settings → POWER / CHARGING | Open the same menu |
| POWER OFF | Close the GPX log, quiesce BLE, request GPS standby, stop BLE/display, enter deep sleep |
| Press BOOT after POWER OFF | Wake into Idle; Start explicitly begins a session |
| RESTART | Close the log and restart into Idle |
| BACK or short BOOT press in menu | Return to the current page |

With the optional USB power detector configured:

| While powered off | Result |
| --- | --- |
| Plug in USB | Wake into CHARGE MODE, showing battery estimate and an ON button |
| Tap ON or press BOOT in charging mode | Start the normal cycling computer |
| Unplug USB before selecting ON | Return to deep sleep |
| Select POWER OFF while USB is already connected | Enter charging mode after saving the ride |

Charging mode starts before the GPS, barometer, BLE and logger tasks. It does
not record a ride or advertise Bluetooth. ON starts these tasks normally.
A cold boot or RESET still starts normally; the off/charging state is retained
across deep sleep only.

Release BOOT before confirming power off. A button held through boot or wake
is ignored until released. RESET also boots the board, but bypasses orderly
log closure. Holding BOOT while applying power or pressing RESET retains the
board's normal download-mode behavior.

Power off and restart end the current ride; ride totals are not restored.
Shutdown waits for the logger and BLE task to become quiescent. If either
does not respond within five seconds, or BOOT remains held, the operation
is canceled. Firmware updates exclude shutdown/restart; disconnecting the
phone aborts an incomplete update and releases that restriction. GPS standby
is the final fallible step before deep sleep; failure cancels shutdown, attempts
UART wake/reconfiguration, and releases logger/BLE quiescence. Restart does not
request GNSS standby.

## Charging hardware

The board's TP4054 charges a compatible single-cell 3.7 V nominal Li-ion/Li-Po
battery from USB automatically. Firmware does not start or stop charging.
Use the board battery connector with the correct polarity and a battery
compatible with its charger. Charging remains a hardware function while
the ESP32 sleeps.

The schematic routes neither charger CHRG nor USB VBUS detection to an ESP32
GPIO. The default build therefore cannot wake on charger insertion. It shows
battery voltage and an approximate percentage, plus **Charge status unavailable**.
It cannot reliably report USB connected, charging, charge complete, or battery
absent from voltage alone. The percentage is a voltage estimate and is less useful during
charging or with no battery connected.

For automatic charger wake, connect a verified external VBUS detector to an
unused RTC-capable expansion GPIO. Its output must be HIGH when USB power is
present and reliably LOW when unplugged, at ESP32-safe logic levels; **do not
connect 5 V VBUS directly to a GPIO**. Once this hardware exists on GPIO2, use:

```bash
pio run -e esp32-s3-usb-sense
```

The default `pio run` still builds the unmodified board, with
`PIN_USB_POWER_SENSE=-1`. The optional build sets it to GPIO2. The code also
allows free expansion GPIO14 or GPIO21 if configured for your wiring.
BOOT uses EXT0 active-low wake; the USB detector uses EXT1 active-high wake.
Do not enable the optional build with a floating/unconnected detector pin.

This detector reports USB presence, not charger current or charge completion.
CHARGE MODE is the device mode; it does not assert that current is entering
the battery. Reading the charger's actual state would require additional
hardware. There is no
software-controlled supply cutoff in the documented circuit. **POWER OFF
is deep sleep**, and connected GPS, touch, regulators, and other board
hardware may continue drawing current. Measure whole-device sleep current
before relying on it for long storage; full power isolation requires hardware.

Hardware reference: [LCDWiki board schematic](https://www.lcdwiki.com/res/ES3C28P/2.8inch_ESP32-S3_Display_Schematic.pdf).
Sleep API reference: [Espressif ESP32-S3 sleep modes](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32s3/api-reference/system/sleep_modes.html).

## Validation

Run `python3 tests/run_native_tests.py` for button handling, shutdown ordering,
failure recovery, OTA exclusion, and GPX filename/closure regressions.
The tests cover USB-wake routing, ON taps, canceled drags and unplugging in
charging mode using simulated hardware inputs.
Run `pio run` for the ESP32-S3 firmware build.

On-device checks still required:

1. Short BOOT press blanks the display while ride logging/BLE continue. Touch
   wakes without triggering a ride action.
2. Hold BOOT, release, and use BACK. Repeat through Settings. Swiping out of
   a menu button must not activate it.
3. Power off during active and paused rides, and with no SD card. Wake with
   BOOT and verify existing GPX files end with `</gpx>` and remain unchanged
   after another ride. Check RESTART the same way.
4. Begin an OTA update and verify shutdown is blocked. Abort or disconnect
   and verify shutdown becomes available again.
5. Confirm the backlight remains off in deep sleep, measure supply current,
   and verify USB charging with a suitable battery and measurement equipment.
6. With the external detector installed: power off, plug in USB, and verify
   CHARGE MODE appears without BLE advertising or a new ride log. Tap ON and
   verify normal operation. Repeat and unplug instead: the display should
   turn off. Reconnect USB and verify charging mode reappears. Check BOOT wake
   and power-off while USB is already connected as well.

## Firmware power optimization: first pass

- RouteIO now waits for task notifications instead of polling every 5 ms.
  Each packet/disconnect wakes it immediately; a one-second maintenance wake
  retains transfer timeout and SD cleanup retries. This reduces idle task
  wake frequency, not the ESP32 clock or GPS update rate.
- When BME280 detection fails at both addresses after the existing retries,
  the barometer publishes one invalid sample and deletes its task instead of
  producing invalid samples at 4 Hz forever. Attaching a sensor still requires
  reboot, as before. Sampling for an installed BME280 is unchanged.
- Recording, navigation, camera controls, display timing and Bluetooth
  connection settings are unchanged by this pass.

No whole-device current reduction has been measured yet. Compare battery-side
current under the same brightness, GPS fix, BLE connections and SD workload:
Idle, recording, screen off, and deep sleep. USB input current while charging
includes battery charge current and is not a direct measurement of device load.

## RushFPV GNSS Micro: software standby

The user identified the module as RushFPV GNSS Micro (four wires: 5 V, GND,
TX, RX). Its official specification identifies a u-blox M10 engine with an
internal backup battery. No additional wake wire is required for UART wake.
[RushFPV product/specification](https://rushfpv.net/products/rushfpv-gnss-micro).

Implemented (physical validation pending):

- Boot releases the GPS TX hold, sends a UART wake byte, waits 500 ms, and
  polls UBX-MON-VER with a checksum-checked, bounded parser. Logs show actual
  software/hardware/protocol strings. No GNSS cold reset or backup erasure.
- Only confirmed protocol **34.10** enables the current standby/configuration
  implementation. Other versions retain ordinary NMEA reception but need
  review against their matching interface manual before standby is enabled.
- Acknowledged CFG-VALSET restores 5 Hz and portable mode in RAM only. It
  replaces the old unchecked legacy configuration frames. Missing ACK is
  logged, not reported as successful configuration.
- Power Off polls identity again, sends the 16-byte UBX-RXM-PMREQ request
  (backup + force, duration zero, UART RX edge wake), then requires 1.2 seconds
  of UART silence within 2.5 seconds. PMREQ is a command, not a CFG ACK exchange.
  Silence after a fresh identity reply is evidence, not proof of low current.
- TX is held at its idle level during deep sleep to avoid spurious UART wake.
  Charging-only mode does not start the GPS task or intentionally send UART
  traffic. Screen Off and ride Pause leave GNSS operating.
- Missing/unrecognized GNSS, write failure, continuing output, or inability to
  acquire UART ownership cancels Power Off with `GPS standby failed. Retry.`
  Restart remains available. Failed standby attempts wake the receiver again;
  stale fixes and queued pre-standby data are cleared.

UART access is serialized between the GPS task and shutdown. Transactions have
bounded read/write waits. Backup retention supports hot start but does not
guarantee a fixed time-to-fix after long storage or under poor satellite view.

Protocol reference: [u-blox M10 SPG 5.10 interface, MON-VER, CFG-VALSET and
RXM-PMREQ](https://content.u-blox.com/sites/default/files/u-blox-M10-SPG-5.10_InterfaceDescription_UBX-21035062.pdf).

Before relying on power savings: capture boot version/configuration logs, test
Power Off/BOOT wake repeatedly, verify a fresh GPS fix after wake, and measure
whole-device battery-side current in active, screen-off and deep-sleep states.
Check charging-only behavior separately if a USB detector has been installed.
An unsupported protocol log should lead to updating the allowlist against its
manual, not bypassing identification. Host tests cover packet checksums,
identity gating, ACK/NAK, quiet/continuing UART, timeouts and shutdown rollback;
they do not emulate GNSS electrical behavior, RF acquisition or GPIO retention.

Timer USB polling remains disabled: the stock board still has no USB sensing
input. A timer cannot replace that missing hardware signal.
