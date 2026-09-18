# GPS: cara kerja, cold/hot start, dan optimasi OpenCyclo

Update uji ride 13 September 2026: minimum satelit sementara diturunkan menjadi
5 untuk jalur NAV-PVT maupun fallback NMEA, atas permintaan pengguna. Ambang
hAcc, sAcc, HDOP, freshness, warm-up 2 detik, dan penolakan lonjakan tetap.
Angka 6/8 satelit di uraian awal di bawah adalah kebijakan sebelum uji ini.

Catatan 12 September 2026. Ini penjelasan teknis dan rencana investigasi;
perubahan download GPX tidak mengubah konfigurasi maupun filter GPS.

## Intinya dulu

Ada dua waktu yang perlu dibedakan: **receiver pertama kali menemukan posisi**
dan **aplikasi pertama kali menerima posisi itu sebagai cukup bagus**. Di
OpenCyclo, keduanya bisa jauh berbeda karena lokasi masih melewati filter.
Jadi laporan 15–20 menit belum membuktikan receiver butuh selama itu.

Hot start juga bukan sekadar nama fungsi di firmware. Ia bergantung pada
informasi yang masih valid saat receiver dinyalakan kembali.

## 1. Bagaimana posisi didapat?

Satelit mengirim sinyal waktu dan informasi orbit. Receiver membandingkan waktu
sinyal dikirim dengan waktu diterima untuk memperkirakan jarak. Dengan beberapa
satelit, receiver menyelesaikan posisi dan kesalahan jam internalnya. Untuk
solusi 3D mandiri, umumnya diperlukan minimal empat satelit; jumlah minimum itu
bukan jaminan posisi sudah akurat. Penjelasan visual:
[GPS.gov — trilateration](https://www.gps.gov/trilateration) dan
[NOAA — GPS positioning](https://oceanservice.noaa.gov/education/tutorial_geodesy/geo09_gps.html).

GPS adalah salah satu sistem GNSS. Galileo, GLONASS, dan BeiDou merupakan sistem
lain. Untuk OpenCyclo, jangan menyimpulkan konstelasi aktif hanya dari nama
modul: konfigurasi receiver perlu dibaca langsung.

Peta OSM bukan sumber lokasi. Receiver menghitung posisi; firmware menaruhnya
di peta. Mengganti kartu SD atau peta tidak mempercepat akuisisi satelit.
GPX juga bukan bantuan satelit: GPX menyimpan track/route, bukan data orbit.

## 2. Informasi apa yang mempercepat startup?

Receiver mendapat manfaat dari perkiraan waktu/lokasi dan informasi satelit:
almanac memberi gambaran konstelasi, sedangkan ephemeris diperlukan untuk
informasi orbit satelit yang lebih rinci. Bantuan GNSS dapat mengirim informasi
tersebut lewat koneksi data, sehingga tidak seluruhnya harus ditunggu dari
sinyal satelit. Ini berbeda dari sekadar mengirim koordinat terakhir.
[u-blox — Assisted GNSS](https://www.u-blox.com/en/technologies/agnss-assistnow)

## 3. Cold, warm, dan hot start

| Jenis | Kondisi informasi saat mulai | Konsekuensi |
|---|---|---|
| Cold | Informasi awal penting tidak tersedia/valid | Receiver harus mengakuisisi dan mendapatkan data yang diperlukan lagi |
| Warm | Sebagian informasi masih berguna, tetapi ephemeris perlu diperbarui | Masih ada pekerjaan mendapatkan data orbit |
| Hot | Informasi yang diperlukan, termasuk ephemeris, masih valid | Biasanya paling cepat |

Manual u-blox menjelaskan bahwa ephemeris biasanya sudah kedaluwarsa setelah
sekitar empat jam. Angka ini bukan batas universal untuk setiap receiver,
konstelasi, atau jenis assistance.
[u-blox — M10 integration manual](https://content.u-blox.com/sites/default/files/documents/MAX-M10M-00B_IntegrationManual_UBX-22038241.pdf)

u-blox menyebut cold start sekitar 30 detik dalam kondisi ideal, tetapi kondisi
sulit bisa membuatnya jauh lebih lama atau gagal. Itu bukan janji waktu untuk
modul RushFPV dalam casing kita. Bandingkan perangkat pada waktu, lokasi, dan
kondisi power yang sama.
[u-blox — Assisted GNSS](https://www.u-blox.com/en/technologies/agnss-assistnow)

## 4. Kenapa sleep belum tentu berarti hot start?

Backup domain menyimpan state receiver dan jam. Jika suplai backup hilang,
keuntungan startup dapat ikut hilang. Manual M10 meminta suplai V_BCKP untuk
mempertahankan BBR/RTC. Empat kabel RX/TX/GND/VCC tidak menjelaskan apakah
breakout memiliki penyimpanan energi backup, bagaimana jalurnya, atau berapa
lama ia bertahan.
[u-blox — MAX-M10N integration manual](https://content.u-blox.com/sites/default/files/documents/MAX-M10N_IntegrationManual_UBXDOC-304424225-19802.pdf)

Di kode OpenCyclo sekarang:

- Shutdown mengirim PMREQ setelah identifikasi receiver, lalu mengamati UART
  diam. Ini bukti komunikasi berhenti, bukan pengukuran arus backup.
- TX ditahan pada level idle selama ESP tidur.
- Saat bangun, firmware mengirim wake byte, membaca versi, lalu konfigurasi RAM.
- Tidak ada perintah sengaja menghapus backup atau memaksa cold reset.
- Parser/filter aplikasi di-reset agar posisi lama tidak dianggap data baru.
  Reset parser ESP berbeda dari menghapus memori navigasi receiver.

Artinya, mekanisme software mendukung bangun kembali, tetapi **retensi backup
secara listrik masih perlu dibuktikan**. Menyimpan latitude/longitude di NVS ESP
saja tidak otomatis membuat hot start.

## 5. Di mana optimasi paling berpengaruh?

### A. Antena dan lingkungan

Gedung, pepohonan, penggunaan indoor, serta pantulan sinyal dapat mengganggu
posisi. Geometri satelit dan kualitas receiver juga penting, bukan hanya jumlah
satelit. Sinyal pantulan disebut multipath.
[GPS.gov — GPS accuracy](https://www.gps.gov/gps-accuracy)

Usulan pengujian kita: bandingkan casing terbuka/tertutup di tempat yang sama;
jaga posisi antena konsisten; ukur dengan USB terpasang dan dengan baterai.
Perubahan hasil akan membantu melokalisasi sumber masalah, tetapi satu percobaan
belum membuktikan penyebab. Jangan mengubah banyak variabel sekaligus.

### B. Suplai dan backup

Ukur tegangan pada modul saat boot, display menyala, BLE aktif, dan saat sleep.
Uji restart ESP tanpa memutus suplai GPS, kemudian bandingkan dengan power-off
total. Ini membedakan reset host dari hilangnya state receiver. Pemeriksaan
jalur backup harus mengikuti skematik breakout yang sebenarnya.

### C. Konfigurasi receiver

Kode saat ini meminta 5 Hz, model portable, dan NAV-PVT melalui UART 115200.
Konfigurasi diterapkan ke RAM setelah versi protokol dikenali.

5 Hz adalah frekuensi keluaran pengukuran, bukan janji first fix lima kali lebih
cepat daripada 1 Hz. Rencana kita adalah membaca balik konfigurasi, menghitung
pesan valid/gagal checksum dan jeda UART, lalu menguji perubahan satu per satu.
Jangan mengaktifkan seluruh pesan debug terus-menerus tanpa menghitung beban UART.

### D. Filter dan tampilan

Ini titik software yang paling nyata untuk diperiksa pada OpenCyclo. Filter
saat ini meminta, untuk data dengan estimasi akurasi:

- usia data di bawah 1,5 detik;
- minimal 6 satelit;
- estimasi kesalahan horizontal maksimal 15 m;
- estimasi kesalahan speed maksimal 0,6 m/s;
- speed valid, serta pemeriksaan lonjakan;
- observasi memenuhi syarat selama minimal 2 detik.

Fallback NMEA meminta minimal 8 satelit dan HDOP maksimal 2. Satu penolakan
dapat mengulang fase penerimaan. Ini kebijakan aplikasi kita, bukan algoritma
Garmin atau iGPSPORT.

Usulan: pisahkan status lokasi dari status speed. Tampilkan posisi awal sebagai
akurasi rendah ketika layak, tetapi jangan langsung memasukkannya ke statistik
dan GPX seolah sudah andal. Smoothing tampilan juga perlu terpisah: tampilan
yang halus tidak membuktikan pengukuran benar.

### E. Assistance dari Flutter

Secara konsep, app bisa mengambil data assistance yang kompatibel lalu
mengirimkannya ke receiver. u-blox menyediakan AssistNow; dukungan pesan,
layanan, autentikasi, dan masa berlaku harus diverifikasi untuk hardware serta
firmware kita. Bantuan ini mengurangi pekerjaan akuisisi, bukan mengganti antena.
[u-blox — Assisted GNSS](https://www.u-blox.com/en/technologies/agnss-assistnow)

Belum diimplementasikan. Jangan menganggap iGPSPORT menggunakan layanan tertentu
tanpa mengetahui model dan dokumentasinya.

## 6. Rencana pengujian GPS berikutnya

Catat timeline sejak boot: UART pertama, pesan navigasi valid pertama, raw fix,
accepted fix, kemudian posisi layak recording. Sertakan alasan setiap penolakan,
hAcc, sAcc, satelit, HDOP, dan usia data.

Lakukan beberapa ulangan outdoor pada lokasi yang sama:

1. Restart host dengan GPS tetap mendapat suplai.
2. Sleep singkat lalu wake.
3. Power-off total, catat durasinya.
4. Bandingkan dengan iGPSPORT berdampingan.

Bandingkan median dan rentang waktu, bukan hanya percobaan tercepat. Jika raw fix
cepat tetapi accepted fix lambat, fokus ke filter. Jika raw fix juga lambat,
fokus ke RF, suplai, backup, konfigurasi, dan assistance. Jika timestamp sering
stale, telusuri UART/parser/task scheduling.

Kesimpulan: optimasi harus memisahkan **waktu akuisisi**, **kualitas posisi**,
**stabilitas speed**, dan **daya**. Menurunkan seluruh ambang filter sekaligus
bisa membuat tulisan “GPS ready” lebih cepat tanpa memperbaiki track.
