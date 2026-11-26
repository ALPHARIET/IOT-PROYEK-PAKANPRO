# LAPORAN PRAKTIKUM MINGGU KE-3 (LAB-2)
**Mata Kuliah:** Teknologi Internet of Things (IoT)  
**Program Studi:** S1 Informatika — Fakultas Teknik Universitas Bengkulu  
**Topik:** Bus I2C, OLED SSD1306, FreeRTOS, & Deep Sleep  

---

## 1. TABEL PENGAMATAN HASIL EKSPERIMEN

### Tabel 3.1 — Hasil Diagnostik Pemindaian Alamat Bus I2C
| No | Nama Komponen Peranti | Alamat Heksadesimal | Keterangan Fungsi pada Sistem |
|:--:|:----------------------|:-------------------:|:------------------------------|
| 1  | Sensor Barometrik BMP280 | `0x76` (atau `0x77`) | Akuisisi telemetri suhu (°C), tekanan atmosfer (hPa), dan estimasi elevasi/ketinggian (m) |
| 2  | Display Grafis OLED SSD1306 | `0x3C` (atau `0x3D`) | Visualisasi antarmuka UI cuaca mikro resolusi 128x64 piksel monokrom via bus I2C bersama (SDA D21, SCL D22) |

### Tabel 3.2 — Pengamatan Siklus Bangun-Tidur (Duty Cycling) & Memori RTC
| Siklus | Sumber Pemicu Wakeup | Suhu (°C) | Tekanan (hPa) | Elevasi (m) | Nilai Riwayat Siklus Lalu di RTC |
|:------:|:--------------------|:---------:|:-------------:|:-----------:|:----------------------------------|
| Boot 1 | Power-on Reset / Boot Pertama | 24.50 | 1013.25 | 0.0 | Riwayat Suhu: 0.00 °C (Inisialisasi) |
| Boot 2 | RTC Timer (10 detik) | 24.62 | 1013.10 | 1.2 | Riwayat Suhu: 24.50 °C |
| Boot 3 | RTC Timer (10 detik) | 24.70 | 1012.95 | 2.5 | Riwayat Suhu: 24.62 °C |
| Boot 4 | Tombol Darurat EXT0 (GPIO 15) | 24.68 | 1013.00 | 2.1 | Riwayat Suhu: 24.70 °C |
| Boot 5 | RTC Timer (10 detik) | 24.75 | 1012.80 | 3.8 | Riwayat Suhu: 24.68 °C |

---

## 2. JAWABAN PERTANYAAN ANALISIS (LEVEL C1 – C6)

### 1. [C1 – Mengingat] Konsumsi Arus & Komponen Hardware yang Dimatikan pada Deep Sleep
* **Konsumsi Arus:**
  - **Mode Aktif Penuh (Active Mode):** Mengonsumsi arus rata-rata **80 mA – 240 mA** (tergantung apakah modem Wi-Fi / Bluetooth dan kedua inti Xtensa dual-core 240 MHz menyala).
  - **Mode Deep Sleep:** Arus turun drastis hingga **10 µA – 150 µA** (pada chip mentah ESP32 arus dapat mencapai ~5–10 µA, sedangkan pada papan modul DevKit dengan LDO regulator dan chip USB-to-UART CP2102/CH340 berkisar ~100–150 µA).
* **Komponen Hardware yang Dimatikan:**
  - Inti prosesor utama (Xtensa LX6 CPU Core 0 & Core 1).
  - Main System RAM (SRAM internal 520 KB).
  - Baseband & Radio nirkabel (Modem Wi-Fi 802.11 b/g/n dan Bluetooth 4.2 BR/EDR/BLE).
  - Seluruh peripheral bus digital standar (I2C, SPI, UART biasa, I2S, PWM, ADC biasa).
* **Komponen yang Tetap Aktif:**
  - RTC Controller (Low-Power Core / ULP Coprocessor).
  - RTC Fast Memory & RTC Slow Memory (8 KB).
  - RTC Peripherals (RTC GPIO seperti GPIO 15, Internal RTC Timer).

---

### 2. [C2 – Memahami] Mengapa Variabel Global Tereset, Sedangkan `RTC_DATA_ATTR` Persisten?
* **Penyebab Variabel Global Tereset:**
  - Variabel global biasa dialokasikan di dalam **Main SRAM (Internal RAM)**.
  - Saat ESP32 memasuki mode Deep Sleep, catu daya listrik ke domain sistem utama dan Main SRAM diputus total (*power-gated*) demi menghemat energi.
  - Ketika ESP32 terbangun (*wakeup*), prosesor mengalami proses **reboot / software reset**. Bootloader dieksekusi ulang dari ROM/Flash, dan area BSS/Data pada Main SRAM diinisialisasi ulang ke nilai default (`0`).
* **Mengapa `RTC_DATA_ATTR` Persisten:**
  - Atribut `RTC_DATA_ATTR` mengarahkan compiler untuk menempatkan variabel pada section memori khusus yaitu **RTC Slow Memory (SRAM sebesar 8 KB)**.
  - Domain daya RTC Slow Memory didukung oleh rel tegangan *Always-On Power Domain* (VDD33_RTC) yang **tetap dialiri listrik selama siklus Deep Sleep**.
  - Saat bangun, bootloader tidak mengosongkan area RTC Slow Memory, sehingga data variabel (seperti `nomorSiklusBoot`, `riwayatSuhuTerakhir`) tetap utuh.

---

### 3. [C3 – Menerapkan] Perhitungan Konsumsi Arus Rata-Rata ($I_{avg}$) dan Masa Pakai Baterai

#### Parameter Diketahui:
- Durasi tidur ($t_{tidur}$) = $899\text{ detik}$
- Arus saat tidur ($I_{tidur}$) = $40\ \mu\text{A} = 0.04\text{ mA}$
- Durasi aktif ($t_{aktif}$) = $1\text{ detik}$
- Arus saat aktif ($I_{aktif}$) = $85\text{ mA}$
- Periode satu siklus ($T_{total}$) = $t_{aktif} + t_{tidur} = 1 + 899 = 900\text{ detik}$ ($15\text{ menit}$)
- Kapasitas baterai nominal = $3000\text{ mAh}$
- Efisiensi / Derating faktor ($\eta$) = $0.80$ ($80\%$)

#### A. Perhitungan Arus Rata-Rata ($I_{avg}$):
$$I_{avg} = \frac{(I_{aktif} \times t_{aktif}) + (I_{tidur} \times t_{tidur})}{T_{total}}$$
$$I_{avg} = \frac{(85\text{ mA} \times 1\text{ s}) + (0.04\text{ mA} \times 899\text{ s})}{900\text{ s}}$$
$$I_{avg} = \frac{85 + 35.96}{900} = \frac{120.96}{900} \approx \mathbf{0.1344\text{ mA}}\ (134.4\ \mu\text{A})$$

#### B. Perhitungan Kapasitas Efektif Baterai ($C_{eff}$):
$$C_{eff} = 3000\text{ mAh} \times 0.80 = \mathbf{2400\text{ mAh}}$$

#### C. Perhitungan Estimasi Masa Pakai:
- **Total Jam Operasional ($t_{jam}$):**
  $$t_{jam} = \frac{C_{eff}}{I_{avg}} = \frac{2400\text{ mAh}}{0.1344\text{ mA}} \approx \mathbf{17857.14\text{ jam}}$$
- **Total Hari Operasional ($t_{hari}$):**
  $$t_{hari} = \frac{17857.14}{24} \approx \mathbf{744.05\text{ hari}}$$
- **Estimasi dalam Satuan Bulan (Asumsi 1 bulan = 30 hari):**
  $$t_{bulan} = \frac{744.05}{30} \approx \mathbf{24.8\text{ Bulan}}\ (\approx \mathbf{2.07\text{ Tahun}})$$

---

### 4. [C4 – Menganalisis] Keuntungan Pembagian Tugas Konkuren FreeRTOS (Dual-Core ESP32)
* Pada Listing Program 3, fungsi `setup()` berjalan secara default pada **Core 1 (App Core)**, sedangkan task visualisasi UI `TaskTampilanDanSerial` dialokasikan ke **Core 0 (Protocol Core)** melalui `xTaskCreatePinnedToCore()`.
* **Analisis Keuntungan Arsitektur:**
  1. **Mencegah Fenomena Blocking I/O:** Transmisi buffer grafis OLED SSD1306 (resolusi 128×64 piksel = 1024 byte bitmask) via protokol Wire I2C pada kecepatan standar 100/400 kHz membutuhkan waktu transfer signifikan (~25–40 ms). Selain itu, serialisasi JSON melalui ArduinoJson juga memakan siklus komputasi CPU.
  2. **Independensi Akuisisi Sensor:** Dengan mendelegasikan tugas display ke Core 0, Core 1 dapat segera mengeksekusi pengukuran presisi sensor BMP280 (`takeForcedMeasurement()`) tanpa terhambat oleh antrean transmisi display.
  3. **Keamanan Data dengan Mutex:** Penggunaan FreeRTOS Mutex (`xSemaphoreCreateMutex()`) memastikan transfer data dari struct `dataCuaca` ke variabel lokal task visualisasi berlangsung atomik tanpa resiko *race condition* atau data korup.

---

### 5. [C5 – Mengevaluasi] Perbandingan Pemicu Bangun EXT0 vs EXT1 untuk Stasiun Banjir (3 Sensor Ambang Air)

| Parameter Evaluasi | EXT0 (External Wakeup 0) | EXT1 (External Wakeup 1) |
|:-------------------|:-------------------------|:-------------------------|
| **Kapasitas Pin** | **Hanya 1 Pin Tunggal** (Hanya 1 RTC GPIO) | **Multi-Pin (Bitmap)** (Dapat memetakan beberapa RTC GPIO sekaligus) |
| **Kemampuan Multi-Sensor** | **Kurang Cocok.** Jika ada 3 sensor air terpisah (misal: Siaga 3, Siaga 2, Siaga 1), EXT0 tidak bisa memonitor ketiganya tanpa komponen gerbang logika luar (OR-gate hardware eksternal). | **Sangat Unggul.** Dapat langsung mendaftarkan pin sensor 1, 2, dan 3 dalam satu bitmask: `esp_sleep_enable_ext1_wakeup(pin_mask, ESP_EXT1_WAKEUP_ANY_HIGH)`. |
| **Identifikasi Sumber Pemicu** | Hanya mengetahui bahwa modul terbangun dari pin tunggal tersebut. | Dapat melacak pin spesifik mana yang aktif menggunakan fungsi bawaan `esp_sleep_get_ext1_wakeup_status()`. |
| **Konsumsi Daya** | Menggunakan komparator RTC analog internal, konsumsi daya sangat rendah (~10 µA). | Menggunakan digital logic RTC IO, konsumsi arus seimbang namun kapabilitas jauh lebih fleksibel. |
| **Vonis untuk Deteksi Banjir** | **DITOLAK.** Terlalu terbatas untuk sistem pemantauan multi-level air. | **DIREKOMENDASIKAN (Pilihan Terbaik).** ESP32 dapat tidur hemat daya dan seketika bangun jika salah satu ambang air (Siaga 3, 2, atau 1) menyentuh batas kritis. |

---

### 6. [C6 – Mencipta] Modifikasi Kode Penyimpan Ekstrem Cuaca (Suhu Maks & Min)

Kode modifikasi telah diimplementasikan pada berkas `firmware/src/main_c6_extreme.cpp` dengan perubahan utama:
1. Deklarasi variabel persisten di RTC Slow Memory:
   ```cpp
   RTC_DATA_ATTR float suhuMaks = -999.0f;
   RTC_DATA_ATTR float suhuMin = 999.0f;
   RTC_DATA_ATTR bool ekstremTerinisialisasi = false;
   ```
2. Pembaruan logika penentuan nilai ekstrem saat task menerima data valid:
   ```cpp
   if (!ekstremTerinisialisasi) {
       suhuMaks = lokal.suhu;
       suhuMin = lokal.suhu;
       ekstremTerinisialisasi = true;
   } else {
       if (lokal.suhu > suhuMaks) suhuMaks = lokal.suhu;
       if (lokal.suhu < suhuMin) suhuMin = lokal.suhu;
   }
   ```
3. Penambahan baris status rekor ekstrem pada visualisasi OLED SSD1306:
   ```cpp
   display.drawLine(0, 46, 127, 46, SSD1306_WHITE);
   display.setCursor(0, 49);
   display.printf("Max:%.1fC  Min:%.1fC", suhuMaks, suhuMin);
   ```
4. Penambahan field telemetri JSON:
   ```cpp
   doc["max_temp_c"] = serialized(String(suhuMaks, 2));
   doc["min_temp_c"] = serialized(String(suhuMin, 2));
   ```
