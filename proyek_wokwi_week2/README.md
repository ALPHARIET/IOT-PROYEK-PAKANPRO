# Praktikum Minggu Ke-2 (Lab-1): Pengendali Greenhouse Kopi Berbasis ESP32

Repositori ini berisi implementasi resmi sistem otomasi iklim mikro greenhouse kopi berbasis ESP32, sensor digital suhu & kelembapan DHT22, emulator sensor kelembapan tanah kapasitif (potensiometer ADC1), relai pompa irigasi dengan logika histeresis, modulasi kecepatan kipas PWM (LEDC), serta fitur keselamatan *Dry-Run Protection* sesuai **Panduan Praktikum Minggu Ke-2 (Lab-1) — S1 Informatika FT UNIB**.

---

## 1. Pemetaan Pin dan Pengabelan Rangkaian

Berdasarkan aturan keamanan pin ESP32 (bebas konflik ADC2 dengan radio Wi-Fi dan bebas interferensi pin *strapping* GPIO 12), alokasi pin dikonfigurasi sebagai berikut:

| Komponen | Pin Modul | Pin ESP32 DevKit V1 | Keterangan / Warna Kabel |
|---|---|---|---|
| **Sensor Digital DHT22** (`dht1`) | VCC | 3V3 | Catu daya 3.3V (Merah) |
| | GND | GND.1 | Ground (Hitam) |
| | SDA / DATA | GPIO 4 (D4) | Pin digital serbaguna aman (Biru) |
| **Potensiometer Tanah** (`potSoil`) | VCC | 3V3 | Catu daya 3.3V (Merah) |
| | GND | GND.1 | Ground (Hitam) |
| | SIG / OUT | GPIO 34 (D34) | Input-only **ADC1_CH6** (Ungu) |
| **Pushbutton Override** (`btnMode`)| Pin 1.L | GPIO 15 (D15) | Tombol manual `INPUT_PULLUP` (Emas) |
| | Pin 2.L | GND.1 | Ground (Hitam) |
| **Modul Relai Pompa** (`relayPump`) | VCC | VIN (5V) | Catu daya koil relai (Merah) |
| | GND | GND.2 | Ground (Hitam) |
| | IN | GPIO 26 (D26) | Pengendali koil relai (Hijau) |
| **LED Indikator Pompa** (`ledPump`) | Anoda (A) | Resistor 220 Ω (`rPump`) -> GPIO 26 | Menyala saat relai pompa ON (Hijau) |
| | Katoda (C)| GND.2 | Ground (Hitam) |
| **LED Emulator Kipas** (`ledFanPwm`)| Anoda (A) | Resistor 220 Ω (`rFan`) -> GPIO 18 | PWM kecepatan kipas LEDC 5 kHz (Sian) |
| | Katoda (C)| GND.2 | Ground (Hitam) |
| **LED Peringatan Pipa/Tandon** (`ledWarn`)| Anoda (A) | Resistor 220 Ω (`rWarn`) -> GPIO 17 | Berkedip saat proteksi dry-run C6 aktif (Merah/Oranye) |
| | Katoda (C)| GND.2 | Ground (Hitam) |

---

## 2. Fitur & Logika Sistem

1. **Karakterisasi ADC1 & Kalibrasi Dua Titik Terbalik (Eksperimen 1):**
   - Menggunakan pembacaan tegangan terkalibrasi internal eFuse `analogReadMilliVolts(34)` untuk mengatasi ketaklinieran ADC mentah ESP32.
   - Dilengkapi filter derau *Moving Average* ($N = 5$ sampel jendela geser).
   - Pemetaan kelembapan tanah terbalik (*inverse linear mapping*):
     - Kering ($0\%$ lengas): $V_{\text{kering}} = 2650\text{ mV}$
     - Jenuh air ($100\%$ lengas): $V_{\text{basah}} = 1100\text{ mV}$
     $$\text{soil\_pct} = \frac{2650 - \text{soil\_mv}}{2650 - 1100} \times 100\%$$

2. **Kendali Histeresis Pompa vs Ambang Tunggal (Eksperimen 2):**
   - Dapat diatur melalui `#define GUNAKAN_HISTERESIS`:
     - **Mode 1 (Histeresis):** Pompa menyala jika $\text{soil\_pct} < 35.0\%$, dan baru mati saat $\text{soil\_pct} \ge 60.0\%$. Mencegah gejala *chattering* (saklar relai berbunyi berulang-ulang di batas ambang).
     - **Mode 0 (Ambang Tunggal):** Pompa menyala jika $\text{soil\_pct} < 45.0\%$. Menimbulkan lonjakan frekuensi klik relai (`relay_switches`) saat sinyal berfluktuasi.

3. **Kendali Proporsional PWM Kipas (Eksperimen 3):**
   - Modulasi perangkat keras LEDC ($5\text{ kHz}$, resolusi 8-bit $0\text{--}255$) pada GPIO 18.
   - Skala linier terhadap suhu udara DHT22:
     - $\le 28.0^\circ\text{C} \implies \text{Duty } 0\text{ (0\%)}$
     - $\ge 38.0^\circ\text{C} \implies \text{Duty } 255\text{ (100\%)}$
     - $31.5^\circ\text{C} \implies \text{Duty } 89\text{ (}\approx 35\%\text{)}$
     - $35.2^\circ\text{C} \implies \text{Duty } 183\text{ (}\approx 72\%\text{)}$

4. **Override Manual Tombol Non-Blocking:**
   - Membaca tombol pada GPIO 15 dengan *software debouncing* $60\text{ ms}$.
   - Menekan tombol memaksa pompa menyala (mode `MANUAL`) dan menekan kembali mengembalikan sistem ke otomatis.

5. **Fitur C6 (Mencipta) — Pengaman Waktu Jalan Maksimum Pompa (*Dry-Run Safety Cut-Off*):**
   - Apabila pompa menyala terus-menerus selama lebih dari $15\text{ detik}$ namun kelembapan tanah tidak kunjung naik (kenaikan $< 1.0\%$), sistem mendeteksi kegagalan (tandon air kosong, selang copot, atau sensor tercabut).
   - Pompa dimatikan secara otomatis demi keselamatan motor, sistem mengunci status `"fault": "DRY_RUN_PROTECTION"`, dan LED peringatan merah pada GPIO 17 berkedip cepat ($250\text{ ms}$).
   - Menekan tombol manual pada GPIO 15 me-*reset* status proteksi tersebut.

---

## 3. Format Telemetri JSON

Setiap interval $1000\text{ ms}$, sistem mengirimkan data telemetri terstruktur melalui Serial Monitor ($115200\text{ baud}$):

```json
{
  "device_id": "esp32-kopi-gh01",
  "seq": 14,
  "adc_raw": 3120,
  "soil_mv_avg": 2450,
  "soil_pct": "12.9",
  "temp_c": "29.4",
  "air_hum_pct": "68.2",
  "pump_on": true,
  "relay_switches": 1,
  "fan_pwm_pct": "14.1",
  "ctrl_mode": "HYSTERESIS"
}
```

Jika proteksi *dry-run* aktif, *payload* akan memuat atribut:
```json
{
  ...,
  "pump_on": false,
  "fault": "DRY_RUN_PROTECTION"
}
```

---

## 4. Cara Menjalankan Simulasi di Wokwi

1. Buka folder ini di **VS Code**.
2. Pastikan ekstensi **Wokwi Simulator** terpasang.
3. Buka berkas [diagram.json](file:///d:/Projek%20Wokwi%20Minggu%20Ke-2/diagram.json) lalu klik tombol **Start Simulation** (ikon Play hijau) atau tekan `F1` lalu ketik `Wokwi: Start Simulator`.
4. Buka **Serial Monitor** pada kecepatan **115200 baud**.
5. **Uji Eksperimen:**
   - Putar potensiometer tanah `potSoil` untuk mengamati perubahan `soil_pct` dan nyala relai pompa.
   - Klik komponen `dht1` lalu ubah slider suhu untuk melihat perubahan kecerahan LED kipas biru `ledFanPwm`.
   - Tekan tombol kuning `btnMode` untuk menguji mode paksa pompa `MANUAL`.
   - Biarkan kelembapan di bawah $35\%$ selama $15\text{ detik}$ tanpa menaikkan knop potensiometer untuk menguji proteksi *dry-run* C6 (LED merah berkedip dan pompa mati otomatis).
