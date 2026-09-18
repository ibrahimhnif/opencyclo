# AssistNow — registration, secure Chipcode and predictive cache

## Use

Build/flash the updated firmware and rebuild Flutter. Connect, then open
**device → assisted GPS**, enter this receiver's **Chipcode**, and tap **sync GPS**.
Requires phone internet and AssistNow Live Orbits entitlement. The Chipcode is
not the Thingstream ZTP token. It is saved securely per receiver after registration.
Existing manually entered Chipcodes can be saved with **save**. Opening the
screen reads the GPS silicon ID and loads its saved credential; **load** retries
this, and **forget** removes only this receiver's credential. No Chipcode goes
to ESP32, SD, logs or source code. Android uses encrypted secure storage backed
by Keystore (auto-backup disabled); iOS uses Keychain. macOS ad-hoc builds use
the encrypted login Keychain, avoiding an unrelated developer-team/signing
change. Keychain access may prompt after a rebuild. Writes are read back before
reporting successful persistence; there is no plaintext fallback.

For registration, expand **register GPS**, enter a fresh ZTP profile token, then
tap **register GPS**. The app reads full checksum-validated `UBX-MON-VER` and
`UBX-SEC-UNIQID` frames from the attached receiver and POSTs them with the token
to the fixed official HTTPS endpoint. This explicitly creates/registers a Thing
in the user's account. The returned Chipcode fills the sync field automatically;
tap **sync GPS** separately. The profile/account must still be created in the
portal first. The app neither purchases a subscription nor selects a paid plan.
Use an Evaluation profile for testing. Official registration workflow:
https://support.thingstream.io/hc/en-gb/articles/19691380675356-AssistNow-device-registration

Live Orbits downloads remain manual. **cache 7 days** downloads Predictive Orbits
to the device's SD card for automatic use; it requires Predictive Orbits access.
Neither option injects phone location or guarantees a particular TTFF. No subscription
has been purchased or device registered by this implementation. No successful
real-server/real-receiver registration or injection has yet been measured.

ZTP tokens are cleared from the input after each attempt and never persisted. Registration
is not automatically retried after HTTP failure (a request may have succeeded
server-side despite a lost reply). Cancellation/disconnection cannot undo a
registration already submitted. HTTP response bodies and secrets are never
logged/displayed, redirects are disabled, and server-supplied service URLs are
not followed. A registered Chipcode is bound in the screen to the selected BLE
device; credentials are actually keyed by a hash of the receiver silicon ID,
not BLE address or firmware version. Before sync, the receiver identity is
checked again to prevent accidental reuse after a GPS module swap.

## Seven-day device cache

Tap **cache 7 days** with the ride stopped, an operational SD card, internet and
a connected GPS. Flutter requests GPS/Galileo `uporb_7`, validates UBX-MGA-ANO
frames, checks checksums and actual calendar dates, then sends a bounded 128 KiB
maximum bundle over BLE. Only predictions are stored—no old UTC packet, secrets
or arbitrary UBX commands. The date displayed is the last UTC calendar day in
the response, not necessarily seven full days after the button press.

The logger owns all cache filesystem I/O while idle under the existing SD lock.
Files `/gps-cache/predictive-a.bin` and `predictive-b.bin` alternate, with a
generation header, size, dates and payload CRC. The inactive slot is overwritten;
the previous verified slot is kept. Read-back validation is required before
reporting **cache saved**. Boot chooses the newest valid slot, ignoring truncated
or corrupt replacements. No SD formatting, map deletion, or ride-file changes.

Cache filesystem work never executes in the GPS/ BLE UART path. Cache reads and
writes have a fixed size ceiling. GPS cache processing uses try-locks so a busy
SD transaction cannot block normal GPS parsing. Upload/update is only available
while the ride is idle. Existing data already loaded in RAM can be used while riding.

Automatic injection: when receiver protocol is supported, time is valid and
today's frames exist, the GPS task sends fresh UTC followed by **only today's**
predictions. Every frame requires matching receiver ACK. It runs at most once
successfully per UTC day per boot; a receiver rejection/timeout stops the attempt
for that day until a new cache is uploaded or the device is restarted. Live sync,
identity reads and cache injection do not overlap UART transactions.

Time sources: fresh HTTPS server Date plus monotonic elapsed on upload; or a
fully resolved, valid-date/time NAV-PVT message with tAcc below one second.
No unchecked phone wall clock, last saved date or stale server-time packet is
used. ESP system RTC may retain time through deep sleep; validity metadata is
cleared on other resets/full power loss. Retained-time uncertainty grows with a
conservative 5% drift allowance plus 60 seconds. Above one hour uncertainty, or
when that uncertainty crosses midnight, automatic injection waits for fresh time.
This drift allowance is a software policy, **not measured oscillator accuracy**.
Clock retention and actual receiver performance still need hardware testing.

Status appears in Flutter (polled while this screen is open) and the firmware
sensor-debug screen: **perlu sync** (empty/expired/no frames for today),
**menunggu waktu**, **cache siap**, **memakai cache**, **cache dipakai**,
**SD unavailable**, or **cache error**. Saved does not mean injected; injected
does not mean a valid position fix. Full power loss can therefore require a
new time source even when the prediction cache itself has not expired.

Cache characteristic `00001909-0000-1000-8000-00805f9b34fb`, integers LE:
BEGIN `01 nonce:u32 size:u32 crc:u32`, DATA `02 nonce:u32 offset:u32 bytes`,
COMMIT `03 nonce:u32`, fresh TIME `04 nonce:u32 utcSeconds:u32`,
ABORT `05 nonce:u32`. Status is 20 bytes: `version=1 transfer cacheState error
nonce:u32 received:u32 firstUtcDay:u32 lastUtcDay:u32`.
Transfer: 0 idle,1 receiving,2 saving,3 saved,4 error. Cache states follow the
status order in the source (`gps_cache.cpp`). Partial uploads time out; previous
cache is retained. Aborting after commit cannot undo a completed save.

Tests: `python3 tests/run_gps_cache_tests.py` exercises the production cache with
fake SD/clock (CRC, invalid dates, missing time, failed-write fallback, persistent
reload, expiry, time-first ACK-driven automatic injection). Flutter cache tests
cover secure storage isolation/forget, expired/corrupt data, MTU23 upload and SD
failure. Real Thingstream access, Keychain/Keystore persistence and cold/deep-sleep
clock behavior require device testing; no live account request is made by tests.

Read-only identity endpoint: `00001908-0000-1000-8000-00805f9b34fb`.
Commands: BEGIN `01 nonce:u32`, SELECT `02 nonce:u32 offset:u16`, CANCEL
`03 nonce:u32`. READ returns up to 20 bytes: `version=1 state error nonce:u32
offset:u16 total:u16 data[0..9]`. States 0 idle, 1/2 version poll/wait, 3/4 unique
ID poll/wait, 5 done, 6 error. Completed data is `versionFrameLength:u16`, full
MON-VER frame, then full SEC-UNIQID frame (maximum 540 bytes). Identity and
assistance transactions exclude each other. Polling runs in the GPS task and
times out after 2 seconds per reply; BLE callbacks never read/write UART.
Normal NMEA/PVT decoding continues. Flutter verifies framing/checksums and the
currently supported receiver protocol before any registration request.

## Data path

Flutter requests GPS/Galileo `ulorb_l1,usvht,ukion` over HTTPS from the fixed
AssistNow endpoint; redirects are disabled. Bounded download (64 KiB, timeout),
UBX frame sizes/checksums/types, time-first ordering, and presence of ephemeris
are verified before sending. Error bodies and credential-bearing URLs are not
shown. Service credential/access errors are shown as HTTP status only.

The server's initial UTC is retained, with uncertainty enlarged to cover elapsed
request/setup time and 10 seconds of transfer slack. Data older than 30 seconds
before time-frame preparation is rejected. A time frame taking over 5 seconds
to assemble over BLE is aborted before commit. This avoids using an unchecked
phone clock or replaying cached UTC. A fresh request is required after failure.

BLE characteristic `00001907-0000-1000-8000-00805f9b34fb` is READ/WRITE.
Writes use response, MTU-sized fragments and bounded identical retries.
Read polling obtains application/GNSS acknowledgment, not just BLE delivery.
All integers below are little endian:

- BEGIN: `01 token:u32` (nonzero random per attempt).
- DATA: `02 token:u32 index:u16 offset:u16 bytes...`.
- COMMIT frame: `03 token:u32 index:u16`.
- END: `04 token:u32 frameCount:u16`.
- ABORT: `05 token:u32`.
- Status (12 bytes): `version=1 state error receiverInfo token:u32 nextIndex:u16 buffered:u16`.
- States: 0 idle, 1 configure, 2 config ACK pending, 3 ready, 4 send pending,
  5 MGA ACK pending, 6 done, 7 error.
- Errors: 1 unsupported receiver, 2 invalid packet, 3 timeout, 4 receiver NACK,
  5 UART write failure, 6 cancelled.

Firmware buffers only one frame (520 bytes maximum, 256 frames/session).
BLE callbacks do no UART work. The GPS task sends a RAM-only CFG-VALSET enabling
MGA acknowledgments and UART1 UBX output, then sends one MGA frame at a time.
It matches UBX-MGA-ACK by message ID and first four payload bytes, checks acceptance,
and advances the sequence only on success. Unrelated NMEA/PVT still reaches the
existing parser. Receiver/config ACK timeout is 2 seconds; abandoned sessions
expire after 15 seconds. Power-off cancels active assistance.

Only M10 protocol 34.10, already recognized by the existing startup handshake,
is enabled. Allowed inputs are UTC initialization and GPS/Galileo Live Orbits
messages—not arbitrary receiver reset/configuration/flash commands. This does
not change measurement rates, constellations, quality filters or ride state.
Baseline bypass has been removed. Normal startup uses the acknowledged RAM
5 Hz/portable/NAV-PVT configuration and shutdown verifies PMREQ standby.
Explicit assistance additionally enables the two RAM settings above.
GPS diagnostic logging remains enabled.

The session token detects stale/retried packets; it is **not authentication**.
This endpoint uses the project's existing BLE link security, with no new
pairing/authentication policy. Keep the device under your control while syncing.

Success means every frame was accepted by the receiver, **not GPS fix**.
Closing the screen cancels transfer; assistance already accepted cannot be
retracted. Failure leaves ordinary standalone acquisition running. The screen
does not claim a cached assistance validity period or persist success across boots.

## Verification

- `rtk proxy python3 tests/run_gps_tests.py`: real GPS decoder/filter regression
  plus assistance state-machine tests (checksum, allowlist, ordering, retries,
  unrelated ACK, NACK, timeout, token isolation, cancellation).
- `rtk proxy flutter test`: parser, time uncertainty, MTU23/retry transfer, receiver
  rejection plus existing app tests.
- `rtk proxy flutter analyze --no-pub` and `rtk proxy pio run`.
- Hardware acceptance: first verify firmware/receiver version; sync with a valid
  receiver Chipcode outdoors. Capture `[GPS AID]` start/done/error timestamps and
  existing GPS diagnostic CSV. Compare raw TTFF and filtered TTFF separately,
  keeping location, antenna, supply and initial receiver state comparable.
- Also test BLE disconnection, cancellation, GPS unplugged, wrong Chipcode,
  denied subscription, and phone without internet. None should report success
  or change recording state. Do not flash during a recorded ride.

Protocol references:
https://support.thingstream.io/hc/en-gb/articles/19691732434972-AssistNow-service-integration-guide
https://support.thingstream.io/hc/en-gb/articles/19691577215004-Developing-Host-software
https://content.u-blox.com/sites/default/files/u-blox-M10-SPG-5.10_InterfaceDescription_UBX-21035062.pdf
