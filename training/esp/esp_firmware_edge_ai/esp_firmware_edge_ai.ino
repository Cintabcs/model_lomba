/*
  =============================================================================
  ESP32 EDGE AI WATER QUALITY CLASSIFIER FIRMWARE (30s TEST + 5s INFERENCE)
  =============================================================================
  Fitur Sistem:
  - 1x Push Button: Pemicu mulai pengujian 30 detik (INPUT_PULLUP ke GND).
  - 1x LED Putih:
      * Standby / Siap: MENYALA TERUS (Solid ON).
      * Sedang Proses Uji (30 Detik): KEDIP-KEDIP (Blinking 200 ms).
  - 3x LED Hasil Klasifikasi:
      * Hijau  : Low (Air Bersih / PDAM)
      * Kuning : Medium (Air Sedang)
      * Merah  : High (Air Kotor / Keruh)
  - Edge AI Model: Random Forest 5-detik windowing (< 1 ms inferensi).
  - Selama 30 detik, model mengambil keputusan setiap 5 detik dan memperbarui LED.
  - Setelah 30 detik selesai, hasil akhir (voting mayoritas) dikunci di LED.
  =============================================================================
*/

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #error "Board tidak didukung! Gunakan ESP32 atau ESP8266."
#endif

#include "rf_model.h" // Model Random Forest Edge AI

// =============================================================================
// KONFIGURASI PIN SENSOR, LED & PUSH BUTTON
// =============================================================================
#if defined(ESP32)
  // Pin Sensor Analog
  const int PIN_TDS           = 34; // Sinyal Analog TDS (GPIO34 / ADC1)
  const int PIN_TURBIDITY     = 35; // Sinyal Analog Turbidity (GPIO35 / ADC1)

  // Pin Indikator LED Klasifikasi Hasil (Sesuai Jalur PCB)
  const int PIN_LED_RED       = 23; // LED Merah  -> High (Air Kotor / Keruh) (D23)
  const int PIN_LED_YELLOW    = 22; // LED Kuning -> Medium (Air Sedang)      (D22)
  const int PIN_LED_GREEN     = 25; // LED Hijau  -> Low (Air Bersih)         (D25)

  // Pin Indikator Status & Tombol Trigger (Sesuai Jalur PCB)
  const int PIN_LED_WHITE     = 33; // LED Putih  -> Standby Solid ON / Blink saat Proses (D33)
  const int PIN_BUTTON        = 32; // Push Button (D32, INPUT_PULLUP ke GND)

  const float VREF            = 3.3;       
  const float ADC_RESOLUTION  = 4095.0; // 12-bit ADC ESP32
#else
  const int PIN_TDS           = A0;
  const int PIN_TURBIDITY     = A0;
  const int PIN_LED_GREEN     = D1;
  const int PIN_LED_YELLOW    = D2;
  const int PIN_LED_RED       = D3;
  const int PIN_LED_WHITE     = D4;
  const int PIN_BUTTON        = D5;
  const float VREF            = 3.3;
  const float ADC_RESOLUTION  = 1023.0;
#endif

// =============================================================================
// EDGE AI RING BUFFER & KONTROL PENGUJIAN 30 DETIK
// =============================================================================
const int WINDOW_SIZE = 50; // 5 Detik = 50 Sampel pada 10 Hz
float tdsBuffer[WINDOW_SIZE];
float turbBuffer[WINDOW_SIZE];
int bufferIndex = 0;
int bufferCount = 0;
String currentAiPrediction = "Standby";

// Pengaturan Durasi Pengujian (30 Detik)
bool isTesting = false;
unsigned long testStartTime = 0;
const unsigned long TEST_DURATION_MS = 30000; // 30 Detik

// Timer Blinking LED Putih (Non-blocking)
unsigned long lastBlinkTime = 0;
bool whiteLedState = HIGH;
const unsigned long BLINK_INTERVAL_MS = 200; // Kedip setiap 200 ms

// Akumulator Voting Keputusan 5-detik selama 30 detik
int voteCountLow = 0;
int voteCountMed = 0;
int voteCountHigh = 0;
int totalDecisions = 0;

// =============================================================================
// FUNGSI PENGENDALI INDIKATOR LED KLASIFIKASI
// =============================================================================
void setClassificationLeds(String label) {
  if (label == "Low") {
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
  } else if (label == "Medium") {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
  } else if (label == "High") {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
  } else {
    // Matikan semua LED hasil (saat standby atau reset)
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
  }
}

// =============================================================================
// FUNGSI DEBOUNCE PUSH BUTTON (NON-BLOCKING)
// =============================================================================
bool checkButtonPressed() {
  static unsigned long lastDebounceTime = 0;
  static int lastState = HIGH;
  static int buttonState = HIGH;
  int reading = digitalRead(PIN_BUTTON);
  bool triggered = false;

  if (reading != lastState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 50) { // Debounce 50 ms
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) { // Tombol ditekan (karena INPUT_PULLUP)
        triggered = true;
      }
    }
  }
  lastState = reading;
  return triggered;
}

// =============================================================================
// FUNGSI INFERENSI EDGE AI (SETIAP 5 DETIK / 50 SAMPEL)
// =============================================================================
void updateEdgeAiWindow(float tds, float turb, unsigned long currentTestElapsed) {
  tdsBuffer[bufferIndex] = tds;
  turbBuffer[bufferIndex] = turb;
  bufferIndex = (bufferIndex + 1) % WINDOW_SIZE;
  if (bufferCount < WINDOW_SIZE) bufferCount++;

  // Jalankan klasifikasi setiap 5 detik (50 sampel)
  if (bufferCount == WINDOW_SIZE && (bufferIndex % WINDOW_SIZE == 0)) {
    float tdsMean = 0, tdsMin = 99999, tdsMax = -99999;
    float turbMean = 0, turbMin = 99999, turbMax = -99999;

    for (int i = 0; i < WINDOW_SIZE; i++) {
      tdsMean += tdsBuffer[i];
      if (tdsBuffer[i] < tdsMin) tdsMin = tdsBuffer[i];
      if (tdsBuffer[i] > tdsMax) tdsMax = tdsBuffer[i];

      turbMean += turbBuffer[i];
      if (turbBuffer[i] < turbMin) turbMin = turbBuffer[i];
      if (turbBuffer[i] > turbMax) turbMax = turbBuffer[i];
    }
    tdsMean /= WINDOW_SIZE;
    turbMean /= WINDOW_SIZE;

    // Hitung Standar Deviasi
    float tdsVar = 0, turbVar = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
      tdsVar += (tdsBuffer[i] - tdsMean) * (tdsBuffer[i] - tdsMean);
      turbVar += (turbBuffer[i] - turbMean) * (turbBuffer[i] - turbMean);
    }
    float tdsStd = sqrt(tdsVar / WINDOW_SIZE);
    float turbStd = sqrt(turbVar / WINDOW_SIZE);

    // Array 8 Fitur Statistik untuk Random Forest Edge AI
    float features[8] = {
      tdsMean, tdsStd, tdsMin, tdsMax,
      turbMean, turbStd, turbMin, turbMax
    };

    // Inferensi Model Edge AI (< 0.2 ms pada ESP32)
    currentAiPrediction = EdgeWaterClassifier::predictLabel(features);
    totalDecisions++;

    if (currentAiPrediction == "Low") voteCountLow++;
    else if (currentAiPrediction == "Medium") voteCountMed++;
    else if (currentAiPrediction == "High") voteCountHigh++;

    // Perbarui LED hasil sesuai prediksi 5-detikan terkini
    setClassificationLeds(currentAiPrediction);

    int elapsedSec = currentTestElapsed / 1000;
    int remainingSec = (TEST_DURATION_MS - currentTestElapsed) / 1000;
    if (remainingSec < 0) remainingSec = 0;

    Serial.println("\n-----------------------------------------------------------------");
    Serial.print(" [🤖 KEPUTUSAN 5-DETIK] Waktu: "); Serial.print(elapsedSec);
    Serial.print("s / 30s (Sisa: "); Serial.print(remainingSec); Serial.println("s)");
    Serial.print(" => KELAS SEMENTARA: "); Serial.println(currentAiPrediction);
    Serial.print("    TDS  : Mean="); Serial.print(tdsMean, 1);
    Serial.print(" | Min="); Serial.print(tdsMin, 1);
    Serial.print(" | Max="); Serial.print(tdsMax, 1);
    Serial.print(" | Std="); Serial.println(tdsStd, 1);
    Serial.print("    TURB : Mean="); Serial.print(turbMean, 1);
    Serial.print(" | Min="); Serial.print(turbMin, 1);
    Serial.print(" | Max="); Serial.print(turbMax, 1);
    Serial.print(" | Std="); Serial.println(turbStd, 1);
    Serial.println("-----------------------------------------------------------------");
  }
}

// =============================================================================
// PILIH MODE WIFI
// =============================================================================
#define USE_HOME_WIFI true  

#if USE_HOME_WIFI
  const char *wifi_ssid     = "MyRepublic_C2222847";
  const char *wifi_password = "C2222847"; 
#else
  const char *wifi_ssid     = "ESP_DA_Logger";
  const char *wifi_password = "12345678";
#endif

const int SERVER_PORT = 8080;
WiFiServer server(SERVER_PORT);
WiFiClient client;

const unsigned long SAMPLE_INTERVAL_MS = 100; // 10 Hz (100 ms)
unsigned long lastSampleTime = 0;
unsigned long sampleId = 0;
float defaultTemperature = 25.0; // Suhu default air (°C)

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================================");
  Serial.println("  ESP32 EDGE AI WATER QUALITY CLASSIFIER (30s SYSTEM)");
  Serial.println("========================================================");

  // Konfigurasi Pin LED & Push Button
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Status Awal Standby:
  digitalWrite(PIN_LED_WHITE, HIGH); // LED Putih MENYALA TERUS saat standby
  setClassificationLeds("");        // Matikan semua LED hasil klasifikasi

  #if defined(ESP32)
    analogSetAttenuation(ADC_11db); // Rentang ADC 0 - 3.3V
    pinMode(PIN_TDS, INPUT);
    pinMode(PIN_TURBIDITY, INPUT);
  #endif

  #if USE_HOME_WIFI
    Serial.print("Connecting ke Wi-Fi: ");
    Serial.println(wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_password);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 25) {
      delay(400);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[✓] Terhubung ke Wi-Fi!");
      Serial.print("IP ESP32: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\n[!] Gagal ke Wi-Fi, mengaktifkan AP Mode...");
      WiFi.mode(WIFI_AP);
      WiFi.softAP("ESP_DA_Logger", "12345678");
    }
  #else
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifi_ssid, wifi_password);
  #endif

  server.begin();
  Serial.println("TCP Server siap pada port 8080.");
  Serial.println("\n>>> SISTEM STANDBY: LED Putih ON. Tekan Push Button untuk Mulai Uji 30 Detik! <<<\n");
}

// =============================================================================
// LOOP UTAMA
// =============================================================================
void loop() {
  unsigned long currentMillis = millis();

  // 1. Cek Koneksi Client Baru (Laptop)
  if (!client || !client.connected()) {
    client = server.available();
  }

  // 2. Baca Push Button untuk Memulai Uji 30 Detik
  if (!isTesting && checkButtonPressed()) {
    isTesting = true;
    testStartTime = currentMillis;
    bufferIndex = 0;
    bufferCount = 0;
    voteCountLow = 0;
    voteCountMed = 0;
    voteCountHigh = 0;
    totalDecisions = 0;
    currentAiPrediction = "Testing...";
    setClassificationLeds(""); // Reset LED hasil

    Serial.println("\n========================================================");
    Serial.println(" [▶ PUSH BUTTON DITEKAN] MEMULAI PENGUJIAN 30 DETIK!");
    Serial.println(" LED Putih Berkedip (Proses Akuisisi & Inferensi AI)...");
    Serial.println("========================================================");
  }

  // 3. Kontrol Indikator LED Putih
  if (isTesting) {
    // Saat proses uji 30 detik: LED Putih KEDIP-KEDIP (Blinking)
    if (currentMillis - lastBlinkTime >= BLINK_INTERVAL_MS) {
      lastBlinkTime = currentMillis;
      whiteLedState = !whiteLedState;
      digitalWrite(PIN_LED_WHITE, whiteLedState);
    }
  } else {
    // Saat standby / tidak menguji: LED Putih MENYALA TERUS (Solid ON)
    digitalWrite(PIN_LED_WHITE, HIGH);
  }

  // 4. Siklus Sampling Sensor 10 Hz (100 ms)
  if (currentMillis - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = currentMillis;
    sampleId++;

    // A. Baca Sensor TDS (GPIO34)
    int tdsRaw = analogRead(PIN_TDS);
    float tdsVoltage = tdsRaw * (VREF / ADC_RESOLUTION);
    float compCoeff = 1.0 + 0.02 * (defaultTemperature - 25.0);
    float compVolt = tdsVoltage / compCoeff;
    float tdsPpm = (133.42 * compVolt * compVolt * compVolt 
                    - 255.86 * compVolt * compVolt 
                    + 857.39 * compVolt) * 0.5;
    if (tdsPpm < 0) tdsPpm = 0;

    // B. Baca Sensor Turbidity (GPIO35)
    int turbidityRaw = analogRead(PIN_TURBIDITY);
    float turbidityVoltage = turbidityRaw * (VREF / ADC_RESOLUTION);
    float turbidityNtu = 0;
    if (turbidityVoltage >= 2.4) {
      turbidityNtu = 0.0;
    } else {
      turbidityNtu = (2.4 - turbidityVoltage) * (500.0 / 1.4);
    }
    if (turbidityNtu < 0) turbidityNtu = 0;

    // C. Jika Sedang Dalam Sesi Pengujian 30 Detik
    if (isTesting) {
      unsigned long elapsedTestTime = currentMillis - testStartTime;

      // Update Buffer & Inferensi Edge AI setiap 5 detik
      updateEdgeAiWindow(tdsPpm, turbidityNtu, elapsedTestTime);

      // Cek apakah durasi 30 detik sudah tercapai
      if (elapsedTestTime >= TEST_DURATION_MS) {
        isTesting = false;
        digitalWrite(PIN_LED_WHITE, HIGH); // LED Putih kembali menyala solid

        // Tentukan Hasil Akhir berdasarkan Voting Mayoritas keputusan 5-detik
        String finalVerdict = "Low";
        if (voteCountHigh >= voteCountMed && voteCountHigh >= voteCountLow && voteCountHigh > 0) {
          finalVerdict = "High";
        } else if (voteCountMed >= voteCountHigh && voteCountMed >= voteCountLow && voteCountMed > 0) {
          finalVerdict = "Medium";
        } else if (voteCountLow > 0) {
          finalVerdict = "Low";
        } else {
          finalVerdict = currentAiPrediction;
        }

        currentAiPrediction = finalVerdict;
        setClassificationLeds(finalVerdict); // Kunci tampilan LED hasil akhir

        Serial.println("\n========================================================");
        Serial.println(" [✓ SELESAI] PENGUJIAN 30 DETIK TELAH SELESAI!");
        Serial.print(" HASIL AKHIR (MAJORITY VOTE) : ");
        Serial.println(finalVerdict);
        Serial.print(" Rincian Voting 5-Detikan    : Low="); Serial.print(voteCountLow);
        Serial.print(" | Medium="); Serial.print(voteCountMed);
        Serial.print(" | High="); Serial.println(voteCountHigh);
        Serial.println(" Status LED Putih: MENYALA TERUS (STANDBY)");
        Serial.println(" Tekan Push Button untuk mengulang pengujian.");
        Serial.println("========================================================\n");
      }
    }

    // D. Stream Data JSON ke Laptop (Wi-Fi TCP)
    String jsonData = "{";
    jsonData += "\"sample_id\":" + String(sampleId) + ",";
    jsonData += "\"esp_timestamp_ms\":" + String(currentMillis) + ",";
    jsonData += "\"is_testing\":" + String(isTesting ? "true" : "false") + ",";
    jsonData += "\"tds_raw\":" + String(tdsRaw) + ",";
    jsonData += "\"tds_voltage\":" + String(tdsVoltage, 3) + ",";
    jsonData += "\"tds_ppm\":" + String(tdsPpm, 1) + ",";
    jsonData += "\"turbidity_raw\":" + String(turbidityRaw) + ",";
    jsonData += "\"turbidity_voltage\":" + String(turbidityVoltage, 3) + ",";
    jsonData += "\"turbidity_ntu\":" + String(turbidityNtu, 1) + ",";
    jsonData += "\"edge_ai_class\":\"" + currentAiPrediction + "\"";
    jsonData += "}\n";

    if (client && client.connected()) {
      client.print(jsonData);
    }

    // E. Monitor Nilai Sensor Realtime (Setiap 1 Detik saat Standby)
    if (!isTesting && (sampleId % 10 == 0)) {
      Serial.print("[STANDBY] TDS: "); Serial.print(tdsPpm, 1);
      Serial.print(" PPM | TURB: "); Serial.print(turbidityNtu, 1);
      Serial.println(" NTU (Tekan Push Button untuk Uji)");
    }
  }
}
