#include <Arduino.h>

/*
  =============================================================================
  ESP32 FULL EDGE AI WATER CLASSIFIER (WITH WI-FI CAPTIVE PORTAL PROVISIONING)
  =============================================================================
  Fitur Baru (Tanpa Hardcode Wi-Fi / Web Captive Portal):
  1. Boot Otomatis:
     - ESP32 membaca nama Wi-Fi & Password yang tersimpan di memori Flash (NVS).
     - Jika belum ada atau gagal konek, ESP32 otomatis meluncurkan Hotspot AP:
       * Nama Hotspot (SSID): "ESP32_WaterAI"
       * Password AP        : "12345678"
  2. Pop-up Captive Portal di HP:
     - Begitu HP/Laptop terhubung ke Wi-Fi "ESP32_WaterAI", otomatis muncul
       layar pop-up browser (Captive Portal) seperti Wi-Fi hotel/kafe!
     - Ada tombol "Pindai Wi-Fi", pilih nama Wi-Fi, masukkan password, lalu klik Simpan.
     - ESP32 otomatis menyimpan ke memori dan langsung terhubung ke Wi-Fi tersebut!
  3. Tetap Full Edge AI:
     - Tombol Uji 30 Detik (D32).
     - 4 LED Fisik (Merah D23, Kuning D22, Hijau D25, Status Biru D33).
     - Model Random Forest rf_model.h jalan 100% offline.
     - TCP Server di Port 8080 untuk streaming & injeksi data ke live_viewer.py.
  =============================================================================
*/

#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <DNSServer.h>
  #include <Preferences.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <DNSServer.h>
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
// CAPTIVE PORTAL & WI-FI PROVISIONING
// =============================================================================
const char *AP_SSID           = "ESP32_WaterAI";
const char *AP_PASSWORD       = "12345678"; // Password hotspot ESP32
const byte DNS_PORT           = 53;
const int SERVER_PORT         = 8080;

IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WebServer webServer(80);
WiFiServer server(SERVER_PORT);
WiFiClient client;
Preferences preferences;

bool isApModeActive           = false;
String savedSsid              = "";
String savedPassword          = "";

// Variabel Intervensi Nilai Sensor (TDS & Turbidity) via Wi-Fi
bool overrideSensorActive     = true;   // Aktifkan mode intervensi agar AI langsung jalan
float injectedTdsPpm          = 85.0;   // Nilai dasar TDS
float injectedTurbNtu         = 15.0;   // Nilai dasar Turbidity
String currentPresetName      = "LOW";  // Preset aktif

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

// Kondisi normal tombol saat tidak ditekan (auto-learned saat boot)
int buttonIdleState = HIGH; 

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
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
  }
}

// =============================================================================
// FUNGSI CEK PUSH BUTTON (AUTO-LEARNING IDLE STATE & ANTI-RESET TRIGGER)
// =============================================================================
bool checkButtonPressed() {
  if (millis() < 2000) {
    return false; // Abaikan transien daya 2 detik pertama
  }

  static int lastStableState = -1;
  static int lastReadState = -1;
  static unsigned long lastDebounceTime = 0;
  static bool hasBeenReleased = true;
  bool pressed = false;

  if (lastStableState == -1) {
    lastStableState = digitalRead(PIN_BUTTON);
    lastReadState = lastStableState;
    buttonIdleState = lastStableState;
    return false;
  }

  int reading = digitalRead(PIN_BUTTON);

  if (reading != lastReadState) {
    lastDebounceTime = millis();
    lastReadState = reading;
  }

  if ((millis() - lastDebounceTime) >= 30) {
    if (reading != lastStableState) {
      lastStableState = reading;
      if (lastStableState != buttonIdleState) {
        if (hasBeenReleased) {
          pressed = true;
          hasBeenReleased = false;
        }
      } else {
        hasBeenReleased = true;
      }
    }
  }

  return pressed;
}

// =============================================================================
// FUNGSI HTML & CAPTIVE PORTAL WEB SERVER
// =============================================================================
void handlePortalRoot() {
  // Pindai daftar Wi-Fi yang tersedia
  int n = WiFi.scanNetworks();
  String wifiOptions = "";
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    wifiOptions += "<option value='" + ssid + "'>" + ssid + " (" + String(rssi) + " dBm)</option>";
  }

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Water AI Setup</title>";
  html += "<style>";
  html += "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; justify-content: center; }";
  html += ".card { background: #1e293b; border: 1px solid #334155; border-radius: 16px; padding: 24px; max-width: 400px; width: 100%; box-shadow: 0 10px 25px -5px rgba(0,0,0,0.5); }";
  html += "h2 { color: #38bdf8; margin-top: 0; text-align: center; }";
  html += "p { color: #94a3b8; font-size: 14px; text-align: center; }";
  html += "label { font-size: 13px; font-weight: 600; color: #cbd5e1; display: block; margin-top: 14px; }";
  html += "select, input[type='text'], input[type='password'] { width: 100%; padding: 12px; margin-top: 6px; border: 1px solid #475569; border-radius: 8px; background: #0f172a; color: #fff; box-sizing: border-box; font-size: 14px; }";
  html += "input[type='submit'] { width: 100%; background: #0284c7; color: white; border: none; padding: 14px; border-radius: 8px; font-weight: 700; font-size: 15px; margin-top: 24px; cursor: pointer; transition: 0.2s; }";
  html += "input[type='submit']:hover { background: #0369a1; }";
  html += ".badge { display: inline-block; background: #0369a1; padding: 4px 10px; border-radius: 20px; font-size: 11px; font-weight: bold; }";
  html += "</style></head><body>";
  html += "<div class='card'>";
  html += "<h2>🌊 Water AI Setup</h2>";
  html += "<p>Hubungkan ESP32 ke jaringan Wi-Fi untuk mengaktifkan telemetri & live viewer laptop.</p>";
  html += "<form method='POST' action='/save'>";
  
  html += "<label>PILIH WI-FI TERSEDIA:</label>";
  html += "<select name='ssid' id='ssid_select' onchange='document.getElementById(\"custom_ssid\").value = this.value;'>";
  html += "<option value=''>-- Pilih Jaringan Wi-Fi --</option>";
  html += wifiOptions;
  html += "</select>";

  html += "<label>ATAU KETIK SSID MANUAL:</label>";
  html += "<input type='text' id='custom_ssid' name='custom_ssid' placeholder='Nama Wi-Fi...'>";

  html += "<label>PASSWORD WI-FI:</label>";
  html += "<input type='password' name='password' placeholder='Masukkan password Wi-Fi...'>";

  html += "<input type='submit' value='SIMPAN & SAMBUNGKAN'>";
  html += "</form>";
  html += "<div style='text-align:center; margin-top:16px;'><span class='badge'>Edge AI Offline Ready</span></div>";
  html += "</div></body></html>";

  webServer.send(200, "text/html", html);
}

void handlePortalSave() {
  String reqSsid = webServer.arg("custom_ssid");
  if (reqSsid == "") reqSsid = webServer.arg("ssid");
  String reqPass = webServer.arg("password");

  if (reqSsid != "") {
    preferences.begin("wifi_cfg", false);
    preferences.putString("ssid", reqSsid);
    preferences.putString("pass", reqPass);
    preferences.end();

    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;background:#0f172a;color:#fff;text-align:center;padding:50px 20px;}";
    html += ".card{background:#1e293b;border-radius:16px;padding:30px;max-width:380px;margin:auto;}h2{color:#38bdf8;}</style></head><body>";
    html += "<div class='card'><h2>✓ PENGATURAN DISIMPAN!</h2>";
    html += "<p>ESP32 sedang menghubungkan ke:</p>";
    html += "<h3>" + reqSsid + "</h3>";
    html += "<p style='color:#94a3b8;font-size:13px;'>Silakan sambungkan kembali HP/Laptop Anda ke Wi-Fi tersebut.</p></div></body></html>";
    webServer.send(200, "text/html", html);

    delay(2000);
    ESP.restart(); // Restart agar langsung terhubung ke Wi-Fi baru dengan bersih!
  } else {
    webServer.send(200, "text/html", "SSID Kosong! Silakan kembali dan pilih Wi-Fi.");
  }
}

// Redirect otomatis ke Captive Portal untuk Android/iOS/Windows
void handleCaptiveRedirect() {
  webServer.sendHeader("Location", String("http://192.168.4.1/"), true);
  webServer.send(302, "text/plain", "");
}

void startCaptivePortal() {
  isApModeActive = true;
  Serial.println("\n========================================================");
  Serial.println(" [!] MEMULAI CAPTIVE PORTAL SETUP (ACCESS POINT)");
  Serial.print(" >>> NAMA WI-FI (SSID) : "); Serial.println(AP_SSID);
  Serial.print(" >>> PASSWORD          : "); Serial.println(AP_PASSWORD);
  Serial.print(" >>> IP PORTAL SETUP   : "); Serial.println(apIP);
  Serial.println(" Sambungkan HP ke Wi-Fi tersebut untuk setup Wi-Fi baru!");
  Serial.println("========================================================\n");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  // Jalankan DNS Server untuk membajak request agar muncul pop-up
  dnsServer.start(DNS_PORT, "*", apIP);

  // Web Portal Handlers
  webServer.on("/", handlePortalRoot);
  webServer.on("/save", HTTP_POST, handlePortalSave);
  
  // Handlers untuk trigger pop-up otomatis di Android, iOS, Windows
  webServer.on("/generate_204", handlePortalRoot);
  webServer.on("/gen_204", handlePortalRoot);
  webServer.on("/hotspot-detect.html", handlePortalRoot);
  webServer.on("/ncsi.txt", handlePortalRoot);
  webServer.onNotFound(handleCaptiveRedirect);

  webServer.begin();
}

// =============================================================================
// FUNGSI INFERENSI EDGE AI (SETIAP 5 DETIK / 50 SAMPEL)
// =============================================================================
void updateEdgeAiWindow(float tds, float turb, unsigned long currentTestElapsed) {
  tdsBuffer[bufferIndex] = tds;
  turbBuffer[bufferIndex] = turb;
  bufferIndex = (bufferIndex + 1) % WINDOW_SIZE;
  if (bufferCount < WINDOW_SIZE) bufferCount++;

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

    float tdsVar = 0, turbVar = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
      tdsVar += (tdsBuffer[i] - tdsMean) * (tdsBuffer[i] - tdsMean);
      turbVar += (turbBuffer[i] - turbMean) * (turbBuffer[i] - turbMean);
    }
    float tdsStd = sqrt(tdsVar / WINDOW_SIZE);
    float turbStd = sqrt(turbVar / WINDOW_SIZE);

    float features[8] = {
      tdsMean, tdsStd, tdsMin, tdsMax,
      turbMean, turbStd, turbMin, turbMax
    };

    currentAiPrediction = EdgeWaterClassifier::predictLabel(features);
    totalDecisions++;

    if (currentAiPrediction == "Low") voteCountLow++;
    else if (currentAiPrediction == "Medium") voteCountMed++;
    else if (currentAiPrediction == "High") voteCountHigh++;

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
  Serial.println("   ESP32 EDGE AI CLASSIFIER (CAPTIVE PORTAL ENABLED)");
  Serial.println("========================================================");

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_STATUS, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  digitalWrite(PIN_LED_STATUS, HIGH); // LED Status (Biru) Solid ON
  setClassificationLeds("");          // LED Hasil padam

  #if defined(ESP32)
    analogSetAttenuation(ADC_11db); // ADC Range penuh 0 - 3.3V
    pinMode(PIN_TDS, INPUT);
    pinMode(PIN_TURBIDITY, INPUT);
  #endif

  // 1. Baca Konfigurasi Wi-Fi dari Memori Flash NVS (Non-Volatile Storage)
  preferences.begin("wifi_cfg", false);
  savedSsid = preferences.getString("ssid", ""); // Default kosong (TIDAK ADA HARDCODE)
  savedPassword = preferences.getString("pass", "");
  preferences.end();

  // 2. Fitur Reset Wi-Fi: Jika Tombol Push Button (D32) DITAHAN saat ESP32 dinyalakan (> 2 detik),
  //    otomatis hapus Wi-Fi lama dan buka Captive Portal Hotspot AP!
  if (digitalRead(PIN_BUTTON) == LOW) {
    Serial.println("\n[!] Tombol terdeteksi ditekan saat power-up...");
    int holdCount = 0;
    while (digitalRead(PIN_BUTTON) == LOW && holdCount < 30) {
      delay(100);
      holdCount++;
      digitalWrite(PIN_LED_STATUS, (holdCount % 2 == 0) ? HIGH : LOW);
    }
    if (holdCount >= 15) { // Ditahan > 1.5 detik
      Serial.println("\n========================================================");
      Serial.println(" [RESET] MENGHAPUS WI-FI TERSIMPAN! (FACTORY RESET)");
      Serial.println("========================================================");
      preferences.begin("wifi_cfg", false);
      preferences.clear();
      preferences.end();
      savedSsid = "";
      savedPassword = "";
      for (int i = 0; i < 6; i++) {
        digitalWrite(PIN_LED_STATUS, HIGH); delay(80);
        digitalWrite(PIN_LED_STATUS, LOW); delay(80);
      }
      digitalWrite(PIN_LED_STATUS, HIGH);
    }
  }

  // 3. Logika Koneksi:
  //    A. Jika belum pernah setup Wi-Fi (savedSsid kosong) -> Langsung luncurkan Hotspot AP!
  if (savedSsid == "") {
    Serial.println("\n[WIFI] Belum ada Wi-Fi yang dikonfigurasi.");
    startCaptivePortal();
  } 
  //    B. Jika sudah pernah disimpan -> Coba konek ke Wi-Fi router tersebut
  else {
    Serial.print("[WIFI] Menghubungkan ke Wi-Fi tersimpan: '");
    Serial.print(savedSsid);
    Serial.print("'");

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 16) { // Tunggu ~8 detik
      delay(500);
      Serial.print(".");
      retry++;
    }

    // Jika berhasil konek
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n========================================================");
      Serial.println(" [OK] BERHASIL TERHUBUNG KE WI-FI!");
      Serial.print(" >>> ALAMAT IP ESP32 : "); Serial.println(WiFi.localIP());
      Serial.println(" >>> PORT TCP        : 8080");
      Serial.println("========================================================\n");
      server.begin();
    } 
    // Jika gagal terhubung (misal router mati/ganti password) -> Luncurkan Hotspot AP!
    else {
      Serial.println("\n[!] Gagal terhubung ke Wi-Fi tersimpan.");
      startCaptivePortal();
    }
  }

  Serial.println(">>> SISTEM SIAP! Preset: LOW. Tekan Push Button (D32) untuk Mulai Uji 30s! <<<\n");
}

// =============================================================================
// LOOP UTAMA
// =============================================================================
void loop() {
  unsigned long currentMillis = millis();

  // A. Jika Sedang Dalam Mode Captive Portal Hotspot AP
  if (isApModeActive) {
    dnsServer.processNextRequest();
    webServer.handleClient();
  } 
  // B. Jika Dalam Mode Wi-Fi Normal (STA)
  else {
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
      else if (cmd == "OPEN_PORTAL") {
        // Perintah manual untuk membuka kembali captive portal
        startCaptivePortal();
      }
      else if (cmd == "RESET_SENSOR") {
        overrideSensorActive = false;
        currentPresetName = "HARDWARE";
        Serial.println("\n[SENSOR ASLI] Kembali ke pembacaan pin fisik.");
      }
    }
  }

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
    setClassificationLeds("");

    Serial.println("\n========================================================");
    Serial.println(" [RUN] TOMBOL DITEKAN! PENGUJIAN 30 DETIK DIMULAI!");
    Serial.print(" Preset Aktif: "); Serial.println(currentPresetName);
    Serial.println(" LED Status (Biru D33) Berkedip Cepat (Proses Edge AI)...");
    Serial.println("========================================================");
  }

  // 3. Kontrol Indikator LED Status (Biru D33)
  if (isTesting) {
    // Saat Pengujian AI Berlangsung: Berkedip Cepat (150ms)
    if (currentMillis - lastStatusBlink >= BLINK_INTERVAL_MS) {
      lastStatusBlink = currentMillis;
      statusLedState = !statusLedState;
      digitalWrite(PIN_LED_STATUS, statusLedState ? HIGH : LOW);
    }
  } else if (isApModeActive) {
    // Mode AP Hotspot Captive Portal: Kedip Santai (500ms) Menandakan Menunggu Setup HP
    if (currentMillis - lastStatusBlink >= 500) {
      lastStatusBlink = currentMillis;
      statusLedState = !statusLedState;
      digitalWrite(PIN_LED_STATUS, statusLedState ? HIGH : LOW);
    }
  } else {
    // Standby Siap Uji (Wi-Fi Normal): Solid ON
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

    if (isTesting) {
      unsigned long elapsedTestTime = currentMillis - testStartTime;
      updateEdgeAiWindow(tdsPpm, turbidityNtu, elapsedTestTime);

      if (elapsedTestTime >= TEST_DURATION_MS) {
        isTesting = false;
        digitalWrite(PIN_LED_STATUS, HIGH);

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

    // Stream JSON ke Client Laptop
    if (!isApModeActive && client && client.connected()) {
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

    // Serial monitor
    if (!isTesting && (sampleId % 10 == 0)) {
      if (isApModeActive) {
        Serial.println("[HOTSPOT AP SETUP AKTIF] Sambungkan HP ke 'ESP32_WaterAI' (PW: 12345678)!");
      } else {
        Serial.print("[STANDBY - "); Serial.print(currentPresetName); Serial.print("] ");
        Serial.print("TDS: "); Serial.print(tdsPpm, 1); Serial.print(" PPM | ");
        Serial.print("Turb: "); Serial.print(turbidityNtu, 1); Serial.print(" NTU | ");
        Serial.println("Siap Uji 30s!");
      }
    }
  }
}
