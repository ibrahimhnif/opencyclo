# OpenCyclo design review

Presentation-only design system and interactive flow review. Open index.html
directly, or run npm run dev. No dependencies or installation required.
All values and asynchronous outcomes are fixtures, not live telemetry.

## Layout comparison

Open compare.html for three synchronized 240 × 320 alternatives: verbose text,
icons with short labels, and icon-only. Five scenarios cover ride controls,
finish/save, save failure, free map and all five connected camera commands.
The verbose baseline is illustrative, not an exact firmware screenshot.
Use the scenario selector or click mock actions; save success is an injected
outcome, never evidence of persistence. Existing flow review remains unchanged.
Recommendation: short labels for consequential actions, icon-only for familiar
map controls. No hardware functionality is changed. Comparison tests verify
action parity and state targets, not pixel layout or physical touch usability.

## Scope

Eleven flows: ride lifecycle, free map, GPX navigation, GPX import/sync, phone
connection, sensors, Insta360, power/charging, layout customization, OTA and
ride dashboard. Use state chips to inspect any state; device buttons follow
the proposed journey. Scenario buttons inject simulated external outcomes.

The Design system tab defines semantic colors, type sizes, touch targets,
spacing, component contracts, copy and contrast calculations. The UX audit
tab records source-based findings, priorities, proposals and acceptance checks.

This is a heuristic review, not usability testing. It does not certify
touch/LCD behavior, sunlight readability, radio reliability or SD persistence.
HTML uses Arimo from the existing app; firmware uses FreeSans/LovyanGFX.
Device views are 240 × 320 CSS pixels, not physical-size calibrated previews.
Companion views are compact responsive mockups, not Flutter screenshots.

## Functional boundaries

- No BLE, GPS, filesystem pickers, GPX writes, camera commands or firmware flash.
- No edits to production hardware, core, navigation, storage or Flutter code.
- Insta360 commands remain pair + shutter, mode, screen toggle, wake and power
  off. Proposed grouping changes only presentation; command feedback never
  asserts an unverified camera recording state.
- Route bends are geometry estimates, not a new routing engine.
- Default hardware cannot report charger state; charge mode is conditional.
- New preflight checks, larger targets, unavailable-value display and grouped
  camera options are proposals, not claims of implemented functionality.
- Last session's UI refactor remains unchanged. This review adds only this folder.

## Source and fidelity

Each flow lists its source-of-truth files. Basemap comes from the existing
Indonesia OSM pack preview around Jakarta, not a user's recorded ride. GPX
overlay and numeric values are illustrative. Map attribution is retained.
The power-and-charging document contains an outdated final sentence about
placeholder timestamps; current logger source and offline-navigation docs
take precedence.

## Validation

npm test checks every flow/state target, controller rendering with a minimal
DOM, critical simulated state paths, zoom bounds and absence of hardware/network
API calls. It does not run a real browser or verify responsive rendering.
npm run build produces static dist/ output.

## Review decision

Not ready to adopt wholesale in firmware. Preserve existing save/map behavior.
First resolve P1: recording vs navigation distinction, discoverable Ride access,
camera touch sizes, truthful async completion and OTA readiness messaging.
Then test outdoor readability and input on physical hardware before implementation.

Suggested acceptance tasks and open language choice are in the HTML audit.
No pass rate, measured task time or empirical usability score is claimed.
