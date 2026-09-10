window.REVIEW.audit=[
  {
    "priority": "P1",
    "title": "Navigasi selesai bukan ride tersimpan",
    "finding": "Free ride, GPX navigation, dan recording punya lifecycle berbeda. User mudah mengira membuka route memulai recording atau Arrived menyimpan ride.",
    "evidence": "src/navigation/navigation.cpp: openNavigation, Control 0x04/0x05, arrived label; src/core/telemetry_state.cpp: requestFinishRide.",
    "proposal": "Pertahankan recording state di entry point Ride. Sesudah Arrived, arahkan ke Ride controls tanpa auto-finish. Map tetap bisa dipakai setelah save.",
    "acceptance": "Dari Arrived, user dapat menjelaskan apakah recording masih berjalan dan memilih Finish secara sadar.",
    "flow": "gpx"
  },
  {
    "priority": "P1",
    "title": "Akses Ride masih tersembunyi",
    "finding": "Dashboard memakai area rec/battery kanan atas (<28 px tinggi) sebagai entry point. Map memakai label ride pada header 32 px.",
    "evidence": "src/ui/ui_task.cpp: touchStartY<28 / touchStartX>=134; src/navigation/navigation.cpp: y1<32 & x1>=195.",
    "proposal": "Entry point bernama Ride dengan target ≥44 px. Pertahankan layout data; keputusan tempat shortcut perlu diuji di layar fisik.",
    "acceptance": "User menemukan Pause/Finish tanpa instruksi dan tidak salah membuka route library.",
    "flow": "ride"
  },
  {
    "priority": "P1",
    "title": "Kontrol Insta360 terlalu padat",
    "finding": "Shutter 58 px, tetapi pair 28 px, mode/screen/wake 34 px dan camera power-off 26 px. Target kecil rentan salah sentuh saat bersepeda.",
    "evidence": "src/ui/engine/widget_registry.cpp: CAM_PAIR_BTN_H / CAM_MODE_H / CAM_SCREEN_H / CAM_WAKE_H / CAM_POWEROFF_H.",
    "proposal": "Proposal dua halaman: Capture dan Camera options, semua action ≥44 px. Command, timing, pairing, wake bytes dan protokol tetap sama.",
    "acceptance": "Seluruh enam entry action (pair + lima commands) tetap tersedia. Tidak ada claim camera recording tanpa ack.",
    "flow": "camera"
  },
  {
    "priority": "P1",
    "title": "Sukses harus sesuai hasil, bukan progress",
    "finding": "Route sync sudah menunggu SAVED ack. Sementara error, no-file, transfer complete dan validation complete harus memiliki wording yang berbeda di semua flow.",
    "evidence": "app/lib/core/ble/route_transfer.dart; src/ui/ride_view.h; src/storage/ride_log_session.h; app/lib/state/ota_provider.dart.",
    "proposal": "Teks eksplisit Sending / Verifying / Saved. Saving GPX indeterminate, bukan angka progres buatan. Retry tidak menyatakan data hilang.",
    "acceptance": "Simulasi disconnect, checksum reject dan SD missing tidak pernah menampilkan Saved.",
    "flow": "routes-app"
  },
  {
    "priority": "P1",
    "title": "Kesiapan OTA belum cukup jelas",
    "finding": "CTA app memeriksa selected file dan flashing, tetapi tidak memasukkan linked ke kondisi enabled langsung. Proteksi backend bukan pengganti penjelasan UI.",
    "evidence": "app/lib/ui/screens/tabs/ota_update_tab.dart: onPressed (!hasFile || flashing); src/hardware/ble_ota_handler.cpp.",
    "proposal": "Disable CTA jika phone link tidak siap, tampilkan target build dan reminder save ride. Preflight tambahan merupakan proposal, belum behavior firmware.",
    "acceptance": "Tidak ada CTA seolah siap update ketika device disconnected. Jangan mengklaim battery validation bila API tidak menyediakannya.",
    "flow": "ota"
  },
  {
    "priority": "P2",
    "title": "Missing sensor tidak sama dengan measured zero",
    "finding": "Widget sensor dapat menampilkan 0 ketika data tidak tersedia. Itu bisa dibaca sebagai pengukuran valid.",
    "evidence": "src/ui/engine/widget_registry.cpp: cadence/heart-rate/power renderers.",
    "proposal": "Gunakan — dengan label unavailable/reconnecting pada presentation layer. Jangan mengubah nilai telemetry atau protokol sensor.",
    "acceptance": "User bisa membedakan connected with value 0 dan sensor disconnected.",
    "flow": "sensors"
  },
  {
    "priority": "P2",
    "title": "Teks bantuan terlalu kecil untuk outdoor",
    "finding": "Footer Font0 dan status kecil mudah dibaca di screenshot desktop tetapi belum terbukti terbaca pada layar 2.8 inci sambil bergerak.",
    "evidence": "src/ui/ride_menu.cpp: small() Font0; src/navigation/navigation.cpp: label() Font0.",
    "proposal": "12 px untuk status penting, ≥18 px aksi, 44–48 px touch target. Teks lisensi boleh sekunder; warning yang memengaruhi keputusan jangan di footer kecil.",
    "acceptance": "Uji matahari, getaran, jari basah dan glove. Jangan menyamakan px browser dengan usability hardware.",
    "flow": "free-map"
  },
  {
    "priority": "P2",
    "title": "Batas hardware charging perlu selalu jujur",
    "finding": "Board default tidak punya USB presence / charge-current status. Mode charge conditional; wake touch tidak boleh menekan tombol di bawahnya.",
    "evidence": "docs/power-and-charging.md; src/ui/power_menu.cpp; src/config/pins.h.",
    "proposal": "Battery estimate + charge status unavailable. Label OpenCyclo power off versus Camera power off. Tandai charge mode optional.",
    "acceptance": "Default board tidak mengklaim charging/fully charged. Power-off tetap menutup GPX sesuai flow saat ini.",
    "flow": "power"
  },
  {
    "priority": "GOOD",
    "title": "Jalur save sudah punya fondasi yang aman",
    "finding": "Cancel preserves active/paused, save state explicit, failed file retained, auto-start blocked after finish, new ride reset dibedakan.",
    "evidence": "src/core/telemetry_state.cpp; src/storage/gpx_writer.cpp; tests/test_ride_session.cpp; tests/test_gpx_writer.cpp.",
    "proposal": "Pertahankan seluruh behavior ini. Design system hanya menstandardkan kata, hierarchy dan posisi tombol.",
    "acceptance": "Regression tests tetap lolos sebelum adaptasi UI firmware.",
    "flow": "ride"
  },
  {
    "priority": "GOOD",
    "title": "Map preview dan GPX cues sudah diberi batas",
    "finding": "Preview bukan synthetic GPS; marker tersembunyi sebelum fix. Route bends dinyatakan sebagai hasil geometri, bukan verified junction routing.",
    "evidence": "docs/offline-navigation.md; src/navigation/navigation.cpp; app/lib/ui/screens/tabs/routes_tab.dart.",
    "proposal": "Pertahankan batas ini sebagai aturan copy lintas device dan app.",
    "acceptance": "Tidak ada mockup yang menyatakan auto-rerouting, live GPS saat waiting, atau Google-Maps-equivalent routing.",
    "flow": "free-map"
  }
];

