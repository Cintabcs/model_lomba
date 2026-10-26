#include <Arduino.h>

/*
  =============================================================================
  ESP32 FULL EDGE AI WATER CLASSIFIER (WITH COMPLETE PRESET & SENSOR SIMULATOR)
  =============================================================================
  Pinout sesuai jalur PCB:
  - 1x LED Status (Biru) : GPIO 33 (D33)
      * Standby / Siap   : MENYALA TERUS (Solid ON).
      * Sedang Proses Uji: KEDAP-KEDIP (Blinking 150 ms) selama 30 detik.
  - 3x LED Hasil Klasifikasi:
      * Hijau            : GPIO 25 (D25) -> Low (Air Bersih / PDAM)
      * Kuning           : GPIO 22 (D22) -> Medium (Air Sedang)
      * Merah            : GPIO 23 (D23) -> High (Air Kotor / Keruh)
  - 1x Push Button       : GPIO 32 (D32) -> Internal INPUT_PULLUP (kaki lain ke GND)
  - 2x Sensor Analog     : TDS (GPIO 34) & Turbidity (GPIO 35)

  Fitur HIL Simulator (Intervensi Penuh Low / Medium / High):
  - Karena sensor fisik sedang rusak/bermasalah, sistem dapat diinjeksi preset
    kategori langsung dari laptop via Wi-Fi (SET_PRESET:LOW / MEDIUM / HIGH).
  - ESP32 menyimulasikan data TDS & Turbidity realistis lengkap dengan mikrovibrasi.
  - Model Random Forest di ESP32 tetap mengolah 8 fitur statistik nyata.
  - LED Merah, Kuning, Hijau di PCB tetap menyala asli dari hasil inferensi AI!
  =============================================================================
*/

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#include "rf_model.h" // Model Random Forest Edge AI (5-Second Window)

// =============================================================================
// KONFIGURASI PIN PCB FISIK
// =============================================================================
#if defined(ESP32)
  const int PIN_TDS           = 34; // Sinyal Analog TDS (GPIO 34 / ADC1)
  const int PIN_TURBIDITY     = 35; // Sinyal Analog Turbidity (GPIO 35 / ADC1)

  // Indikator LED Klasifikasi Hasil
  const int PIN_LED_RED       = 23; // LED Merah  -> High (Air Kotor / Keruh) (D23)
  const int PIN_LED_YELLOW    = 22; // LED Kuning -> Medium (Air Sedang)      (D22)
  const int PIN_LED_GREEN     = 25; // LED Hijau  -> Low (Air Bersih)         (D25)

  // Indikator Status & Tombol Trigger
  const int PIN_LED_STATUS    = 33; // LED Status (Biru) -> Standby Solid ON / Blink saat Proses (D33)
  const int PIN_BUTTON        = 32; // Push Button (D32, INPUT_PULLUP ke GND)

  const float VREF            = 3.3;       
  const float ADC_RESOLUTION  = 4095.0; // 12-bit ADC ESP32
#else
  const int PIN_TDS           = A0;
  const int PIN_TURBIDITY     = A0;
  const int PIN_LED_GREEN     = D1;
  const int PIN_LED_YELLOW    = D2;
  const int PIN_LED_RED       = D3;
  const int PIN_LED_STATUS    = D4;
  const int PIN_BUTTON        = D5;
  const float VREF            = 3.3;
  const float ADC_RESOLUTION  = 1023.0;
#endif

// =============================================================================
// KONEKSI WI-FI BACKGROUND & TCP SERVER (PORT 8080)
// =============================================================================
#define ENABLE_BACKGROUND_WIFI true  

const char *WIFI_SSID     = "MyRepublic_C2222847";
const char *WIFI_PASSWORD = "C2222847";
const int SERVER_PORT     = 8080;

WiFiServer server(SERVER_PORT);
WiFiClient client;

// Variabel Intervensi Nilai Sensor (TDS & Turbidity) via Wi-Fi
bool overrideSensorActive = true;       // Aktifkan mode intervensi agar AI langsung jalan!
float injectedTdsPpm      = 85.0;       // Nilai dasar TDS
float injectedTurbNtu     = 15.0;       // Nilai dasar Turbidity
String currentPresetName  = "LOW";      // Preset aktif

// =============================================================================
// VARIABEL PENGUJIAN 30 DETIK & BUFFER EDGE AI
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

// Timer Blinking LED Status (Non-blocking)
unsigned long lastStatusBlink = 0;
bool statusLedState = HIGH;
const unsigned long BLINK_INTERVAL_MS = 150; // Kedip cepat 150 ms saat proses

// Voting Keputusan 5-detikan selama 30 detik
int voteCountLow = 0;
int voteCountMed = 0;
int voteCountHigh = 0;
int totalDecisions = 0;

// Sampling Sensor (10 Hz = 100 ms)
const unsigned long SAMPLE_INTERVAL_MS = 100;
unsigned long lastSampleTime = 0;
unsigned long sampleId = 0;
float defaultTemperature = 25.0; // Suhu default air (°C)

// =============================================================================
// FUNGSI KENDALI LED HASIL KLASIFIKASI
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
    // Matikan semua LED hasil saat standby atau reset awal
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
  }
}

// Kondisi normal tombol saat tidak ditekan (akan dipelajari otomatis saat boot)
int buttonIdleState = HIGH; 

// =============================================================================
// FUNGSI CEK PUSH BUTTON (AUTO-LEARNING IDLE STATE & ANTI-RESET TRIGGER)
// =============================================================================
bool checkButtonPressed() {
  // 1. Abaikan transien daya saat 2 detik pertama setelah dicolok kabel/baterai/reset
  if (millis() < 2000) {
    return false;
  }

  static int lastStableState = -1;
  static int lastReadState = -1;
  static unsigned long lastDebounceTime = 0;
  static bool hasBeenReleased = true;
  bool pressed = false;

  // Inisialisasi awal saat pertama kali dipanggil setelah 2 detik
  if (lastStableState == -1) {
    lastStableState = digitalRead(PIN_BUTTON);
    lastReadState = lastStableState;
    buttonIdleState = lastStableState; // Kunci kondisi diam saat boot
    return false;
  }

  int reading = digitalRead(PIN_BUTTON);

  // Jika kondisi pembacaan berubah (kemungkinan noise / getaran mekanis)
  if (reading != lastReadState) {
    lastDebounceTime = millis();
    lastReadState = reading;
  }

  // Jika kondisi baru sudah stabil selama 30 ms
  if ((millis() - lastDebounceTime) >= 30) {
    if (reading != lastStableState) {
      lastStableState = reading;

      // Trigger HANYA saat tombol ditekan (berubah dari posisi idle)
      if (lastStableState != buttonIdleState) {
        if (hasBeenReleased) {
          pressed = true;
          hasBeenReleased = false; // Kunci agar tidak trigger berulang kali
        }
      } else {
        // Tombol kembali dilepas ke posisi idle
        hasBeenReleased = true;
      }
    }
  }

  return pressed;
}

// =============================================================================
// FUNGSI INFERENSI EDGE AI (SETIAP 5 DETIK / 50 SAMPEL)
// =============================================================================
void updateEdgeAiWindow(float tds, float turb, unsigned long currentTestElapsed) {
  tdsBuffer[bufferIndex] = tds;
  turbBuffer[bufferIndex] = turb;
  bufferIndex = (bufferIndex + 1) % WINDOW_SIZE;
  if (bufferCount < WINDOW_SIZE) bufferCount++;

  // Jalankan klasifikasi Edge AI setiap kali terkumpul 50 sampel baru
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

    // Array 8 Fitur Statistik untuk Model Random Forest
    float features[8] = {
      tdsMean, tdsStd, tdsMin, tdsMax,
      turbMean, turbStd, turbMin, turbMax
    };

    // Inferensi Edge AI Offline (< 0.2 ms pada ESP32)
    currentAiPrediction = EdgeWaterClassifier::predictLabel(features);
    totalDecisions++;

    if (currentAiPrediction == "Low") voteCountLow++;
    else if (currentAiPrediction == "Medium") voteCountMed++;
    else if (currentAiPrediction == "High") voteCountHigh++;

    // Perbarui LED hasil sesuai keputusan sementara 5-detikan
    setClassificationLeds(currentAiPrediction);

    int elapsedSec = currentTestElapsed / 1000;
    int remainingSec = (int)((TEST_DURATION_MS - currentTestElapsed + 999) / 1000);
    if (remainingSec < 0) remainingSec = 0;

    Serial.println("\n-----------------------------------------------------------------");
    Serial.print(" [AI 5s] Waktu: "); Serial.print(elapsedSec);
    Serial.print("s / 30s (Sisa: "); Serial.print(remainingSec); Serial.println("s)");
    Serial.print(" => KELAS TERDETEKSI: "); Serial.println(currentAiPrediction);
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
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("");
  Serial.println("========================================================");
  Serial.println("   ESP32 FULL EDGE AI WATER CLASSIFIER (PRESET ENABLED)");
  Serial.println("========================================================");
  Serial.println(" Pinout PCB:");
  Serial.println("   [R]   LED Merah  : GPIO 23 (High / Keruh)");
  Serial.println("   [Y]   LED Kuning : GPIO 22 (Medium / Sedang)");
  Serial.println("   [G]   LED Hijau  : GPIO 25 (Low / Bersih)");
  Serial.println("   [STS] LED Status : GPIO 33 (Biru - Standby Solid / Blink)");
  Serial.println("   [BTN] Tombol Uji : GPIO 32 (INPUT_PULLUP)");
  Serial.println("   [TDS] Sensor TDS : GPIO 34 (ADC1)");
  Serial.println("   [TRB] Sensor Turb: GPIO 35 (ADC1)");
  Serial.println("--------------------------------------------------------");

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_STATUS, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Status Awal Standby
  digitalWrite(PIN_LED_STATUS, HIGH); // LED Status (Biru) Solid ON
  setClassificationLeds("");          // LED Hasil padam

  #if defined(ESP32)
    analogSetAttenuation(ADC_11db); // ADC Range penuh 0 - 3.3V
    pinMode(PIN_TDS, INPUT);
    pinMode(PIN_TURBIDITY, INPUT);
  #endif

  #if ENABLE_BACKGROUND_WIFI
    Serial.println("\n[WIFI] Menyiapkan modul Wi-Fi...");
    WiFi.disconnect(true); // Bersihkan sisa koneksi sebelumnya
    delay(200);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // Matikan power saving agar sinyal Wi-Fi stabil & responsif
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[WIFI] Menghubungkan ke '");
    Serial.print(WIFI_SSID);
    Serial.print("'");

    // Beri waktu 8 detik saat booting agar langsung terhubung
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 16) {
      delay(500);
      Serial.print(".");
      timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n========================================================");
      Serial.println(" [OK] BERHASIL TERHUBUNG KE WI-FI RUMAH!");
      Serial.print(" >>> ALAMAT IP ESP32 : "); Serial.println(WiFi.localIP());
      Serial.println(" >>> PORT TCP        : 8080");
      Serial.println(" (Gunakan IP di atas untuk live_viewer.py di laptop)");
      Serial.println("========================================================\n");
    } else {
      Serial.println("\n[!] Belum terhubung (akan terus mencoba di background).");
      Serial.println("    Sistem tetap dapat dijalankan secara OFFLINE!\n");
    }

    server.begin();
  #endif

  Serial.println(">>> SISTEM SIAP! Preset Awal: LOW (TDS 85 PPM, Turb 15 NTU).");
  Serial.println(">>> Tekan Push Button (D32) di PCB untuk mulai pengujian 30 detik! <<<\n");
}

// =============================================================================
// LOOP UTAMA
// =============================================================================
void loop() {
  unsigned long currentMillis = millis();

  // 1. Monitor & Auto-Reconnect Wi-Fi di Background
  #if ENABLE_BACKGROUND_WIFI
    static unsigned long lastWifiCheck = 0;
    static bool wifiAnnounced = false;

    if (currentMillis - lastWifiCheck >= 3000) {
      lastWifiCheck = currentMillis;
      if (WiFi.status() == WL_CONNECTED) {
        if (!wifiAnnounced) {
          wifiAnnounced = true;
          Serial.println("\n========================================================");
          Serial.println(" [OK] WI-FI TERHUBUNG!");
          Serial.print(" >>> ALAMAT IP ESP32 : "); Serial.println(WiFi.localIP());
          Serial.println(" >>> PORT TCP        : 8080");
          Serial.println("========================================================\n");
        }
      } else {
        wifiAnnounced = false;
        WiFi.reconnect();
      }
    }

    // Terima Client TCP dari Laptop
    if (server.hasClient()) {
      if (!client || !client.connected()) {
        client = server.available();
        Serial.println("\n[TCP] Client laptop (live_viewer.py) terhubung!");
      }
    }

    if (client && client.connected() && client.available()) {
      String cmd = client.readStringUntil('\n');
      cmd.trim();

      if (cmd == "SET_PRESET:LOW") {
        overrideSensorActive = true;
        injectedTdsPpm = 85.0;
        injectedTurbNtu = 15.0;
        currentPresetName = "LOW";
        Serial.println("\n[PRESET DIUBAH] -> LOW (TDS ~85 PPM, Turb ~15 NTU)");
      } 
      else if (cmd == "SET_PRESET:MEDIUM") {
        overrideSensorActive = true;
        injectedTdsPpm = 245.0;
        injectedTurbNtu = 215.0;
        currentPresetName = "MEDIUM";
        Serial.println("\n[PRESET DIUBAH] -> MEDIUM (TDS ~245 PPM, Turb ~215 NTU)");
      } 
      else if (cmd == "SET_PRESET:HIGH") {
        overrideSensorActive = true;
        injectedTdsPpm = 480.0;
        injectedTurbNtu = 450.0;
        currentPresetName = "HIGH";
        Serial.println("\n[PRESET DIUBAH] -> HIGH (TDS ~480 PPM, Turb ~450 NTU)");
      }
      else if (cmd.startsWith("SET_CUSTOM:")) {
        int commaIdx = cmd.indexOf(',');
        if (commaIdx > 11) {
          float cTds = cmd.substring(11, commaIdx).toFloat();
          float cTurb = cmd.substring(commaIdx + 1).toFloat();
          overrideSensorActive = true;
          injectedTdsPpm = cTds;
          injectedTurbNtu = cTurb;
          currentPresetName = "CUSTOM";
          Serial.print("\n[CUSTOM DIUBAH] -> TDS: "); Serial.print(injectedTdsPpm, 1);
          Serial.print(" | Turb: "); Serial.println(injectedTurbNtu, 1);
        }
      }
      else if (cmd == "RESET_SENSOR") {
        overrideSensorActive = false;
        currentPresetName = "HARDWARE";
        Serial.println("\n[SENSOR ASLI] Kembali ke pembacaan pin fisik.");
      }
    }
  #endif

  // 2. Baca Push Button (D32) untuk Memulai Pengujian 30 Detik
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
    Serial.println(" [RUN] TOMBOL DITEKAN! PENGUJIAN 30 DETIK DIMULAI!");
    Serial.print(" Preset Aktif: "); Serial.println(currentPresetName);
    Serial.println(" LED Status (Biru D33) Berkedip Cepat (Proses Edge AI)...");
    Serial.println("========================================================");
  }

  // 3. Kontrol Indikator LED Status (Biru D33)
  if (isTesting) {
    if (currentMillis - lastStatusBlink >= BLINK_INTERVAL_MS) {
      lastStatusBlink = currentMillis;
      statusLedState = !statusLedState;
      digitalWrite(PIN_LED_STATUS, statusLedState ? HIGH : LOW);
    }
  } else {
    digitalWrite(PIN_LED_STATUS, HIGH);
  }

  // 4. Siklus Sampling Sensor 10 Hz (Setiap 100 ms)
  if (currentMillis - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = currentMillis;
    sampleId++;

    int tdsRaw = 0;
    float tdsVoltage = 0.0;
    float tdsPpm = 0.0;
    int turbidityRaw = 0;
    float turbidityVoltage = 0.0;
    float turbidityNtu = 0.0;

    if (overrideSensorActive) {
      // Buat sedikit fluktuasi acak realistis agar statistik std deviasi nyata
      float jitterTds = (float)random(-15, 16) / 10.0;
      float jitterTurb = (float)random(-20, 21) / 10.0;
      tdsPpm = max(5.0f, injectedTdsPpm + jitterTds);
      turbidityNtu = max(0.0f, injectedTurbNtu + jitterTurb);
      tdsVoltage = (tdsPpm / 500.0) * 1.5;
      tdsRaw = (int)(tdsVoltage * (ADC_RESOLUTION / VREF));
      turbidityVoltage = max(0.2f, 2.4f - (turbidityNtu * 1.4f / 500.0f));
      turbidityRaw = (int)(turbidityVoltage * (ADC_RESOLUTION / VREF));
    } else {
      tdsRaw = analogRead(PIN_TDS);
      tdsVoltage = tdsRaw * (VREF / ADC_RESOLUTION);
      float compCoeff = 1.0 + 0.02 * (defaultTemperature - 25.0);
      float compVolt = tdsVoltage / compCoeff;
      tdsPpm = (133.42 * compVolt * compVolt * compVolt 
                      - 255.86 * compVolt * compVolt 
                      + 857.39 * compVolt) * 0.5;
      if (tdsPpm < 0) tdsPpm = 0;

      turbidityRaw = analogRead(PIN_TURBIDITY);
      turbidityVoltage = turbidityRaw * (VREF / ADC_RESOLUTION);
      if (turbidityVoltage >= 2.4) {
        turbidityNtu = 0.0;
      } else {
        turbidityNtu = (2.4 - turbidityVoltage) * (500.0 / 1.4);
      }
      if (turbidityNtu < 0) turbidityNtu = 0;
    }

    // C. Jika Sedang Dalam Sesi Pengujian 30 Detik
    if (isTesting) {
      unsigned long elapsedTestTime = currentMillis - testStartTime;

      // Update Ring Buffer & Jalankan Inferensi Edge AI setiap 5 detik
      updateEdgeAiWindow(tdsPpm, turbidityNtu, elapsedTestTime);

      // Cek apakah 30 detik sudah tercapai
      if (elapsedTestTime >= TEST_DURATION_MS) {
        isTesting = false;
        digitalWrite(PIN_LED_STATUS, HIGH); // LED Status (Biru) kembali solid ON

        // Tentukan Hasil Akhir berdasarkan Suara Terbanyak (Majority Vote)
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
        
        // KUNCI LED HASIL AKHIR AGAR TETAP MENYALA
        setClassificationLeds(finalVerdict);

        Serial.println("\n========================================================");
        Serial.println(" [OK] PENGUJIAN 30 DETIK SELESAI!");
        Serial.print(" HASIL AKHIR TERKUNCI (MAJORITY VOTE) : ");
        Serial.println(finalVerdict);
        Serial.print(" Rincian Voting 5-Detikan             : Low="); Serial.print(voteCountLow);
        Serial.print(" | Medium="); Serial.print(voteCountMed);
        Serial.print(" | High="); Serial.println(voteCountHigh);
        Serial.println(" Status LED: LED Status (Biru D33) Solid ON.");
        Serial.println(" LED Hasil Klasifikasi Tetap Menyala.");
        Serial.println(" Tekan Push Button untuk memulai pengujian baru.");
        Serial.println("========================================================\n");
      }
    }

    // D. Stream Data JSON ke Laptop via Wi-Fi (live_viewer.py)
    #if ENABLE_BACKGROUND_WIFI
      if (client && client.connected()) {
        int elapsedSec = isTesting ? (int)((currentMillis - testStartTime) / 1000) : 0;
        String jsonData = "{";
        jsonData += "\"sample_id\":" + String(sampleId) + ",";
        jsonData += "\"esp_timestamp_ms\":" + String(currentMillis) + ",";
        jsonData += "\"is_testing\":" + String(isTesting ? "true" : "false") + ",";
        jsonData += "\"test_elapsed_sec\":" + String(elapsedSec) + ",";
        jsonData += "\"preset\":\"" + currentPresetName + "\",";
        jsonData += "\"tds_raw\":" + String(tdsRaw) + ",";
        jsonData += "\"tds_voltage\":" + String(tdsVoltage, 3) + ",";
        jsonData += "\"tds_ppm\":" + String(tdsPpm, 1) + ",";
        jsonData += "\"turbidity_raw\":" + String(turbidityRaw) + ",";
        jsonData += "\"turbidity_voltage\":" + String(turbidityVoltage, 3) + ",";
        jsonData += "\"turbidity_ntu\":" + String(turbidityNtu, 1) + ",";
        jsonData += "\"edge_ai_class\":\"" + currentAiPrediction + "\"";
        jsonData += "}\n";
        client.print(jsonData);
      }
    #endif

    // E. Monitor Pembacaan Serial saat Standby (Setiap 1 Detik)
    if (!isTesting && (sampleId % 10 == 0)) {
      Serial.print("[STANDBY - "); Serial.print(currentPresetName); Serial.print("] ");
      Serial.print("TDS: "); Serial.print(tdsPpm, 1); Serial.print(" PPM | ");
      Serial.print("Turb: "); Serial.print(turbidityNtu, 1); Serial.print(" NTU | ");
      Serial.println("Siap Uji 30s!");
    }
  }
}
