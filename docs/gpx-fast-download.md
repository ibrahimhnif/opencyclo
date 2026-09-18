# GPX binary download v1

## Current throughput revision (prepared, not yet hardware-validated)

Capability command 0x15 returns CAPS 64. A new app uses a 12-byte window request,
adding a one-byte packet credit count (1–64). Legacy 11-byte requests retain 16
credits. The app falls back to 16 when old firmware rejects capability discovery.
Source payload remains capped at 180 bytes, so the new window holds at most
11520 bytes. Inter-packet pacing is 2 ms instead of 10 ms; queue-result checks
and bounded transient-error retries remain active.

Subscription requests a 15–30 ms connection interval with zero slave latency;
the central OS decides whether to accept. Unsubscription requests restoration of
the previous interval/latency/timeout. Do not interpret requested parameters as
measured throughput. This revision requires the updated app for 64-packet windows.
It was intentionally not flashed while the user's previous download was running.

The following sections document earlier protocol stages; current limits above
supersede the original 16-packet/480-byte examples.

Both updated Flutter app and firmware are needed. App discovers characteristic
1906 and chooses binary export; if absent, it uses the legacy 160-byte hex path.
The existing GPX Save dialog remains unchanged. No automatic flash is performed.

The RouteIO worker, never a BLE callback, opens a saved GPX read-only, reads
sequentially across windows, and seeks only on a retry/nonsequential offset.
It retains route-transfer ownership until close, disconnect, or 30-second
inactivity cleanup. Active/paused rides and unfinished saves cannot be exported.
SD maps and saved ride contents are not changed.

## Wire protocol

All integers are little-endian. Control replies remain text on 1904 with the
existing BUSY mailbox protocol. Data notifications use 1906.

| Command | Request after opcode | Reply |
|---|---|---|
| 0x12 Open | expected size u32, safe GPX basename | FAST session size |
| 0x13 Window | session u32, offset u32, payload size u16 | BLOCK start end CRC32-hex |
| 0x14 Close | session u32 | OK closed |

Each notification contains session u32 + absolute offset u32 + binary bytes.
The app selects payload=min(MTU-11,480), requests up to 16 packets per window,
subscribes before requesting data, and validates session, offsets, packet
lengths, total file size and per-window CRC32 before advancing progress.
The firmware caps a window at 7680 bytes in a fixed buffer.

Notifications may arrive before the control reply, out of order, duplicated,
or slightly after the reply. The app allows up to 500 ms after the reply for
missing packets and retries a failed window up to three attempts. Cancel
closes the session; disconnect/expiry provide firmware cleanup if close fails.
A stalled control transaction fails rather than blindly duplicating commands.
Resume across app restarts is not implemented.

## Expected improvement, not a measured benchmark

Legacy export sends 160 source bytes as 320 hex characters for every command.
With MTU 512, binary export requests up to 7680 source bytes per command.
A 1 MiB file therefore needs 6554 legacy chunk requests versus 137 binary
window requests, plus open/close. This is a reduction in round trips, not a
48x throughput guarantee: radio conditions, connection interval, notification
queues, other BLE links and SD performance still matter.

MTU 23 remains supported (12 data bytes per notification). Its improvement can
be limited; do not assume large-MTU speeds on that connection.

Verification: Flutter tests cover exact bytes, MTUs 23/185/512, packet order,
duplicates, CRC retry, exhausted missing-packet retries and cancellation.
Native integration tests exercise actual firmware open/read/close handlers,
session/offset rejection and idle timeout alongside legacy export.
Hardware throughput still needs a same-file timed comparison on the phone.
 
## macOS transfer failure hardening

Hardware follow-up: 488-byte notifications repeatedly returned BLE_HS_ENOMEM (6).
The firmware now caps source payload at 180 bytes (188 including export header),
paces notifications at 10 ms, and configures 32 MSYS1 blocks instead of 12.
The updated app already handles the lower server limit without an app code change.
The effective window is now at most 2880 bytes; earlier 7680-byte figures above
describe the original protocol ceiling, not the current conservative policy.

NimBLE-Arduino 1.4.3's high-level notify() returns void and discards the host
notification enqueue result. The firmware now targets the subscribed connection
with ble_gatts_notify_custom(), checks its return code, and retries transient
buffer/queue pressure for up to 300 ms per packet. It also checks the receiver's
actual negotiated MTU before sending a window. ERR export MTU reports the safe
payload; the app reduces its payload and recomputes the window before retrying.
ERR export backpressure retries the window with a short delay. Flutter logs
received/rejected packet counts and lengths to distinguish missing data from
size mismatch. These changes require fresh app and firmware builds; tests do
not establish success on the affected physical macOS link.
