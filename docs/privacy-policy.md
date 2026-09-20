# Privacy policy — OpenCyclo

**Effective date:** 20 September 2026

OpenCyclo is the companion app for the OpenCyclo DIY GPS cycling computer.
This policy explains what the app does with your information. The short version:
**your rides, routes and location stay on your phone and your device.** The app
has no accounts, no ads, no analytics and no tracking, and it never sends your
ride data anywhere.

## Who this policy is from

The OpenCyclo app is published by Liostech. Questions about this policy can be
sent to **privacy@liostech.id**.

## What the app does with your information

### Location

The app can use your phone's location as an alternative GPS source for the
connected bike computer, when the device's own receiver has a slow or weak fix.
You turn this on yourself, per ride.

- Location is read on the phone and sent **only** to your paired OpenCyclo
  device, over Bluetooth.
- It is never sent to us, to u-blox, or to anyone else.
- The app does not keep a location history of its own.
- If you enable the background location permission, the app keeps supplying
  your position while the screen is locked or the app is in the background, so
  the device keeps tracking during a ride. Android shows a permanent
  notification while this is active, and you can turn it off at any time in
  Android settings.

### Bluetooth

The app scans for and connects to your OpenCyclo device and to standard
Bluetooth speed/cadence, heart-rate and cycling power sensors. Bluetooth is used
only for these local connections. Incoming ride files, routes and firmware
images travel over this same local link.

### Files

- When you import a GPX route, the app reads the file you pick in the system
  file picker.
- When you export a ride or save a screenshot, the app writes the file on your
  phone. Where it goes is your choice or your system's default.
- The app does not scan or index your storage.

### AssistNow credentials

To use u-blox assisted GPS, you enter a Chipcode or a Thingstream token in the
app. These are stored using your platform's secure storage (iOS Keychain, or
Android's Keystore-backed encrypted storage), not in plain text. They are used
only to make the assisted-GPS requests described below, and you can clear them
by clearing the app's data or uninstalling it.

## The only network requests the app makes

The app talks to the internet in exactly one situation: when **you** tap to sync
assisted GPS. Both requests go to u-blox, over HTTPS.

| When | Where | What is sent |
| --- | --- | --- |
| Live assistance, or predictive cache download | `assistnow.services.u-blox.com` | Your AssistNow Chipcode and the GNSS constellations requested |
| Registering a new receiver (Thingstream zero-touch provisioning) | `api.thingstream.io` | Your Thingstream token and the GPS module's identity messages, which include that module's unique ID |

These requests exist to download satellite orbit data so your bike computer gets
a GPS fix faster. u-blox processes that request as an independent service;
their handling of it is covered by u-blox's own privacy policy
(https://www.u-blox.com/en/privacy-statement).

Nothing else in the app reaches the network. There is no telemetry, no crash
reporting, no advertising SDK and no third-party analytics.

## What we do not do

- We do not have accounts or logins, so we hold no profile about you.
- We do not collect, receive or store your location, rides, routes or device
  data. We operate no servers that receive them.
- We do not sell or share personal information, and we do not use it for
  advertising.
- We do not track you across apps or websites.

## Retention and deletion

Because nothing is transmitted to us, there is nothing on our side to retain or
delete. On your phone, the only stored item is the AssistNow credential in
secure storage: clear it by clearing the app's data or uninstalling the app.
Ride files and routes live on your phone and on the device's SD card, and are
yours to delete.

## Security

Network requests to u-blox are made over TLS, and the app refuses to follow
redirects on them so a credential is never forwarded to another host. Credentials
are kept in platform secure storage. The device link is a local Bluetooth
connection.

## Children

The app is not directed at children under 13, and we do not knowingly collect
any information from them.

## Changes to this policy

If this policy changes, the new version will be published at this address with a
new effective date. Material changes will also be noted in the app's release
notes.

## Contact

Liostech — privacy@liostech.id
