# Browser companion (Web Serial)

Static web tool in [`web/`](../web/index.html). No build step, no backend.
Serve over `https` or `localhost` (Web Serial requirement), open in Chrome
or Edge on desktop.

```bash
cd web
python3 -m http.server 8000
# open http://localhost:8000/
```

## Theme editor (`index.html`)

- Connect USB → handshake (`OC+HELLO`) shows firmware and schema versions.
- Theme tab edits every color token plus font scale, with a live 240×320
  preview driven by the same formatting rules as
  `src/ui/engine/widget_model.cpp`.
- Pages tab edits titles, templates, and per-slot widgets.
- Apply theme / Apply layout push over USB (`OC+SET_THEME`, `OC+SET_LAYOUT`).
  Imports are staged and validated on-device; a rejected document changes
  nothing. Themes also export/import as JSON files for sharing presets.

## Firmware flashing (`flash.html`)

- Uses [esptool-js 0.7.0](https://github.com/espressif/esptool-js) from CDN
  (`ESPLoader`, `Transport`, `writeFlash`, `eraseFlash`).
- Parameters match `platformio.ini`: qio, 80MHz, 16MB.
- After `pio run`, pick the three binaries from
  `.pio/build/esp32-s3-devkitc-1/`: `bootloader.bin` @ `0x0`,
  `partitions.bin` @ `0x8000`, `firmware.bin` @ `0x10000`.

## Serial protocol

115200 8-N-1 over USB CDC, one line per frame:

- Host: `OC+COMMAND` or `OC+COMMAND:<json payload>`
- Device: `OC=<json>` (exactly one reply line per command)

| Command      | Reply                                           |
| ------------ | ----------------------------------------------- |
| `HELLO`      | `{"ok":true,"proto":1,"fw":"0.1.0",...}`        |
| `GET_LAYOUT` | `{"ok":true,"layout":{...}}` (UiConfig schema)  |
| `SET_LAYOUT` | `{"ok":true}` or `{"ok":false,"err":"..."}`     |
| `GET_THEME`  | `{"ok":true,"theme":{...}}` (theme schema)      |
| `SET_THEME`  | `{"ok":true}` or `{"ok":false,"err":"..."}`     |
| `RESET_ALL`  | `{"ok":true}` (factory layout + theme)          |

Theme JSON: `{"schema":1,"bg":"#000000",...,"font_scale":100}` —
CSS hex colors shared by the firmware (`ui/engine/theme.h`) and the editor.

## Firefox / Safari

Web Serial is Chromium-only, so those browsers stay on the Flutter app,
which keeps working: layout BLE sync is unchanged and the v1 `UiConfig`
schema is untouched. The theme lives in its own NVS key, so old layouts
load exactly as before.
