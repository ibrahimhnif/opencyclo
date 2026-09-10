# GPX sync and explicit ride start

## Observed failure

On 2026-09-10 the device rebooted during a sync attempt with a core 0
`Double exception` and `CORRUPTED` backtrace. Firmware ELF SHA256 began
`76e90d7cb26fea1d`. The app reported BLE disconnection. The damaged backtrace
does not establish the exact corrupting instruction or prove stack overflow.

The old route callbacks synchronously took navigation/SD locks, checked FAT
free space, wrote files and validated routes on NimBLE's host task. These are
now run by a separate 12 KB RouteIO task. This removes a plausible stack and
latency hazard; a physical-device retest is still required to confirm resolution.

## Protocol and lifecycle

The subsequent `Missing extension byte` app error exposed a separate concrete
serialization bug: NimBLE 1.4's `setValue<T>` explicitly forwards `T`, so passing
`s.c_str()` selects raw pointer serialization. Route replies now always use
the byte-buffer + length overload. The BLE fake models that template behavior
so pointer bytes cannot silently pass the ACK tests again.

- Control (0x1904) and data (0x1905) writes enqueue one bounded request and
  immediately expose `BUSY` on the control characteristic.
- The app polls control reads until the final ACK or error, with a 30-second
  deadline. It does not resend writes during polling. Old firmware's immediate
  ACK remains supported. New firmware requires the updated app.
- Offset, CRC, file validation and final `SAVED` acknowledgement are retained.
- Disconnect only requests cleanup; it never performs SD I/O in the callback.
  Pending work is discarded and a busy SD is retried before new work is accepted.
- Shutdown detaches the response characteristic before BLE deletes it.
- Worker control-command logs include elapsed time and remaining stack.

Boot always starts with an idle ride and zero session statistics. Only the
explicit Start command creates a session. Auto-pause/resume applies only after
Start; manual Pause prevents automatic resume. Free ride/map navigation and
live sensor readings remain usable without recording. Reboot does not recover
an interrupted recording automatically; existing SD files are not deleted.

## Verification

Host tests cover queued work, disconnect before processing, busy-SD cleanup,
oversized requests, duplicate uploads, CRC/offset validation, deleted BLE
characteristics, BUSY polling, timeouts and manual-start lifecycle. They do not
simulate the real NimBLE scheduler, radio or SD latency.

Hardware acceptance (finish any active ride before flashing):

1. Install both updated firmware and Flutter app.
2. Boot with a GPS fix/movement: Ride must offer Start, with no new recording.
3. Import/sync the previously failing GPX. Verify `SAVED`, route selection and
   map coverage; record RouteIO timings/stack and check for panic/reset.
4. Repeat the same route, then disconnect mid-transfer and reconnect/retry.
5. Explicitly Start, verify recording, Pause/Resume and Finish/Save.

Do not use repeated USB serial open/close as a reset-free diagnostic: this
board showed `USB_UART_CHIP_RESET` when the monitor was opened during diagnosis.
