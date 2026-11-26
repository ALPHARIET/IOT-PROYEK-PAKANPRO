#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define LEBAR_LAYAR 128
#define TINGGI_LAYAR 64
#define ALAMAT_OLED 0x3C
#define PIN_LED_STATUS 2
#define PIN_WAKEUP_TOMBOL GPIO_NUM_15
#define DURASI_TIDUR_DETIK 10

Adafruit_SSD1306 display(LEBAR_LAYAR, TINGGI_LAYAR, &Wire, -1);
Adafruit_BMP280 bmp;

// --- VARIABEL PERSISTEN DI RTC SLOW MEMORY ---
RTC_DATA_ATTR int nomorSiklusBoot = 0;
RTC_DATA_ATTR float riwayatSuhuTerakhir = 0.0f;
RTC_DATA_ATTR float riwayatTekananTerakhir = 0.0f;

// [C6] PENYIMPAN EKSTREM CUACA (PERSISTEN DI RTC MEMORY)
RTC_DATA_ATTR float suhuMaks = -999.0f;
RTC_DATA_ATTR float suhuMin = 999.0f;
RTC_DATA_ATTR bool ekstremTerinisialisasi = false;

struct DataSensor {
    float suhu;
    float tekanan;
    float elevasi;
    bool valid;
} dataCuaca;

SemaphoreHandle_t mutexData;

void cetakDiagnostikWakeup() {
    esp_sleep_wakeup_cause_t alasan = esp_sleep_get_wakeup_cause();
    Serial.printf("\n========================================\n");
    Serial.printf("[RTC] Siklus Bangun Ke-%d\n", nomorSiklusBoot);
    switch (alasan) {
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("[RTC WAKEUP] Pemicu: Pewaktu Internal (RTC Timer)");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("[RTC WAKEUP] Pemicu: Tombol Darurat Luar (EXT0 pada GPIO 15)");
            break;
        default:
            Serial.println("[RTC WAKEUP] Pemicu: Power-on Reset / Boot Pertama");
            break;
    }
    Serial.printf("[RTC MEM] Riwayat Lalu -> Suhu: %.2f C | Tekanan: %.2f hPa\n",
                  riwayatSuhuTerakhir, riwayatTekananTerakhir);
    Serial.printf("[RTC C6] Rekor Ekstrem -> Suhu Maks: %.2f C | Suhu Min: %.2f C\n",
                  suhuMaks, suhuMin);
}

void perbaruiTampilanOled(float t, float p, float alt, int siklus, const char* pemicu) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Baris Judul
    display.setCursor(0, 0);
    display.printf("UNIB IoT - STASIUN CUACA");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Parameter Lingkungan
    display.setCursor(0, 14);
    display.printf("Suhu    : %.1f C", t);
    display.setCursor(0, 24);
    display.printf("Tekanan : %.1f hPa", p);
    display.setCursor(0, 34);
    display.printf("Elevasi : %.1f m", alt);

    // [C6] Rekor Suhu Ekstrem di Baris Bawah
    display.drawLine(0, 46, 127, 46, SSD1306_WHITE);
    display.setCursor(0, 49);
    display.printf("Max:%.1fC  Min:%.1fC", suhuMaks, suhuMin);
    display.setCursor(0, 57);
    display.printf("#%d | %s", siklus, pemicu);

    display.display();
}

void TaskTampilanDanSerial(void *pvParameters) {
    xSemaphoreTake(mutexData, portMAX_DELAY);
    DataSensor lokal = dataCuaca;
    xSemaphoreGive(mutexData);

    if (lokal.valid) {
        esp_sleep_wakeup_cause_t alasan = esp_sleep_get_wakeup_cause();
        const char* labelPemicu = (alasan == ESP_SLEEP_WAKEUP_EXT0) ? "EXT0 BTN" : "TIMER";

        // [C6] Update Rekor Ekstrem
        if (!ekstremTerinisialisasi) {
            suhuMaks = lokal.suhu;
            suhuMin = lokal.suhu;
            ekstremTerinisialisasi = true;
        } else {
            if (lokal.suhu > suhuMaks) suhuMaks = lokal.suhu;
            if (lokal.suhu < suhuMin) suhuMin = lokal.suhu;
        }

        // 1. Tampilkan ke Layar OLED SSD1306
        perbaruiTampilanOled(lokal.suhu, lokal.tekanan, lokal.elevasi, nomorSiklusBoot, labelPemicu);

        // 2. Serialisasi JSON
        JsonDocument doc;
        doc["device_id"] = "esp32-weather-st01";
        doc["boot_count"] = nomorSiklusBoot;
        doc["wakeup_cause"] = labelPemicu;
        doc["temp_c"] = serialized(String(lokal.suhu, 2));
        doc["pressure_hpa"] = serialized(String(lokal.tekanan, 2));
        doc["altitude_m"] = serialized(String(lokal.elevasi, 1));
        doc["prev_temp_c"] = serialized(String(riwayatSuhuTerakhir, 2));
        doc["max_temp_c"] = serialized(String(suhuMaks, 2));
        doc["min_temp_c"] = serialized(String(suhuMin, 2));
        serializeJson(doc, Serial);
        Serial.println();

        // Simpan ke memori RTC
        riwayatSuhuTerakhir = lokal.suhu;
        riwayatTekananTerakhir = lokal.tekanan;
    }
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_WAKEUP_TOMBOL, INPUT_PULLUP);
    digitalWrite(PIN_LED_STATUS, HIGH);

    nomorSiklusBoot++;
    cetakDiagnostikWakeup();

    mutexData = xSemaphoreCreateMutex();
    Wire.begin(21, 22);

    if (!display.begin(SSD1306_SWITCHCAPVCC, ALAMAT_OLED)) {
        Serial.println("Gagal menemukan layar OLED SSD1306!");
    }

    if (!bmp.begin(0x76)) {
        Serial.println("Gagal mendeteksi sensor BMP/BME280 pada alamat 0x76!");
        dataCuaca.valid = false;
    } else {
        bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                        Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_X16,
                        Adafruit_BMP280::FILTER_X16,
                        Adafruit_BMP280::STANDBY_MS_500);
        bmp.takeForcedMeasurement();

        xSemaphoreTake(mutexData, portMAX_DELAY);
        dataCuaca.suhu = bmp.readTemperature();
        dataCuaca.tekanan = bmp.readPressure() / 100.0F;
        dataCuaca.elevasi = bmp.readAltitude(1013.25);
        dataCuaca.valid = true;
        xSemaphoreGive(mutexData);
    }

    xTaskCreatePinnedToCore(TaskTampilanDanSerial, "TaskUI", 4096, NULL, 1, NULL, 0);
    delay(500);

    Serial.printf("[POWER] Menyiapkan Deep Sleep selama %d detik...\n", DURASI_TIDUR_DETIK);
    esp_sleep_enable_timer_wakeup((uint64_t)DURASI_TIDUR_DETIK * 1000000ULL);
    esp_sleep_enable_ext0_wakeup(PIN_WAKEUP_TOMBOL, 0);

    digitalWrite(PIN_LED_STATUS, LOW);
    Serial.println("[POWER] Zzz... Memasuki Deep Sleep. Arus turun ke ~10-150 uA.");
    Serial.flush();
    esp_deep_sleep_start();
}

void loop() {}
