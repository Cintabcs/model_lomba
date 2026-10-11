/*
  =============================================================================
  ESP32 / ESP8266 DATA ACQUISITION FIRMWARE (10 Hz)
  =============================================================================
  MODE KONEKSI:
  Mode 1 (STA Mode - DIREKOMENDASIKAN):
  - ESP32 & Laptop SAMA-SAMA terhubung ke Wi-Fi Rumah / Hotspot HP Anda.
  - Laptop TETAP BISA INTERNETAN untuk ngerjain tugas!
  
  Mode 2 (AP Mode):
  - ESP32 membuat Hotspot sendiri (Tanpa Internet).
  =============================================================================
*/

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #error "Board tidak didukung! Gunakan ESP32 atau ESP8266."
#endif

// =============================================================================
// 1. PILIH MODE WIFI
// Set true  = Connect ke Wi-Fi Rumah (Laptop Tetap Bisa Internetan!)
// Set false = ESP buat Hotspot AP sendiri
// =============================================================================
#define USE_HOME_WIFI true  

#if USE_HOME_WIFI
  // Wi-Fi Rumah Anda:
  const char *wifi_ssid     = "MyRepublic_C2222847";
  const char *wifi_password = "C2222847"; // <-- MASUKKAN PASSWORD WIFI DI SINI
#else
  // Setting jika ESP buat Hotspot sendiri:
  const char *wifi_ssid     = "ESP_DA_Logger";
  const char *wifi_password = "12345678";
#endif

// Server Configuration Port
const int SERVER_PORT = 8080;
WiFiServer server(SERVER_PORT);
WiFiClient client;

// Sampling Rate Settings (10 Hz = 100 ms)
const unsigned long SAMPLE_INTERVAL_MS = 100; 
unsigned long lastSampleTime = 0;
unsigned long sampleId = 0;

// Konfigurasi Pin Sensor (ESP32)
#if defined(ESP32)
  const int PIN_TDS = 34;       // Analog Pin Sensor TDS (GPIO34)
  const int PIN_TURBIDITY = 35; // Analog Pin Sensor Turbidity (GPIO35)
  const float VREF = 3.3;       // Tegangan Referensi ESP32 (3.3V)
  const float ADC_RESOLUTION = 4095.0; // 12-bit ADC
#else
  const int PIN_TDS = A0;
  const int PIN_TURBIDITY = A0;
  const float VREF = 3.3;
  const float ADC_RESOLUTION = 1023.0;
#endif

float defaultTemperature = 25.0; // Suhu default air (°C)

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 WATER SENSOR DATA LOGGER (10 Hz) ---");

#if USE_HOME_WIFI
  // CONNECT KE WIFI RUMAH (STA MODE)
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
    Serial.println("\n[✓] BERHASIL Terhubung ke Wi-Fi Rumah!");
    Serial.print("[!] MASUKKAN IP INI DI PYTHON (config.py / menu): ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[!] Gagal terhubung ke Wi-Fi Rumah! Beralih ke Hotspot AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP_DA_Logger", "12345678");
    Serial.print("IP Hotspot ESP: ");
    Serial.println(WiFi.softAPIP());
  }

#else
  // HOTSPOT AP MODE
  WiFi.mode(WIFI_AP);
  WiFi.softAP(wifi_ssid, wifi_password);
  Serial.print("Hotspot AP ESP Aktif! IP ESP: ");
  Serial.println(WiFi.softAPIP());
#endif

  // Start Server Socket
  server.begin();
  Serial.print("TCP Server berjalan di port: ");
  Serial.println(SERVER_PORT);

#if defined(ESP32)
  analogSetAttenuation(ADC_11db); // Set ADC Full Scale Range to 0 - 3.3V
  pinMode(PIN_TDS, INPUT);
  pinMode(PIN_TURBIDITY, INPUT);
#endif

  Serial.println("Siap menerima koneksi dari Laptop...");
}

void loop() {
  // Cek client baru (Laptop)
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("\n[+] Laptop terhubung!");
    }
  }

  // Timer 10 Hz (100 ms)
  unsigned long currentMillis = millis();
  if (currentMillis - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = currentMillis;
    sampleId++;

    // 1. Baca Sensor TDS
    int tdsRaw = analogRead(PIN_TDS);
    float tdsVoltage = tdsRaw * (VREF / ADC_RESOLUTION);
    float compCoeff = 1.0 + 0.02 * (defaultTemperature - 25.0);
    float compVolt = tdsVoltage / compCoeff;
    float tdsPpm = (133.42 * compVolt * compVolt * compVolt 
                    - 255.86 * compVolt * compVolt 
                    + 857.39 * compVolt) * 0.5;
    if (tdsPpm < 0) tdsPpm = 0;

    // 2. Baca Sensor Turbidity
    int turbidityRaw = analogRead(PIN_TURBIDITY);
    float turbidityVoltage = turbidityRaw * (VREF / ADC_RESOLUTION);
    float turbidityNtu = 0;
    
    // Kalibrasi Custom Khusus Sensor Anda (Berdasarkan data log CSV):
    // Air Jernih (Low) -> Tegangan sekitar 2.38V - 2.4V
    // Air Kotor (Medium) -> Tegangan turun ke 1.83V
    
    if (turbidityVoltage >= 2.4) {
      turbidityNtu = 0.0;
    } else {
      // Skala diperkecil! Biar angkanya nggak terlalu gede.
      // 2.4V = 0 NTU
      // 1.0V = 500 NTU (sebelumnya 3000)
      turbidityNtu = (2.4 - turbidityVoltage) * (500.0 / (2.4 - 1.0));
    }

    if (turbidityNtu < 0) turbidityNtu = 0;

    // 3. JSON Stream Data
    String jsonData = "{";
    jsonData += "\"sample_id\":" + String(sampleId) + ",";
    jsonData += "\"esp_timestamp_ms\":" + String(currentMillis) + ",";
    jsonData += "\"tds_raw\":" + String(tdsRaw) + ",";
    jsonData += "\"tds_voltage\":" + String(tdsVoltage, 3) + ",";
    jsonData += "\"tds_ppm\":" + String(tdsPpm, 1) + ",";
    jsonData += "\"turbidity_raw\":" + String(turbidityRaw) + ",";
    jsonData += "\"turbidity_voltage\":" + String(turbidityVoltage, 3) + ",";
    jsonData += "\"turbidity_ntu\":" + String(turbidityNtu, 1);
    jsonData += "}\n";

    // 4. Kirim ke Laptop
    if (client && client.connected()) {
      client.print(jsonData);
    }

    // Monitor via Serial
    if (sampleId % 10 == 0) {
      Serial.print("Sampel #");
      Serial.print(sampleId);
      Serial.print(" | TDS: ");
      Serial.print(tdsPpm, 1);
      Serial.print(" PPM | Turbidity: ");
      Serial.print(turbidityNtu, 1);
      Serial.println(" NTU");
    }
  }
}
