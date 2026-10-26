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
  // Pindai daftar Wi-Fi 2.4 GHz sekitar
  int n = WiFi.scanNetworks();
  String wifiListHtml = "";

  if (n <= 0) {
    wifiListHtml = "<div class='wifi-item'><span style='color:#94a3b8;'>Tidak ada Wi-Fi terdeteksi. Silakan ketik nama Wi-Fi manual di bawah.</span></div>";
  } else {
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String security = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "🔓 Terbuka" : "🔒 WPA2";
      
      String signalColor = "#22c55e"; // Hijau
      String signalText = "Kuat";
      if (rssi < -75) { signalColor = "#ef4444"; signalText = "Lemah"; }
      else if (rssi < -60) { signalColor = "#eab308"; signalText = "Sedang"; }

      wifiListHtml += "<div class='wifi-item' onclick='selectWifi(\"" + ssid + "\")'>";
      wifiListHtml += "  <div class='wifi-info'>";
      wifiListHtml += "    <div class='wifi-name'>" + ssid + "</div>";
      wifiListHtml += "    <div class='wifi-meta'>" + security + " &bull; <span style='color:" + signalColor + "; font-weight:600;'>" + signalText + " (" + String(rssi) + " dBm)</span></div>";
      wifiListHtml += "  </div>";
      wifiListHtml += "  <button type='button' class='btn-select'>PILIH</button>";
      wifiListHtml += "</div>";
    }
  }

  String currentSsidDisplay = (savedSsid == "") ? "Belum Terhubung" : savedSsid;

  String html = "<!DOCTYPE html><html lang='id'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Edge AI - Pengaturan Wi-Fi</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }";
  html += "body { background: #0b1120; color: #f8fafc; margin: 0; padding: 16px; display: flex; justify-content: center; }";
  html += ".container { max-width: 440px; width: 100%; background: #1e293b; border: 1px solid #334155; border-radius: 20px; padding: 22px; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5); }";
  html += ".header { text-align: center; margin-bottom: 20px; }";
  html += ".header h2 { margin: 6px 0; color: #38bdf8; font-size: 22px; }";
  html += ".header p { margin: 0; color: #94a3b8; font-size: 13px; }";
  html += ".info-box { background: #0f172a; border: 1px solid #334155; border-radius: 12px; padding: 14px; margin-bottom: 18px; font-size: 12px; }";
  html += ".info-row { display: flex; justify-content: space-between; margin-bottom: 6px; }";
  html += ".info-row:last-child { margin-bottom: 0; }";
  html += ".info-label { color: #94a3b8; }";
  html += ".info-value { color: #38bdf8; font-weight: 600; }";
  html += ".section-title { font-size: 13px; font-weight: 700; color: #cbd5e1; text-transform: uppercase; margin: 16px 0 8px; letter-spacing: 0.5px; }";
  html += ".wifi-list { max-height: 190px; overflow-y: auto; background: #0f172a; border: 1px solid #334155; border-radius: 12px; padding: 6px; margin-bottom: 16px; }";
  html += ".wifi-item { display: flex; justify-content: space-between; align-items: center; padding: 10px; border-radius: 8px; cursor: pointer; transition: 0.2s; border-bottom: 1px solid #1e293b; }";
  html += ".wifi-item:hover { background: #1e293b; }";
  html += ".wifi-name { font-size: 14px; font-weight: 600; color: #f1f5f9; }";
  html += ".wifi-meta { font-size: 11px; color: #64748b; margin-top: 2px; }";
  html += ".btn-select { background: #0284c7; color: white; border: none; padding: 6px 12px; border-radius: 6px; font-size: 11px; font-weight: 700; cursor: pointer; }";
  html += "label { font-size: 12px; font-weight: 600; color: #94a3b8; display: block; margin-top: 12px; }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 12px; margin-top: 5px; border: 1px solid #475569; border-radius: 8px; background: #0f172a; color: #fff; font-size: 14px; }";
  html += "input[type='submit'] { width: 100%; background: linear-gradient(135deg, #0284c7, #2563eb); color: white; border: none; padding: 14px; border-radius: 10px; font-weight: 700; font-size: 15px; margin-top: 20px; cursor: pointer; }";
  html += ".tips { margin-top: 16px; padding: 12px; background: rgba(56, 189, 248, 0.08); border-left: 3px solid #38bdf8; border-radius: 6px; font-size: 12px; color: #94a3b8; line-height: 1.5; }";
  html += "</style>";
  html += "<script>";
  html += "function selectWifi(name) {";
  html += "  document.getElementById('ssid_input').value = name;";
  html += "  document.getElementById('password_input').focus();";
  html += "}";
  html += "</script></head><body>";
  
  html += "<div class='container'>";
  html += "  <div class='header'>";
  html += "    <h2>💧 ESP32 Water AI Setup</h2>";
  html += "    <p>Pengaturan Jaringan & Telemetri Kualitas Air</p>";
  html += "  </div>";

  html += "  <div class='info-box'>";
  html += "    <div class='info-row'><span class='info-label'>Status Wi-Fi Saat Ini</span><span class='info-value'>" + currentSsidDisplay + "</span></div>";
  html += "    <div class='info-row'><span class='info-label'>IP Gateway Hotspot</span><span class='info-value'>192.168.4.1</span></div>";
  html += "    <div class='info-row'><span class='info-label'>MAC Address ESP32</span><span class='info-value'>" + WiFi.macAddress() + "</span></div>";
  html += "    <div class='info-row'><span class='info-label'>Edge AI Classifier</span><span class='info-value' style='color:#22c55e;'>100% Offline Ready</span></div>";
  html += "  </div>";

  html += "  <div class='section-title'>PILIH WI-FI 2.4 GHz TERSEDIA:</div>";
  html += "  <div class='wifi-list'>" + wifiListHtml + "</div>";

  html += "  <form method='POST' action='/save'>";
  html += "    <label>NAMA WI-FI (SSID):</label>";
  html += "    <input type='text' id='ssid_input' name='ssid' value='" + savedSsid + "' placeholder='Pilih dari daftar atau ketik manual...' required>";

  html += "    <label>PASSWORD WI-FI:</label>";
  html += "    <input type='password' id='password_input' name='password' placeholder='Masukkan password Wi-Fi...'>";

  html += "    <input type='submit' value='⚡ HUBUNGKAN ESP32 KE WI-FI'>";
  html += "  </form>";

  html += "  <div class='tips'>";
  html += "    💡 <b>Catatan Penting:</b><br>";
  html += "    &bull; ESP32 hanya mendukung sinyal <b>Wi-Fi 2.4 GHz</b>.<br>";
  html += "    &bull; Wi-Fi bersifat opsional! Uji klasifikasi kualitas air fisik tetap bisa berjalan mandiri via Push Button (D32).";
  html += "  </div>";
  html += "</div></body></html>";

  webServer.send(200, "text/html", html);
}

void handlePortalSave() {
  String reqSsid = webServer.arg("ssid");
  reqSsid.trim();
  String reqPass = webServer.arg("password");
  reqPass.trim();

  if (reqSsid == "") {
    webServer.send(200, "text/html", "<h3>Nama Wi-Fi tidak boleh kosong! <a href='/'>Kembali</a></h3>");
    return;
  }

  Serial.println("\n[SETUP WEB] Menerima konfigurasi Wi-Fi baru dari HP:");
  Serial.print("  SSID     : "); Serial.println(reqSsid);
  Serial.print("  Password : "); Serial.println(reqPass.length() > 0 ? "********" : "(Tanpa Password)");

  // Aktifkan mode Dual (AP + STA) agar ESP32 bisa tes koneksi ke router
  // tanpa mematikan Hotspot AP yang sedang dibuka di browser HP!
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(reqSsid.c_str(), reqPass.c_str());

  Serial.print("[SETUP WEB] Menguji koneksi ke router");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) { // Tunggu hingga 12.5 detik
    delay(500);
    Serial.print(".");
    attempts++;
  }

  // JIKA BERHASIL TERHUBUNG:
  if (WiFi.status() == WL_CONNECTED) {
    savedSsid = reqSsid;
    savedPassword = reqPass;

    preferences.begin("wifi_cfg", false);
    preferences.putString("ssid", savedSsid);
    preferences.putString("pass", savedPassword);
    preferences.end();

    server.begin(); // Buka TCP Port 8080 untuk Live Viewer Laptop
    isApModeActive = false;

    String successHtml = "<!DOCTYPE html><html lang='id'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    successHtml += "<style>body{background:#0b1120;color:#fff;font-family:sans-serif;padding:24px;text-align:center;}";
    successHtml += ".card{background:#1e293b;border-radius:18px;padding:28px;max-width:400px;margin:auto;border:1px solid #22c55e;box-shadow:0 10px 25px rgba(0,0,0,0.5);}";
    successHtml += "h2{color:#22c55e;margin-top:0;} .ip-box{background:#0f172a;padding:14px;border-radius:10px;font-size:18px;font-weight:bold;color:#38bdf8;margin:16px 0;border:1px solid #334155;}</style></head><body>";
    successHtml += "<div class='card'>";
    successHtml += "  <h2>✅ BERHASIL TERHUBUNG!</h2>";
    successHtml += "  <p>ESP32 kini sudah resmi terhubung ke Wi-Fi:</p>";
    successHtml += "  <h3 style='color:#f8fafc;'>" + savedSsid + "</h3>";
    successHtml += "  <div class='ip-box'>IP ESP32: " + WiFi.localIP().toString() + "</div>";
    successHtml += "  <p style='color:#94a3b8;font-size:13px;'>Port TCP Telemetri: <b>8080</b></p>";
    successHtml += "  <p style='color:#cbd5e1;font-size:13px;line-height:1.5;'>Hotspot setup akan dinonaktifkan. Silakan hubungkan kembali HP/Laptop Anda ke Wi-Fi <b>" + savedSsid + "</b> dan jalankan <code>live_viewer.py</code> di laptop!</p>";
    successHtml += "</div></body></html>";

    webServer.send(200, "text/html", successHtml);

    Serial.println("\n========================================================");
    Serial.println(" [OK] SUKSES TERSAMBUNG KE WI-FI BARU!");
    Serial.print(" >>> ALAMAT IP ESP32 : "); Serial.println(WiFi.localIP());
    Serial.println(" >>> PORT TCP        : 8080");
    Serial.println("========================================================\n");

    delay(3000);
    WiFi.mode(WIFI_STA); // Nonaktifkan AP agar bersih
  } 
  // JIKA GAGAL TERHUBUNG (Password salah / router tidak merespons):
  else {
    String failHtml = "<!DOCTYPE html><html lang='id'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    failHtml += "<style>body{background:#0b1120;color:#fff;font-family:sans-serif;padding:24px;text-align:center;}";
    failHtml += ".card{background:#1e293b;border-radius:18px;padding:28px;max-width:400px;margin:auto;border:1px solid #ef4444;}";
    failHtml += "h2{color:#ef4444;margin-top:0;} a{display:inline-block;background:#0284c7;color:#fff;padding:12px 24px;border-radius:8px;text-decoration:none;font-weight:bold;margin-top:16px;}</style></head><body>";
    failHtml += "<div class='card'>";
    failHtml += "  <h2>❌ GAGAL TERHUBUNG</h2>";
    failHtml += "  <p>ESP32 tidak dapat terhubung ke <b>" + reqSsid + "</b>.</p>";
    failHtml += "  <p style='color:#94a3b8;font-size:13px;text-align:left;'><b>Kemungkinan penyebab:</b><br>1. Password Wi-Fi salah.<br>2. Sinyal Wi-Fi router terlalu jauh/lemah.<br>3. Jaringan adalah 5 GHz murni (ESP32 butuh 2.4 GHz).</p>";
    failHtml += "  <a href='/'>⬅️ KEMBALI & COBA LAGI</a>";
    failHtml += "</div></body></html>";

    webServer.send(200, "text/html", failHtml);
    Serial.println("\n[!] Gagal menghubungkan ke Wi-Fi baru. Hotspot AP tetap aktif.");
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

  // 2. Logika Koneksi:
  //    A. Jika belum pernah setup Wi-Fi (savedSsid kosong) -> Langsung luncurkan Hotspot AP!
  if (savedSsid == "") {
    Serial.println("\n[WIFI] Belum ada Wi-Fi yang dikonfigurasi. Membuka Captive Portal AP!");
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
    while (WiFi.status() != WL_CONNECTED && retry < 25) { // Beri waktu hingga 12.5 detik
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
      Serial.println("\n[!] Gagal terhubung ke Wi-Fi tersimpan. Meluncurkan Hotspot Setup...");
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
  } else {
    // Standby: Selalu Solid ON (Tanpa Kedip, karena Wi-Fi bersifat opsional)
    digitalWrite(PIN_LED_STATUS, HIGH);
  }

  // 4. Mode Hemat Data & Daya:
  //    Akusisi data sensor (10 Hz) dan streaming data PENUH HANYA berjalan
  //    ketika tombol push button ditekan & pengujian 30 detik sedang aktif!
  if (isTesting) {
    if (currentMillis - lastSampleTime >= SAMPLE_INTERVAL_MS) {
      lastSampleTime = currentMillis;
      sampleId++;

      int tdsRaw = 0;
      float tdsVoltage = 0.0;
      float tdsPpm = 0.0;
      int turbidityRaw = 0;
      float turbidityVoltage = 0.0;
      float turbidityNtu = 0.0;

      // Baca Data Sensor (Simulasi Intervensi / ADC Fisik)
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

      // Update Ring Buffer & Inferensi Edge AI setiap 5 detik
      unsigned long elapsedTestTime = currentMillis - testStartTime;
      updateEdgeAiWindow(tdsPpm, turbidityNtu, elapsedTestTime);

      // Cek apakah 30 detik pengujian sudah selesai
      if (elapsedTestTime >= TEST_DURATION_MS) {
        isTesting = false;
        digitalWrite(PIN_LED_STATUS, HIGH); // LED Status kembali solid ON

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
        setClassificationLeds(finalVerdict); // Kunci LED hasil akhir

        Serial.println("\n========================================================");
        Serial.println(" [OK] PENGUJIAN 30 DETIK SELESAI!");
        Serial.print(" HASIL AKHIR TERKUNCI (MAJORITY VOTE) : ");
        Serial.println(finalVerdict);
        Serial.print(" Rincian Voting 5-Detikan             : Low="); Serial.print(voteCountLow);
        Serial.print(" | Medium="); Serial.print(voteCountMed);
        Serial.print(" | High="); Serial.println(voteCountHigh);
        Serial.println(" Sensor dimatikan kembali ke mode hemat daya.");
        Serial.println(" Tekan Push Button untuk memulai pengujian baru.");
        Serial.println("========================================================\n");
      }

      // Stream JSON Telemetri Penuh ke Laptop saat Uji 30s Aktif
      if (!isApModeActive && client && client.connected()) {
        int elapsedSec = (int)((currentMillis - testStartTime) / 1000);
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
    }
  } 
  // 5. Saat Standby: Sensor TIDAK dibaca & Wi-Fi hanya kirim status hemat (Heartbeat 1 Hz)
  else {
    static unsigned long lastHeartbeatTime = 0;
    if (currentMillis - lastHeartbeatTime >= 1000) { // Cukup 1 detik sekali
      lastHeartbeatTime = currentMillis;

      if (!isApModeActive && client && client.connected()) {
        String hb = "{\"is_testing\":false,\"preset\":\"" + currentPresetName + "\",\"edge_ai_class\":\"" + currentAiPrediction + "\"}\n";
        client.print(hb);
      }

      if (isApModeActive) {
        Serial.println("[HOTSPOT AP SETUP] Sambungkan HP ke 'ESP32_WaterAI' (PW: 12345678)!");
      } else {
        Serial.print("[STANDBY - "); Serial.print(currentPresetName);
        Serial.println("] Sensor OFF. Tekan Tombol (D32) untuk Mulai Uji 30s!");
      }
    }
  }
}
