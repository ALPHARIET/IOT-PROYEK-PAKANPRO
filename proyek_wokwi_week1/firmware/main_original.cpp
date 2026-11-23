#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>

// Pilih 0 untuk Blocking (delay) dan 1 untuk Non-Blocking (millis).
// Ubah nilai ini, lalu lakukan Build ulang untuk membandingkan kedua mode.
#define MODE_NON_BLOCKING 1

#define PIN_DHT 4
#define TIPE_DHT DHT22
#define PIN_LED_ALERT 2
#define PIN_TOMBOL_DARURAT 15

DHT dht(PIN_DHT, TIPE_DHT);

unsigned long waktuBacaTerakhir = 0;
const unsigned long INTERVAL_TELEMETRI_MS = 2000;
const float AMBANG_SUHU_C = 32.0;
unsigned long nomorPaket = 0;
bool statusTombolSebelumnya = HIGH;

void kirimTelemetriJson(float suhu, float kelembapan, bool alertSuhu) {
  JsonDocument doc;

  doc["device_id"] = "esp32-unib-node01";
  doc["seq"] = ++nomorPaket;
  doc["uptime_ms"] = millis();
  doc["mode"] = MODE_NON_BLOCKING ? "non-blocking" : "blocking";
  doc["temp_c"] = suhu;
  doc["hum_pct"] = kelembapan;
  doc["alert"] = alertSuhu;

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
  // 1. Periksa tombol darurat pada setiap putaran loop.
  const bool statusTombol = digitalRead(PIN_TOMBOL_DARURAT);

  // INPUT_PULLUP: tombol yang ditekan akan terbaca LOW.
  if (statusTombol == LOW && statusTombolSebelumnya == HIGH) {
    Serial.printf(
      "{\"event\":\"emergency_button\",\"pressed_at_ms\":%lu}\n",
      millis()
    );

    // Tekanan tombol mengubah kondisi LED.
    digitalWrite(PIN_LED_ALERT, !digitalRead(PIN_LED_ALERT));
    delay(50); // Debounce singkat sesuai modul praktikum.
  }

  statusTombolSebelumnya = statusTombol;

  // 2. Pembacaan sensor: bandingkan mode Blocking dan Non-Blocking.
#if MODE_NON_BLOCKING == 0
  // BLOCKING: mikrokontroler menunggu selama 2.000 ms.
  delay(INTERVAL_TELEMETRI_MS);

  const float h = dht.readHumidity();
  const float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {
    const bool alert = (t >= AMBANG_SUHU_C);
    digitalWrite(PIN_LED_ALERT, alert ? HIGH : LOW);
    kirimTelemetriJson(t, h, alert);
  } else {
    Serial.println(F("{\"event\":\"sensor_error\",\"msg\":\"DHT22 read failed\"}"));
  }
#else
  // NON-BLOCKING: loop tetap berjalan sambil menunggu interval sensor.
  const unsigned long sekarang = millis();

  if (sekarang - waktuBacaTerakhir >= INTERVAL_TELEMETRI_MS) {
    waktuBacaTerakhir = sekarang;

    const float h = dht.readHumidity();
    const float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      const bool alert = (t >= AMBANG_SUHU_C);
      digitalWrite(PIN_LED_ALERT, alert ? HIGH : LOW);
      kirimTelemetriJson(t, h, alert);
    } else {
      Serial.println(F("{\"event\":\"sensor_error\",\"msg\":\"DHT22 read failed\"}"));
    }
  }
#endif
}
