window.REVIEW.flows.push(...[
  {
    "id": "free-map",
    "name": "Free ride & map",
    "surface": "Device · 240 × 320",
    "purpose": "Melihat map tanpa menyamakan peta, GPS fix, dan recording.",
    "source": "src/navigation/navigation.cpp · src/navigation/map_renderer.cpp",
    "current": "Preview Monas sebelum fix; tanpa marker lokasi palsu. Drag menonaktifkan follow, zoom 13–17. Peta dibaca dari SD; cache diproses di background.",
    "review": "Tiga status berbeda harus terbaca: map loaded, GPS waiting, recording on/off. Tombol map lama kecil dan area tap header belum jelas.",
    "contract": "Panning tidak mengubah ride state. GPS preview tidak masuk GPX. Missing map bukan waiting GPS. Basemap tidak menyediakan destination routing.",
    "states": [
      {
        "id": "preview",
        "name": "GPS waiting",
        "title": "Free ride",
        "status": "PREVIEW · NO GPS",
        "layout": "map",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Ride controls",
            "ride:ready"
          ],
          [
            "Follow GPS",
            "preview"
          ]
        ],
        "note": "Real SD map sekitar Monas, bukan lokasi user. Tidak ada marker sebelum fix.",
        "events": [
          [
            "GPS fix acquired",
            "follow"
          ],
          [
            "SD unavailable",
            "sd-missing"
          ],
          [
            "Tile not installed",
            "missing"
          ],
          [
            "Reading new area",
            "loading"
          ]
        ]
      },
      {
        "id": "follow",
        "name": "Follow",
        "title": "Free ride",
        "status": "GPS FIX · FOLLOW",
        "layout": "map",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Pan map",
            "pan"
          ],
          [
            "Ride controls",
            "ride:recording"
          ]
        ],
        "note": "Marker hanya saat GPS valid. Contoh recording terpisah dari mode free ride.",
        "marker": true,
        "events": [
          [
            "Lose GPS",
            "lost"
          ]
        ]
      },
      {
        "id": "pan",
        "name": "Pan",
        "title": "Browse map",
        "status": "GPS FIX · PAN",
        "layout": "map",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Recenter",
            "follow"
          ],
          [
            "Ride controls",
            "ride:recording"
          ]
        ],
        "note": "Recenter harus jelas, bukan label Follow yang ambigu. Simulasi ini mengganti state, tidak mengukur touch latency.",
        "marker": true,
        "pan": true
      },
      {
        "id": "loading",
        "name": "Tile loading",
        "title": "Free ride",
        "status": "LOADING MAP",
        "layout": "map",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Ride controls",
            "ride:recording"
          ],
          [
            "Back",
            "preview"
          ]
        ],
        "note": "Konten cached tetap terlihat saat area baru dibaca. Loading bukan missing data.",
        "events": [
          [
            "Tiles ready",
            "follow"
          ],
          [
            "Tiles absent",
            "missing"
          ]
        ]
      },
      {
        "id": "lost",
        "name": "GPS lost",
        "title": "Free ride",
        "status": "GPS LOST · LAST VIEW",
        "layout": "map",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Ride controls",
            "ride:recording"
          ],
          [
            "Back",
            "follow"
          ]
        ],
        "note": "Tampilan posisi terakhir bukan live tracking. Hilangkan marker live; jangan menyambung garis palsu.",
        "events": [
          [
            "GPS recovered",
            "follow"
          ]
        ]
      },
      {
        "id": "missing",
        "name": "Missing map",
        "title": "Map unavailable",
        "status": "GPS AND MAP ARE SEPARATE",
        "layout": "message",
        "copy": "Area not on SD",
        "detail": "Copy the map pack to /maps on the SD card.",
        "buttons": [
          [
            "Ride controls",
            "ride:recording"
          ],
          [
            "Back",
            "preview"
          ]
        ],
        "note": "Pesan harus menyebut tindakan pemulihan; jangan menyuruh menunggu GPS.",
        "tone": "warning"
      },
      {
        "id": "sd-missing",
        "name": "No SD",
        "title": "SD unavailable",
        "status": "NO OFFLINE MAP",
        "layout": "message",
        "copy": "Check the SD card",
        "detail": "Turn off safely before reinserting the card.",
        "buttons": [
          [
            "Ride controls",
            "ride:ready"
          ],
          [
            "Back",
            "preview"
          ]
        ],
        "note": "Jangan menjanjikan GPX saving kalau SD tidak tersedia.",
        "tone": "danger"
      }
    ]
  },
  {
    "id": "gpx",
    "name": "GPX navigation",
    "surface": "Device · 240 × 320",
    "purpose": "Memilih track, mengikuti arah route, dan menangani off-route dengan jujur.",
    "source": "src/navigation/navigation.cpp · src/navigation/geo.h",
    "current": "Route library di SD, cue list, cyan GPX overlay, estimasi route bends, warning >50 m dan arrival dekat akhir route. Tidak ada rerouting otomatis.",
    "review": "Gunakan istilah route bend saat cue berasal dari geometri. 'Arrived' tidak boleh dianggap Finish Ride.",
    "contract": "Navigasi tidak memulai atau menyelesaikan recording. User membuat GPX sendiri dan sync lewat app. Tidak menambahkan turn restrictions / routing engine.",
    "states": [
      {
        "id": "library",
        "name": "Routes",
        "title": "Choose route",
        "status": "SAVED ON DEVICE",
        "layout": "list",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Monas loop",
            "following"
          ],
          [
            "Free ride",
            "free-map:preview"
          ]
        ],
        "note": "Daftar route harus menunjukkan nama dan context pemilihan.",
        "rows": [
          [
            "Monas loop",
            "Example GPX · 12.8 km"
          ],
          [
            "Morning ride",
            "Example GPX · 24.2 km"
          ]
        ],
        "events": [
          [
            "No saved routes",
            "empty"
          ]
        ]
      },
      {
        "id": "empty",
        "name": "No route",
        "title": "No routes yet",
        "status": "GPX REQUIRED",
        "layout": "message",
        "copy": "Import on your phone",
        "detail": "Routes → Import GPX → Sync to device.",
        "buttons": [
          [
            "Free ride",
            "free-map:preview"
          ],
          [
            "Back",
            "library"
          ]
        ],
        "note": "Jangan blank screen. Beri instruksi recovery yang cocok dengan app.",
        "tone": "warning"
      },
      {
        "id": "following",
        "name": "Following",
        "title": "Monas loop",
        "status": "180 m · ROUTE BENDS RIGHT",
        "layout": "map",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Cue list",
            "cues"
          ],
          [
            "Ride controls",
            "ride:recording"
          ]
        ],
        "note": "Cue estimasi bukan instruksi intersection yang diverifikasi. Route overlay contoh hanya untuk review.",
        "marker": true,
        "route": true,
        "events": [
          [
            "Off route >50 m",
            "off-route"
          ],
          [
            "Near route end",
            "arrived"
          ],
          [
            "GPS lost",
            "lost"
          ]
        ]
      },
      {
        "id": "cues",
        "name": "Cue list",
        "title": "Route cues",
        "status": "FROM ROUTE START",
        "layout": "list",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Back to map",
            "following"
          ],
          [
            "Choose route",
            "library"
          ]
        ],
        "note": "Daftar jarak dari start, bukan semua dihitung dari lokasi saat ini.",
        "rows": [
          [
            "0.8 km · Right bend",
            "Estimated from GPX"
          ],
          [
            "1.4 km · Left bend",
            "Estimated from GPX"
          ]
        ]
      },
      {
        "id": "off-route",
        "name": "Off route",
        "title": "Monas loop",
        "status": "OFF ROUTE · RETURN TO TRACK",
        "layout": "map",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Cue list",
            "cues"
          ],
          [
            "Ride controls",
            "ride:recording"
          ]
        ],
        "note": "Cyan track tetap ditampilkan; tidak membuat route baru tanpa engine routing.",
        "route": true,
        "marker": true,
        "tone": "warning",
        "events": [
          [
            "Back on track",
            "following"
          ]
        ]
      },
      {
        "id": "lost",
        "name": "GPS lost",
        "title": "Monas loop",
        "status": "GPS LOST · GUIDANCE PAUSED",
        "layout": "map",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Cue list",
            "cues"
          ],
          [
            "Ride controls",
            "ride:recording"
          ]
        ],
        "note": "Bekukan progress. Hindari menghitung cue dari posisi imajiner.",
        "route": true,
        "tone": "warning",
        "events": [
          [
            "GPS recovered",
            "following"
          ]
        ]
      },
      {
        "id": "arrived",
        "name": "Arrived",
        "title": "Route complete",
        "status": "ARRIVED ≠ RIDE SAVED",
        "layout": "message",
        "copy": "You reached the end",
        "detail": "Recording continues until you choose Finish ride.",
        "buttons": [
          [
            "Ride controls",
            "ride:recording"
          ],
          [
            "Keep map open",
            "following"
          ]
        ],
        "note": "Pisahkan completion navigasi dari penyimpanan GPX ride.",
        "tone": "success"
      }
    ]
  },
  {
    "id": "routes-app",
    "name": "Import & sync GPX",
    "surface": "Flutter companion · responsive",
    "phone": true,
    "purpose": "Dari file GPX user sampai route benar-benar tersimpan di device.",
    "source": "app/lib/ui/screens/tabs/routes_tab.dart · app/lib/core/ble/route_transfer.dart",
    "current": "Import ≤8 MB, satu continuous track/route, validasi points/cues; transfer offset + CRC dan SAVED ack; coverage check terpisah.",
    "review": "Status import, sending, saved, coverage, navigation harus berurutan. 100% bytes bukan otomatis sukses. Pisahkan route geometry preview dari map pack.",
    "contract": "Mock file dan persentase hanya fixture. Tidak memilih file lokal atau mengirim BLE. Map Indonesia tetap dicopy terpisah ke SD.",
    "states": [
      {
        "id": "empty",
        "name": "Empty",
        "title": "Routes",
        "status": "NO ROUTE SELECTED",
        "layout": "message",
        "copy": "Your route, offline",
        "detail": "Import a GPX you created, then sync it to OpenCyclo.",
        "buttons": [
          [
            "Import sample GPX",
            "preview"
          ],
          [
            "Open free ride",
            "free-map:preview"
          ]
        ],
        "note": "Label Import, bukan Generate: app belum membuat route.",
        "events": [
          [
            "Invalid GPX",
            "invalid"
          ],
          [
            "No device connection",
            "disconnected"
          ]
        ]
      },
      {
        "id": "preview",
        "name": "Preview",
        "title": "Monas loop",
        "status": "GPX VALIDATED",
        "layout": "message",
        "copy": "12.8 km · 682 points",
        "detail": "Route geometry only. Offline map is installed separately on the SD card.",
        "buttons": [
          [
            "Sync to device",
            "sending"
          ],
          [
            "Choose another GPX",
            "empty"
          ]
        ],
        "note": "Tampilkan nama, jarak dan cue count sebelum sync."
      },
      {
        "id": "sending",
        "name": "Sending",
        "title": "Syncing route",
        "status": "TRANSFER · 64%",
        "layout": "message",
        "copy": "Sending GPX…",
        "detail": "Keep the device awake and in Bluetooth range.",
        "buttons": [
          [
            "Cancel transfer",
            "cancelled"
          ]
        ],
        "note": "Persentase review adalah fixture, bukan telemetry live.",
        "progress": 64,
        "events": [
          [
            "Bytes sent",
            "verifying"
          ],
          [
            "Connection dropped",
            "disconnected"
          ]
        ]
      },
      {
        "id": "verifying",
        "name": "Verifying",
        "title": "Verify route",
        "status": "100% SENT · NOT SAVED YET",
        "layout": "message",
        "copy": "Waiting for device",
        "detail": "The device checks CRC and closes the route file.",
        "buttons": [],
        "note": "Transisi sukses harus menunggu SAVED ack.",
        "progress": 100,
        "events": [
          [
            "SAVED + coverage OK",
            "saved"
          ],
          [
            "Checksum rejected",
            "invalid"
          ]
        ]
      },
      {
        "id": "saved",
        "name": "Saved",
        "title": "Route on device",
        "status": "SAVED · MAP COVERED",
        "layout": "message",
        "copy": "Ready to navigate",
        "detail": "This selects navigation only. Ride recording is separate.",
        "buttons": [
          [
            "Start navigation",
            "gpx:following"
          ],
          [
            "Import another",
            "empty"
          ]
        ],
        "note": "Coverage bukan jaminan semua jalan bisa dilalui.",
        "tone": "success",
        "events": [
          [
            "Coverage missing",
            "coverage"
          ]
        ]
      },
      {
        "id": "coverage",
        "name": "Coverage issue",
        "title": "Route on device",
        "status": "SAVED · MAP PARTLY MISSING",
        "layout": "message",
        "copy": "Install missing maps",
        "detail": "GPX can still be followed without a complete basemap.",
        "buttons": [
          [
            "Navigate GPX anyway",
            "gpx:following"
          ],
          [
            "Back",
            "saved"
          ]
        ],
        "note": "Jangan membatalkan status route saved hanya karena map check bermasalah.",
        "tone": "warning"
      },
      {
        "id": "invalid",
        "name": "Invalid GPX",
        "title": "Can't use this GPX",
        "status": "VALIDATION FAILED",
        "layout": "message",
        "copy": "Check the route file",
        "detail": "Use one continuous track/route, ≤8 MB. No multi-segment joins.",
        "buttons": [
          [
            "Choose another GPX",
            "empty"
          ],
          [
            "Back",
            "empty"
          ]
        ],
        "note": "Production error detail berbeda-beda. Usulan: terjemahkan technical error menjadi cara memperbaiki.",
        "tone": "danger"
      },
      {
        "id": "disconnected",
        "name": "Disconnected",
        "title": "Device not linked",
        "status": "SYNC NOT COMPLETE",
        "layout": "message",
        "copy": "Reconnect to OpenCyclo",
        "detail": "Incomplete transfers are not saved routes.",
        "buttons": [
          [
            "Open connection",
            "connection:disconnected"
          ],
          [
            "Try again",
            "preview"
          ]
        ],
        "note": "Jangan mengklaim resume partial transfer; current flow re-sync safely.",
        "tone": "warning"
      },
      {
        "id": "cancelled",
        "name": "Cancelled",
        "title": "Transfer cancelled",
        "status": "NOT SAVED",
        "layout": "message",
        "copy": "Your phone GPX is kept",
        "detail": "You can sync it again when ready.",
        "buttons": [
          [
            "Retry sync",
            "sending"
          ],
          [
            "Back",
            "preview"
          ]
        ],
        "note": "Cancel tidak menghapus GPX asli.",
        "tone": "warning"
      }
    ]
  },
  {
    "id": "connection",
    "name": "Phone connection",
    "surface": "Flutter companion · responsive",
    "phone": true,
    "purpose": "Membuat status BLE device jelas sebelum aksi remote.",
    "source": "app/lib/ui/screens/tabs/device_tab.dart · app/lib/state/ble_provider.dart",
    "current": "App dapat connect/disconnect OpenCyclo dan menampilkan telemetry. Status sensor/kamera bukan sama dengan status phone link.",
    "review": "Istilah linked harus spesifik: Phone ↔ OpenCyclo. Jangan tampilkan semua peripheral sebagai connected hanya karena app tersambung.",
    "contract": "Simulasi tidak memindai perangkat atau meminta izin Bluetooth.",
    "states": [
      {
        "id": "disconnected",
        "name": "Disconnected",
        "title": "Device",
        "status": "NOT CONNECTED",
        "layout": "message",
        "copy": "Connect your OpenCyclo",
        "detail": "Keep Bluetooth on and the cycling computer nearby.",
        "buttons": [
          [
            "Find device",
            "scanning"
          ],
          [
            "Routes",
            "routes-app:empty"
          ]
        ],
        "note": "Aksi yang perlu device harus menunjukkan prerequisite."
      },
      {
        "id": "scanning",
        "name": "Scanning",
        "title": "Find OpenCyclo",
        "status": "SCANNING",
        "layout": "message",
        "copy": "Looking for your device",
        "detail": "Make sure the device is awake.",
        "buttons": [
          [
            "Cancel",
            "disconnected"
          ]
        ],
        "note": "Loading harus bisa dibatalkan.",
        "events": [
          [
            "Device found",
            "found"
          ],
          [
            "No devices",
            "empty"
          ]
        ]
      },
      {
        "id": "found",
        "name": "Found",
        "title": "Nearby devices",
        "status": "DEVICE FOUND",
        "layout": "list",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Connect OpenCyclo",
            "connected"
          ],
          [
            "Scan again",
            "scanning"
          ]
        ],
        "note": "Nama device konsisten dengan target OTA / route sync.",
        "rows": [
          [
            "OpenCyclo",
            "Nearby · sample device"
          ]
        ]
      },
      {
        "id": "connected",
        "name": "Connected",
        "title": "OpenCyclo",
        "status": "PHONE LINK CONNECTED",
        "layout": "message",
        "copy": "Device is ready",
        "detail": "Routes, layout sync and firmware update use this connection.",
        "buttons": [
          [
            "Open routes",
            "routes-app:empty"
          ],
          [
            "Disconnect",
            "disconnected"
          ]
        ],
        "note": "Phone link bukan bukti GPS fix / camera pairing.",
        "tone": "success",
        "events": [
          [
            "Link lost",
            "disconnected"
          ]
        ]
      },
      {
        "id": "empty",
        "name": "None found",
        "title": "No device found",
        "status": "SCAN FINISHED",
        "layout": "message",
        "copy": "Wake the cycling computer",
        "detail": "Check Bluetooth permission, then scan again.",
        "buttons": [
          [
            "Try again",
            "scanning"
          ],
          [
            "Back",
            "disconnected"
          ]
        ],
        "note": "Error permission dan deviceoff perlu detail berbeda di implementasi.",
        "tone": "warning"
      }
    ]
  },
  {
    "id": "sensors",
    "name": "Sensor pairing",
    "surface": "Device · 240 × 320",
    "purpose": "Pair sensor dan bedakan status koneksi dari nilai pengukuran.",
    "source": "src/ui/engine/widget_registry.cpp · src/hardware/ble_task.cpp",
    "current": "BLE manager mengelola HR/CSC/power. Firmware menyimpan pairing dan mencoba reconnect. Missing sensor saat ini bisa ditampilkan sebagai 0.",
    "review": "0 bukan selalu measured zero. Usulan UI: unavailable menjadi — dengan label state; butuh mapping display-only, bukan mengubah sensor protocol.",
    "contract": "Tidak mengubah scan, pairing, reconnect, characteristic atau cadence/power/HR calculation.",
    "states": [
      {
        "id": "list",
        "name": "Sensor list",
        "title": "Sensors",
        "status": "PAIRING & CONNECTION",
        "layout": "list",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Scan sensors",
            "scanning"
          ],
          [
            "Back to ride",
            "ride:recording"
          ]
        ],
        "note": "Tampilkan sensor identity per jenis, bukan status global.",
        "rows": [
          [
            "Heart rate · Not linked",
            "— bpm"
          ],
          [
            "Cadence · Connected",
            "82 rpm"
          ]
        ]
      },
      {
        "id": "scanning",
        "name": "Scanning",
        "title": "Find sensors",
        "status": "SCANNING",
        "layout": "message",
        "copy": "Wake your sensor",
        "detail": "Wear the HR strap or rotate the crank.",
        "buttons": [
          [
            "Cancel",
            "list"
          ]
        ],
        "note": "Petunjuk sesuai jenis sensor memudahkan recovery.",
        "events": [
          [
            "HR found",
            "found"
          ],
          [
            "No sensor",
            "empty"
          ]
        ]
      },
      {
        "id": "found",
        "name": "Found",
        "title": "Heart rate",
        "status": "SENSOR FOUND",
        "layout": "message",
        "copy": "HR strap",
        "detail": "Sample identifier · never real personal MAC",
        "buttons": [
          [
            "Pair sensor",
            "connected"
          ],
          [
            "Back",
            "list"
          ]
        ],
        "note": "Identifier review sengaja bukan MAC user."
      },
      {
        "id": "connected",
        "name": "Connected",
        "title": "Heart rate",
        "status": "CONNECTED",
        "layout": "message",
        "copy": "146 bpm",
        "detail": "Sample measured value. Connection is independent of phone link.",
        "buttons": [
          [
            "Back to sensors",
            "list"
          ],
          [
            "Back to ride",
            "ride:recording"
          ]
        ],
        "note": "Nilai normal tampil besar; connection status tetap jelas.",
        "tone": "success",
        "events": [
          [
            "Sensor disconnected",
            "lost"
          ]
        ]
      },
      {
        "id": "lost",
        "name": "Disconnected",
        "title": "Heart rate",
        "status": "RECONNECTING",
        "layout": "message",
        "copy": "— bpm",
        "detail": "Bring the sensor closer. Reconnection follows existing firmware behavior.",
        "buttons": [
          [
            "Sensors",
            "list"
          ],
          [
            "Back to ride",
            "ride:recording"
          ]
        ],
        "note": "Usulan display unavailable; tidak boleh menyatakan sensor mengukur 0.",
        "tone": "warning"
      },
      {
        "id": "empty",
        "name": "Not found",
        "title": "No sensor found",
        "status": "TRY AGAIN",
        "layout": "message",
        "copy": "Check the sensor",
        "detail": "Wake it and keep it nearby.",
        "buttons": [
          [
            "Scan again",
            "scanning"
          ],
          [
            "Back",
            "list"
          ]
        ],
        "note": "Jangan memaksa user menghapus pairing sebagai recovery pertama.",
        "tone": "warning"
      }
    ]
  },
  {
    "id": "camera",
    "name": "Insta360 remote",
    "surface": "Device · 240 × 320",
    "purpose": "Merapikan kontrol kamera tanpa menyentuh command yang sudah terbukti bekerja.",
    "source": "src/ui/engine/widget_registry.cpp:429–550 · src/hardware/ble_camera_remote.cpp",
    "current": "Pair 30 s, shutter, mode cycle, screen toggle, wake 10 s jika wake bytes tersedia, dan camera power-off. State recording kamera tidak diasumsikan tersedia.",
    "review": "Button kamera saat ini 26–34 px kecuali shutter. Usulan dua halaman: Capture dan Camera options, menjaga semua command sama.",
    "contract": "SHUTTER tetap toggle sesuai mode kamera. Jangan tampilkan 'recording started' tanpa camera acknowledgement. Tidak mengubah UUID, bytes, timing, pairing atau wake protocol.",
    "states": [
      {
        "id": "disconnected",
        "name": "Disconnected",
        "title": "Insta360",
        "status": "CAMERA NOT CONNECTED",
        "layout": "message",
        "copy": "Connect the camera",
        "detail": "Enable the camera's remote pairing mode.",
        "buttons": [
          [
            "Pair camera",
            "pairing"
          ],
          [
            "Camera options",
            "options"
          ]
        ],
        "note": "Beda istilah camera disconnected dan phone disconnected.",
        "tone": "warning"
      },
      {
        "id": "pairing",
        "name": "Pairing",
        "title": "Insta360",
        "status": "PAIRING · 30 s WINDOW",
        "layout": "message",
        "copy": "Pair on your camera",
        "detail": "Keep both devices nearby.",
        "buttons": [],
        "note": "Jangan klaim connected sampai subscribed.",
        "events": [
          [
            "Camera subscribed",
            "capture"
          ],
          [
            "Pair timed out",
            "disconnected"
          ]
        ]
      },
      {
        "id": "capture",
        "name": "Capture",
        "title": "Insta360",
        "status": "CAMERA CONNECTED",
        "layout": "message",
        "copy": "Camera remote",
        "detail": "Shutter acts on the camera's current mode.",
        "buttons": [
          [
            "Shutter",
            "sent"
          ],
          [
            "Camera options",
            "options"
          ]
        ],
        "note": "Shutter dominan, bukan power-off. Usulan grouping UI saja.",
        "tone": "success"
      },
      {
        "id": "sent",
        "name": "Shutter sent",
        "title": "Insta360",
        "status": "SHUTTER COMMAND REQUESTED",
        "layout": "message",
        "copy": "Check your camera",
        "detail": "No verified recording state is shown by this prototype.",
        "buttons": [
          [
            "Shutter again",
            "sent"
          ],
          [
            "Camera options",
            "options"
          ]
        ],
        "note": "Command sent ≠ camera recording. Copy feedback ini proposal; jangan invent acknowledgement.",
        "tone": "success"
      },
      {
        "id": "options",
        "name": "Options",
        "title": "Camera options",
        "status": "SAME EXISTING COMMANDS",
        "layout": "camera-options",
        "copy": "",
        "detail": "",
        "buttons": [],
        "note": "Semua command lama tetap tersedia. Tombol utilitas dibuat ≥44 px lewat halaman terpisah.",
        "events": [
          [
            "Wake bytes missing",
            "wake-disabled"
          ]
        ]
      },
      {
        "id": "waking",
        "name": "Wake",
        "title": "Wake camera",
        "status": "WAKE BEACON · 10 s",
        "layout": "message",
        "copy": "Looking for camera",
        "detail": "Wait for the camera to reconnect.",
        "buttons": [],
        "note": "Wake hanya jika configured wake bytes ada; bukan arbitrary BLE broadcast.",
        "events": [
          [
            "Camera connected",
            "capture"
          ],
          [
            "No response",
            "disconnected"
          ]
        ]
      },
      {
        "id": "wake-disabled",
        "name": "Wake unavailable",
        "title": "Wake unavailable",
        "status": "SETUP REQUIRED",
        "layout": "message",
        "copy": "Wake data not set",
        "detail": "Configure the camera-specific wake data first.",
        "buttons": [
          [
            "Back to options",
            "options"
          ],
          [
            "Pair camera",
            "pairing"
          ]
        ],
        "note": "Disabled action butuh alasan; jangan tombol redup yang tetap tampak bisa bekerja.",
        "tone": "warning"
      },
      {
        "id": "power-off",
        "name": "Camera off command",
        "title": "Insta360",
        "status": "CAMERA POWER OFF REQUESTED",
        "layout": "message",
        "copy": "Check the camera",
        "detail": "This command affects the camera, not OpenCyclo.",
        "buttons": [
          [
            "Camera options",
            "options"
          ],
          [
            "Back to ride",
            "ride:recording"
          ]
        ],
        "note": "Tidak mengubah command power-off. Konfirmasi tambahan hanya decision desain, belum diterapkan.",
        "tone": "warning"
      },
      {
        "id": "mode",
        "name": "Mode command",
        "title": "Insta360",
        "status": "MODE COMMAND REQUESTED",
        "layout": "message",
        "copy": "Check camera mode",
        "detail": "Existing cycle: Photo / Video / Time Shift. Not all modes are supported.",
        "buttons": [
          [
            "Back to capture",
            "capture"
          ],
          [
            "Camera options",
            "options"
          ]
        ],
        "note": "Jangan tawarkan FreeFrame jika remote tidak mendukung."
      },
      {
        "id": "screen-toggle",
        "name": "Screen command",
        "title": "Insta360",
        "status": "SCREEN TOGGLE REQUESTED",
        "layout": "message",
        "copy": "Camera screen toggled?",
        "detail": "Verify on camera; OpenCyclo recording is unchanged.",
        "buttons": [
          [
            "Back to capture",
            "capture"
          ],
          [
            "Camera options",
            "options"
          ]
        ],
        "note": "Feedback tidak mengklaim keadaan layar yang tidak dikonfirmasi."
      }
    ]
  },
  {
    "id": "power",
    "name": "Power & charging",
    "surface": "Device · 240 × 320",
    "purpose": "Membedakan layar tidur, power-off device, dan power-off kamera.",
    "source": "src/ui/power_menu.cpp · src/ui/charging_screen.cpp · docs/power-and-charging.md",
    "current": "BOOT singkat sleep/wake layar; tahan 2 s power menu. Power off/restart menutup GPX. Charge state tidak terdeteksi di hardware default.",
    "review": "Power menu perlu copy konsekuensi dan target 'OpenCyclo'. Jangan tampilkan baterai sebagai charging jika hardware tidak menyediakan status.",
    "contract": "Wake touch dikonsumsi, tidak mengaktifkan tombol di bawahnya. USB auto-wake / charge mode conditional pada external sense hardware.",
    "states": [
      {
        "id": "menu",
        "name": "Power menu",
        "title": "Power & battery",
        "status": "OPEN CYCLO · DEVICE",
        "layout": "message",
        "copy": "Battery estimate: 74%",
        "detail": "Power off / restart ends this ride.",
        "buttons": [
          [
            "Power off",
            "saving"
          ],
          [
            "Restart",
            "restart"
          ]
        ],
        "note": "Label menyebut device, bukan kamera.",
        "events": [
          [
            "Short BOOT: screen sleep",
            "screen-sleep"
          ],
          [
            "USB-sense hardware mode",
            "charge"
          ],
          [
            "OTA busy",
            "blocked"
          ]
        ]
      },
      {
        "id": "screen-sleep",
        "name": "Screen sleep",
        "title": "Screen off",
        "status": "RECORDING / BLE CONTINUE",
        "layout": "message",
        "copy": "Display is asleep",
        "detail": "Touch wakes the display only.",
        "buttons": [
          [
            "Simulate wake touch",
            "awake"
          ]
        ],
        "note": "Sleep layar bukan stop ride. Tombol di sini milik simulator, bukan layar firmware."
      },
      {
        "id": "awake",
        "name": "Awake",
        "title": "Screen awake",
        "status": "WAKE TOUCH CONSUMED",
        "layout": "message",
        "copy": "Ride unchanged",
        "detail": "The wake touch did not trigger any ride action.",
        "buttons": [
          [
            "Back to ride",
            "ride:recording"
          ],
          [
            "Power menu",
            "menu"
          ]
        ],
        "note": "Jangan ada tap-through ke Finish."
      },
      {
        "id": "saving",
        "name": "Closing GPX",
        "title": "Powering off",
        "status": "WAITING FOR LOGGER",
        "layout": "message",
        "copy": "Saving current ride",
        "detail": "Do not remove the SD card.",
        "buttons": [],
        "note": "Power flow memakai mekanisme shutdown yang ada.",
        "events": [
          [
            "Logger/BLE ready",
            "off"
          ],
          [
            "Shutdown busy",
            "blocked"
          ]
        ]
      },
      {
        "id": "off",
        "name": "Deep sleep",
        "title": "OpenCyclo off",
        "status": "DEEP SLEEP",
        "layout": "message",
        "copy": "Press BOOT to wake",
        "detail": "Device peripherals may still draw power.",
        "buttons": [
          [
            "Simulate BOOT wake",
            "ride:ready"
          ]
        ],
        "note": "Tampilan ini explanatory frame, layar fisik sebenarnya mati."
      },
      {
        "id": "restart",
        "name": "Restarting",
        "title": "Restart OpenCyclo",
        "status": "CLOSE GPX → RESTART",
        "layout": "message",
        "copy": "End current session",
        "detail": "Ride totals are not restored after restart.",
        "buttons": [],
        "note": "Jangan menyebut restart resume ride.",
        "events": [
          [
            "Restart completed",
            "ride:ready"
          ],
          [
            "Could not close",
            "blocked"
          ]
        ]
      },
      {
        "id": "blocked",
        "name": "Busy",
        "title": "Can't power off",
        "status": "OPERATION BUSY",
        "layout": "message",
        "copy": "Finish the transfer first",
        "detail": "A shutdown or update conflict must not corrupt the file.",
        "buttons": [
          [
            "Back",
            "menu"
          ]
        ],
        "note": "Busy bukan gagal simpan permanen.",
        "tone": "warning"
      },
      {
        "id": "charge",
        "name": "Charge mode",
        "title": "Charge mode",
        "status": "OPTIONAL USB-SENSE HARDWARE",
        "layout": "message",
        "copy": "Battery estimate: 74%",
        "detail": "Charging current/status unavailable.",
        "buttons": [
          [
            "Turn on",
            "ride:ready"
          ]
        ],
        "note": "CONDITIONAL: default board tidak punya USB presence sense. Jangan preview ini dianggap enabled.",
        "tone": "warning",
        "events": [
          [
            "USB unplugged",
            "off"
          ]
        ]
      }
    ]
  },
  {
    "id": "layout",
    "name": "Layout customization",
    "surface": "Flutter companion · responsive",
    "phone": true,
    "purpose": "Mengatur data layar lewat template dan slot yang didukung firmware.",
    "source": "app/lib/ui/screens/tabs/layout_builder_tab.dart · src/ui/engine/widget_catalog.cpp",
    "current": "Template/size-class contract, widget picker dan preview device; layout sync via app. Kamera sebagai full-container widget harus tetap tersedia.",
    "review": "Tunjukkan preview dan kompatibilitas slot sebelum Apply. Pisahkan perubahan lokal dari perubahan yang diterapkan ke device.",
    "contract": "Tidak menambahkan widget incompatible atau mengubah template/schema. Save/sync mock bukan write ke device.",
    "states": [
      {
        "id": "pages",
        "name": "Pages",
        "title": "Layout builder",
        "status": "LOCAL PREVIEW",
        "layout": "list",
        "copy": "",
        "detail": "",
        "buttons": [
          [
            "Edit ride page",
            "edit"
          ],
          [
            "Preview camera page",
            "camera-page"
          ]
        ],
        "note": "Mempertahankan camera widget bukan menghapusnya saat restyle.",
        "rows": [
          [
            "Ride · Hero 6-grid",
            "Speed / distance / time"
          ],
          [
            "Remote · Full container",
            "Insta360 controls"
          ]
        ]
      },
      {
        "id": "edit",
        "name": "Edit",
        "title": "Ride page",
        "status": "SELECT A SLOT",
        "layout": "message",
        "copy": "Hero: speed",
        "detail": "Small slots use only compatible widgets.",
        "buttons": [
          [
            "Preview layout",
            "preview"
          ],
          [
            "Cancel",
            "pages"
          ]
        ],
        "note": "Review pilihan compatible, bukan arbitrary drag semua jenis widget."
      },
      {
        "id": "preview",
        "name": "Preview",
        "title": "Device preview",
        "status": "NOT APPLIED YET",
        "layout": "message",
        "copy": "Ride page ready",
        "detail": "Speed · distance · moving time · heart rate",
        "buttons": [
          [
            "Apply to device",
            "applied"
          ],
          [
            "Keep editing",
            "edit"
          ]
        ],
        "note": "Usulan copy state Applied harus mengikuti ack existing flow.",
        "events": [
          [
            "Device disconnected",
            "disconnected"
          ]
        ]
      },
      {
        "id": "camera-page",
        "name": "Camera preserved",
        "title": "Remote page",
        "status": "FULL CONTAINER",
        "layout": "message",
        "copy": "Insta360 remote",
        "detail": "All existing camera commands remain present.",
        "buttons": [
          [
            "Preview camera flow",
            "camera:capture"
          ],
          [
            "Back to pages",
            "pages"
          ]
        ],
        "note": "Tidak memodifikasi behavior remote saat menyusun layout."
      },
      {
        "id": "applied",
        "name": "Applied",
        "title": "Layout synced",
        "status": "DEVICE ACKNOWLEDGED",
        "layout": "message",
        "copy": "Ready on OpenCyclo",
        "detail": "Your existing widgets and commands are preserved.",
        "buttons": [
          [
            "Back to pages",
            "pages"
          ],
          [
            "Preview ride",
            "ride:recording"
          ]
        ],
        "note": "Sukses dibedakan dari local draft.",
        "tone": "success"
      },
      {
        "id": "disconnected",
        "name": "Sync blocked",
        "title": "Device not linked",
        "status": "LAYOUT NOT APPLIED",
        "layout": "message",
        "copy": "Reconnect first",
        "detail": "Keep the local draft while reconnecting.",
        "buttons": [
          [
            "Device connection",
            "connection:disconnected"
          ],
          [
            "Back to preview",
            "preview"
          ]
        ],
        "note": "Draft yang belum tersimpan bukan sukses sync.",
        "tone": "warning"
      }
    ]
  },
  {
    "id": "ota",
    "name": "Firmware update",
    "surface": "Flutter companion · responsive",
    "phone": true,
    "purpose": "Mencegah user menyamakan file dipilih, bytes terkirim, dan firmware aktif.",
    "source": "app/lib/ui/screens/tabs/ota_update_tab.dart · app/lib/state/ota_provider.dart · src/hardware/ble_ota_handler.cpp",
    "current": "Pilih binary, transfer wireless, status success/error. Firmware update mengeksklusi route sync dan power-off.",
    "review": "Aksi update perlu checklist target/link/ride saved. Tombol UI saat ini tidak mengikat enabled state ke linked secara langsung. Ini temuan, bukan perubahan backend.",
    "contract": "Prototype tidak mem-flash ESP32. Tidak mengubah OTA protocol, partitions, reboot atau rollback semantics.",
    "states": [
      {
        "id": "pick",
        "name": "Choose binary",
        "title": "Firmware update",
        "status": "NO FILE SELECTED",
        "layout": "message",
        "copy": "Choose firmware.bin",
        "detail": "Make sure the build targets this OpenCyclo board.",
        "buttons": [
          [
            "Choose sample binary",
            "ready"
          ],
          [
            "Device connection",
            "connection:connected"
          ]
        ],
        "note": "Tidak menyarankan file firmware pihak lain.",
        "events": [
          [
            "No BLE link",
            "disconnected"
          ]
        ]
      },
      {
        "id": "ready",
        "name": "Preflight",
        "title": "Ready to update",
        "status": "VERIFY TARGET",
        "layout": "message",
        "copy": "OpenCyclo · ESP32-S3",
        "detail": "Save the current ride first. Keep the device awake and connected.",
        "buttons": [
          [
            "Start update",
            "sending"
          ],
          [
            "Choose another file",
            "pick"
          ]
        ],
        "note": "Preflight presentation proposal; tidak mengklaim backend punya semua verification.",
        "events": [
          [
            "Route transfer active",
            "busy"
          ]
        ]
      },
      {
        "id": "sending",
        "name": "Sending",
        "title": "Wireless update",
        "status": "SENDING · 64%",
        "layout": "message",
        "copy": "Keep devices nearby",
        "detail": "Do not power off during the update.",
        "buttons": [],
        "note": "Gunakan progress existing. Tidak ada success sebelum validasi.",
        "progress": 64,
        "events": [
          [
            "Transfer complete",
            "verifying"
          ],
          [
            "Link dropped",
            "failed"
          ]
        ]
      },
      {
        "id": "verifying",
        "name": "Verifying",
        "title": "Finishing update",
        "status": "100% SENT · VERIFYING",
        "layout": "message",
        "copy": "Wait for device",
        "detail": "Validation and reboot are separate from byte progress.",
        "buttons": [],
        "note": "Completion update app perlu diverifikasi terhadap ack + reconnect, bukan hanya progress bar.",
        "progress": 100,
        "events": [
          [
            "Device confirms / reboots",
            "success"
          ],
          [
            "Validation failed",
            "failed"
          ]
        ]
      },
      {
        "id": "success",
        "name": "Complete",
        "title": "Update complete",
        "status": "DEVICE CONFIRMED",
        "layout": "message",
        "copy": "Reconnect to OpenCyclo",
        "detail": "Verify the expected firmware on the device.",
        "buttons": [
          [
            "Device connection",
            "connection:disconnected"
          ],
          [
            "Back",
            "pick"
          ]
        ],
        "note": "Jangan menampilkan invented firmware version.",
        "tone": "success"
      },
      {
        "id": "failed",
        "name": "Failed",
        "title": "Update interrupted",
        "status": "NOT CONFIRMED",
        "layout": "message",
        "copy": "Reconnect and retry",
        "detail": "Do not assume the new image is active.",
        "buttons": [
          [
            "Reconnect",
            "connection:disconnected"
          ],
          [
            "Back",
            "ready"
          ]
        ],
        "note": "Pesan recovery membedakan failure dari device reboot.",
        "tone": "danger"
      },
      {
        "id": "busy",
        "name": "Busy",
        "title": "Update unavailable",
        "status": "ROUTE SYNC ACTIVE",
        "layout": "message",
        "copy": "Finish route sync first",
        "detail": "Only one protected transfer can run at a time.",
        "buttons": [
          [
            "Back",
            "ready"
          ]
        ],
        "note": "Memakai exclusion yang sudah ada, bukan menambah capability.",
        "tone": "warning"
      },
      {
        "id": "disconnected",
        "name": "No link",
        "title": "Device not linked",
        "status": "UPDATE UNAVAILABLE",
        "layout": "message",
        "copy": "Connect to OpenCyclo",
        "detail": "A selected file alone does not make update ready.",
        "buttons": [
          [
            "Connect device",
            "connection:disconnected"
          ],
          [
            "Back",
            "pick"
          ]
        ],
        "note": "Usulan disable start sebelum link ready, verifikasi UI saja.",
        "tone": "warning"
      }
    ]
  }
]);
const ride=window.REVIEW.flows[0];
ride.states.forEach(s=>{
 const backs={ready:"free-map:preview",recording:"dashboard:live",paused:"dashboard:paused",confirm:"recording","confirm-paused":"paused",saved:"done",error:"error-return","no-file":"ended-return",done:"saved"};
 s.back=backs[s.id];
 (s.buttons||[]).forEach(b=>{if(b[0]==="Start ride"||b[0]==="New ride")b[2]="newRide"});
});
ride.states.find(s=>s.id==="ready").buttons[1][1]="free-map:preview";
ride.states.find(s=>s.id==="error").buttons[1][1]="error-return";
ride.states.find(s=>s.id==="no-file").buttons[0][1]="ended-return";
ride.states.push(
{id:"error-return",name:"Back · unsaved",title:"Ride not saved",status:"SAVE FAILED · RETRY AVAILABLE",tone:"warning",layout:"message",copy:"Recording stopped",detail:"Map remains available. Your unsaved file is retained.",note:"Back does not discard the pending file or enable a new ride.",buttons:[["Retry from controls","error"],["Map","free-map:preview"]]},
{id:"ended-return",name:"Back · no file",title:"Ride ended",status:"NO GPX FILE SAVED",tone:"warning",layout:"message",copy:"Map remains available",detail:"Check SD logging before starting another ride.",note:"No false saved summary.",buttons:[["Ride controls","no-file"],["Map","free-map:preview"]]}
);
window.REVIEW.flows.push({
id:"dashboard",name:"Ride dashboard",surface:"Device · 240 × 320",purpose:"Data utama terbaca sekilas, dengan akses jelas ke map dan ride controls.",source:"src/ui/engine/layout_manager.cpp · src/ui/engine/template_engine.cpp",current:"Template dan widget size-class sudah ada; data live dan page swipes. Entry header membuka map/ride controls.",review:"Prototype mengusulkan dua shortcut named actions. Penempatan akhir harus disepakati tanpa menghapus widget / camera page.",contract:"Data hanya fixture. Tidak mengubah calculations, layout schema atau widget compatibility.",
states:[
{id:"live",name:"Live data",title:"Ride",status:"GPS 12 · RECORDING · 74%",tone:"success",layout:"dashboard",note:"Speed dominan; distance/time cukup besar. Map dan Ride tetap dua fungsi berbeda.",buttons:[["Map","free-map:follow"],["Ride","ride:recording"]],events:[["Auto pause","paused"]]},
{id:"paused",name:"Paused data",title:"Ride",status:"GPS 12 · PAUSED · 74%",tone:"warning",layout:"dashboard",note:"Pausing preserves this session. Control semantics use existing backend.",buttons:[["Map","free-map:follow"],["Ride","ride:paused"]]}
]});
