#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define PIN_DHT 4
#define TIPE_DHT DHT22
#define PIN_LED_ALERT 2
#define PIN_TOMBOL_DARURAT 15

const float AMBANG_SUHU_C = 32.0;
const unsigned long INTERVAL_BACA_SENSOR_MS = 2000;
const unsigned long INTERVAL_NORMAL_MS = 2000;
const unsigned long INTERVAL_SIAGA_MS = 500;
const unsigned long DEBOUNCE_MS = 50;

DHT dht(PIN_DHT, TIPE_DHT);

unsigned long waktuBacaSensorTerakhir = 0;
unsigned long waktuKirimTerakhir = 0;
unsigned long waktuPerubahanTombol = 0;
unsigned long nomorPaket = 0;
unsigned long intervalTelemetriMs = INTERVAL_NORMAL_MS;

bool modeSiagaCepat = false;
bool statusTombolMentahSebelumnya = HIGH;
bool statusTombolStabil = HIGH;
bool sensorValid = false;

float suhuTerakhir = 0.0;
float kelembapanTerakhir = 0.0;

void kirimTelemetriJson() {
  JsonDocument doc;
  doc["device_id"] = "esp32-unib-node01";
  doc["seq"] = ++nomorPaket;
  doc["uptime_ms"] = millis();
  doc["mode"] = "non-blocking";
  doc["operation_mode"] = modeSiagaCepat ? "siaga-cepat" : "normal";
  doc["telemetry_interval_ms"] = intervalTelemetriMs;
  doc["temp_c"] = suhuTerakhir;
  doc["hum_pct"] = kelembapanTerakhir;
  doc["alert"] = (suhuTerakhir >= AMBANG_SUHU_C);
  serializeJson(doc, Serial);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_ALERT, OUTPUT);
  pinMode(PIN_TOMBOL_DARURAT, INPUT_PULLUP);
  digitalWrite(PIN_LED_ALERT, LOW);
  dht.begin();
  Serial.println(F("{\"event\":\"boot\",\"msg\":\"Simpul IoT UNIB Aktif\"}"));
}

void loop() {
  const unsigned long sekarang = millis();

  // Debounce tombol tanpa delay().
  const bool pembacaanTombol = digitalRead(PIN_TOMBOL_DARURAT);
  if (pembacaanTombol != statusTombolMentahSebelumnya) {
    waktuPerubahanTombol = sekarang;
    statusTombolMentahSebelumnya = pembacaanTombol;
  }

  if ((sekarang - waktuPerubahanTombol >= DEBOUNCE_MS) &&
      (pembacaanTombol != statusTombolStabil)) {
    statusTombolStabil = pembacaanTombol;

    if (statusTombolStabil == LOW) {
      modeSiagaCepat = !modeSiagaCepat;
      intervalTelemetriMs = modeSiagaCepat
          ? INTERVAL_SIAGA_MS
          : INTERVAL_NORMAL_MS;

      Serial.printf(
        "{\"event\":\"emergency_button\",\"operation_mode\":\"%s\",\"interval_ms\":%lu}\n",
        modeSiagaCepat ? "siaga-cepat" : "normal",
        intervalTelemetriMs
      );
    }
  }

  // DHT22 tetap dibaca paling cepat setiap 2 detik.
  if (sekarang - waktuBacaSensorTerakhir >= INTERVAL_BACA_SENSOR_MS) {
    waktuBacaSensorTerakhir = sekarang;
    const float h = dht.readHumidity();
    const float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      kelembapanTerakhir = h;
      suhuTerakhir = t;
      sensorValid = true;
      digitalWrite(PIN_LED_ALERT,
                   suhuTerakhir >= AMBANG_SUHU_C ? HIGH : LOW);
    } else {
      Serial.println(F("{\"event\":\"sensor_error\",\"msg\":\"DHT22 read failed\"}"));
    }
  }

  // Telemetri dapat dikirim setiap 500 ms saat siaga cepat.
  // Nilai sensor memakai hasil pembacaan terakhir yang valid.
  if (sensorValid &&
      sekarang - waktuKirimTerakhir >= intervalTelemetriMs) {
    waktuKirimTerakhir = sekarang;
    kirimTelemetriJson();
  }
}
