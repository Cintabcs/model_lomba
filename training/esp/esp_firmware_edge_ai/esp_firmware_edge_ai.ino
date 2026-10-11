/*
  =============================================================================
  ESP32 EDGE AI WATER QUALITY CLASSIFIER FIRMWARE (5-SECOND WINDOW)
  =============================================================================
  - Menjalankan klasifikasi Offline Random Forest di mikroprosesor ESP32.
  - Membaca sensor TDS (GPIO34) & Turbidity (GPIO35) pada 10 Hz (100 ms).
  - Mengelompokkan data per 5 detik (50 sampel) ke dalam Ring Buffer.
  - Menghitung 8 Fitur Statistik (Mean, Std, Min, Max).
  - Melakukan inferensi Edge AI mandiri (< 1 ms).
  =============================================================================
*/

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #error "Board tidak didukung! Gunakan ESP32 atau ESP8266."
#endif

#include "rf_model.h" // Edge AI Model (Random Forest 5-Second Window)

// =============================================================================
// EDGE AI RING BUFFER (5 Detik = 50 Sampel pada 10 Hz)
// =============================================================================
const int WINDOW_SIZE = 50;
float tdsBuffer[WINDOW_SIZE];
float turbBuffer[WINDOW_SIZE];
int bufferIndex = 0;
int bufferCount = 0;
String currentAiPrediction = "Initializing...";

void updateEdgeAiWindow(float tds, float turb) {
  tdsBuffer[bufferIndex] = tds;
  turbBuffer[bufferIndex] = turb;
  bufferIndex = (bufferIndex + 1) % WINDOW_SIZE;
  if (bufferCount < WINDOW_SIZE) bufferCount++;

  // Jalankan klasifikasi Edge AI setiap kali buffer terisi 50 sampel (5 detik)
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

    // Array 8 Fitur untuk Random Forest Edge AI
    float features[8] = {
      tdsMean, tdsStd, tdsMin, tdsMax,
      turbMean, turbStd, turbMin, turbMax
    };

    // Inferensi Edge AI (< 1 ms di ESP32)
    currentAiPrediction = EdgeWaterClassifier::predictLabel(features);

    Serial.println("\n=================================================================");
    Serial.print(" [🤖 EDGE AI INFERENCE (5s Window)] => KLASIFIKASI: ");
    Serial.println(currentAiPrediction);
    Serial.print(" 📊 TDS  : Mean="); Serial.print(tdsMean, 1);
    Serial.print(" PPM | Min="); Serial.print(tdsMin, 1);
    Serial.print(" | Max="); Serial.print(tdsMax, 1);
    Serial.print(" | Std="); Serial.println(tdsStd, 1);
    Serial.print(" 📊 TURB : Mean="); Serial.print(turbMean, 1);
    Serial.print(" NTU | Min="); Serial.print(turbMin, 1);
    Serial.print(" | Max="); Serial.print(turbMax, 1);
    Serial.print(" | Std="); Serial.println(turbStd, 1);
    Serial.println("=================================================================\n");
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

const unsigned long SAMPLE_INTERVAL_MS = 100; // 10 Hz
unsigned long lastSampleTime = 0;
unsigned long sampleId = 0;

#if defined(ESP32)
  const int PIN_TDS = 34;       
  const int PIN_TURBIDITY = 35; 
  const float VREF = 3.3;       
  const float ADC_RESOLUTION = 4095.0;
#else
  const int PIN_TDS = A0;
  const int PIN_TURBIDITY = A0;
  const float VREF = 3.3;
  const float ADC_RESOLUTION = 1023.0;
#endif

float defaultTemperature = 25.0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 EDGE AI WATER CLASSIFIER (5s WINDOW) ---");

#if USE_HOME_WIFI
  Serial.print("Connecting ke Wi-Fi Rumah: ");
  Serial.println(wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[✓] BERHASIL Terhubung ke Wi-Fi!");
    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[!] Gagal ke Wi-Fi, beralih ke AP Mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP_DA_Logger", "12345678");
  }
#else
  WiFi.mode(WIFI_AP);
  WiFi.softAP(wifi_ssid, wifi_password);
#endif

  server.begin();
  Serial.println("TCP Server siap...");

#if defined(ESP32)
  analogSetAttenuation(ADC_11db); // Set ADC Full Scale Range to 0 - 3.3V
  pinMode(PIN_TDS, INPUT);
  pinMode(PIN_TURBIDITY, INPUT);
#endif
}

void loop() {
  if (!client || !client.connected()) {
    client = server.available();
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = currentMillis;
    sampleId++;

    // 1. Baca TDS
    int tdsRaw = analogRead(PIN_TDS);
    float tdsVoltage = tdsRaw * (VREF / ADC_RESOLUTION);
    float compCoeff = 1.0 + 0.02 * (defaultTemperature - 25.0);
    float compVolt = tdsVoltage / compCoeff;
    float tdsPpm = (133.42 * compVolt * compVolt * compVolt 
                    - 255.86 * compVolt * compVolt 
                    + 857.39 * compVolt) * 0.5;
    if (tdsPpm < 0) tdsPpm = 0;

    // 2. Baca Turbidity
    int turbidityRaw = analogRead(PIN_TURBIDITY);
    float turbidityVoltage = turbidityRaw * (VREF / ADC_RESOLUTION);
    float turbidityNtu = 0;
    if (turbidityVoltage >= 2.4) {
      turbidityNtu = 0.0;
    } else {
      turbidityNtu = (2.4 - turbidityVoltage) * (500.0 / 1.4);
    }
    if (turbidityNtu < 0) turbidityNtu = 0;

    // 3. Update Buffer & Inferensi Edge AI (5 Detik)
    updateEdgeAiWindow(tdsPpm, turbidityNtu);

    // 4. JSON Stream dengan hasil Edge AI Class
    String jsonData = "{";
    jsonData += "\"sample_id\":" + String(sampleId) + ",";
    jsonData += "\"esp_timestamp_ms\":" + String(currentMillis) + ",";
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

    // Monitor Pembacaan Sensor Realtime & Raw ADC (Setiap 1 Detik / 10 Sampel)
    if (sampleId % 10 == 0) {
      Serial.print("Sampel #"); Serial.print(sampleId);
      Serial.print(" | TDS: Raw="); Serial.print(tdsRaw);
      Serial.print(" ("); Serial.print(tdsVoltage, 2); Serial.print("V -> ");
      Serial.print(tdsPpm, 1); Serial.print(" PPM)");
      Serial.print(" | TURB: Raw="); Serial.print(turbidityRaw);
      Serial.print(" ("); Serial.print(turbidityVoltage, 2); Serial.print("V -> ");
      Serial.print(turbidityNtu, 1); Serial.println(" NTU)");
    }
  }
}
