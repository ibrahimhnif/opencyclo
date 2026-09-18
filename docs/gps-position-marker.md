# GPS position direction marker

Free ride and GPX navigation share a north-up, fixed-pixel-size position arrow.
The arrow derives travel direction from accepted GPS positions, independently
of the GPX route and viewport pan/zoom. It is not a compass heading.

- Require speed >= 3 km/h, at least 6 m displacement and >= 500 ms between
  anchors. Reject implausible jumps; restart the baseline after a 10 s gap.
- Smooth angular changes along the shortest circular arc, including north wrap.
- Moving: cyan triangle with white/black outline. Stopped/stale direction: hold
  the last arrow in grey. Before the first direction is known: white/blue dot.
- GPS loss: no live marker; clear the direction baseline before reacquisition.
- No changes to GPS filtering, recording, routing or camera control.

Native tests cover direction, jitter, stop, loss, jumps and timestamp wrap.
Navigation integration tests cover triangle rendering and hiding after GPS loss.
Hardware validation still needs a short outdoor ride after flashing.
