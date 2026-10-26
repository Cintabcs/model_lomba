#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

/*
  =============================================================================
  ESP32 ALL-IN-ONE HARDWARE & SENSOR CHECKER (WI-FI + LED + BUTTON + TDS + TURB)
  =============================================================================
  Pinout sesuai jalur PCB:
  - LED Merah      : GPIO 23 (D23)
  - LED Kuning     : GPIO 22 (D22)
  - LED Hijau      : GPIO 25 (D25)
  - LED Putih      : GPIO 33 (D33)
  - Push Button    : GPIO 32 (D32) -> Internal INPUT_PULLUP (kaki lain ke GND)
  - Sensor TDS     : GPIO 34 (D34) -> ADC1
  - Sensor Turb    : GPIO 35 (D35) -> ADC1
  - LED Biru       : GPIO 2 (Onboard Blue LED ESP32)

  Logika Status LED:
  1. LED Biru (Indikator Wi-Fi):
     - Belum Konek Wi-Fi: Kedap-kedip 2 kali lalu istirahat (Blink-Blink-Pause).
     - Sudah Konek Wi-Fi: MENYALA TERUS (Solid ON).
  2. LED Putih (Status Sistem):
     - Standby: MENYALA TERUS (Solid ON).
     - Saat Tombol Ditekan (Uji 10 Detik): KEDAP-KEDIP CEPAT.
  3. LED Merah, Kuning, Hijau:
     - Standby: MATI.
     - Saat Tombol Ditekan: Joget asik (Running Chase & Strobo) selama 10 detik.
  4. Stream Wi-Fi TCP (Port 8080):
     - Mengirim JSON pembacaan TDS & Turbidity secara realtime ke laptop (live_viewer.py).
  =============================================================================
*/

// =============================================================================
// KONFIGURASI WI-FI RUMAH
// =============================================================================
const char *WIFI_SSID     = "MyRepublic_C2222847";
const char *WIFI_PASSWORD = "C2222847";
const int SERVER_PORT     = 8080;

WiFiServer server(SERVER_PORT);
WiFiClient client;

// =============================================================================
// KONFIGURASI PIN PCB
// =============================================================================
const int PIN_LED_RED    = 23; // D23
const int PIN_LED_YELLOW = 22; // D22
const int PIN_LED_GREEN  = 25; // D25
const int PIN_LED_WHITE  = 33; // D33
const int PIN_BUTTON     = 32; // D32 (INPUT_PULLUP)
const int PIN_TDS        = 34; // D34 (ADC1)
const int PIN_TURBIDITY  = 35; // D35 (ADC1)
const int PIN_LED_BLUE   = 2;  // Onboard Blue LED (GPIO 2)

const float VREF         = 3.3;
const float ADC_RES      = 4095.0;

// Variabel Animasi Pesta 10 Detik
bool isPartyMode = false;
unsigned long partyStartTime = 0;
const unsigned long PARTY_DURATION_MS = 10000; // 10 Detik

unsigned long lastWhiteBlink = 0;
bool whiteState = false;

unsigned long lastAnimStep = 0;
int animStep = 0;
int lastRemainingSec = -1;

// Timer Baca Sensor (10 Hz = 100 ms)
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL_MS = 100;
unsigned long sampleId = 0;

float currentTdsPpm = 0.0;
float currentTdsVolt = 0.0;
float currentTurbNtu = 0.0;
float currentTurbVolt = 0.0;

// =============================================================================
// FUNGSI KENDALI LED RGB
// =============================================================================
void setRgb(bool r, bool y, bool g) {
  digitalWrite(PIN_LED_RED, r ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW, y ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, g ? HIGH : LOW);
}

// =============================================================================
// FUNGSI INDIKATOR LED BIRU (WI-FI STATUS)
// =============================================================================
// Pola: Jika belum konek -> Kedip 2x lalu istirahat (Blink-Blink-Pause).
//       Jika sudah konek -> Solid ON terus menerus.
void updateBlueWifiLed(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(PIN_LED_BLUE, HIGH); // Solid ON
  } else {
    // Siklus pola 1200 ms:
    // 0 - 100 ms   : ON
    // 100 - 200 ms : OFF
    // 200 - 300 ms : ON
    // 300 - 1200 ms: OFF (Istirahat / Jeda)
    unsigned long phase = now % 1200;
    if ((phase < 100) || (phase >= 200 && phase < 300)) {
      digitalWrite(PIN_LED_BLUE, HIGH);
    } else {
      digitalWrite(PIN_LED_BLUE, LOW);
    }
  }
}

// =============================================================================
// FUNGSI CEK PUSH BUTTON (DEBOUNCE)
// =============================================================================
bool checkButtonPressed() {
  static unsigned long lastDebounce = 0;
  static int lastBtnState = HIGH;
  static int currentBtnState = HIGH;
  
  int reading = digitalRead(PIN_BUTTON);
  bool pressed = false;

  if (reading != lastBtnState) {
    lastDebounce = millis();
  }

  if ((millis() - lastDebounce) > 50) {
    if (reading != currentBtnState) {
      currentBtnState = reading;
      if (currentBtnState == LOW) { // Tombol ditekan ke GND
        pressed = true;
      }
    }
  }
  lastBtnState = reading;
  return pressed;
}

// =============================================================================
// FUNGSI BACA SENSOR TDS & TURBIDITY
// =============================================================================
void readSensors() {
  // 1. Baca Sensor TDS (GPIO34)
  int tdsRaw = analogRead(PIN_TDS);
  currentTdsVolt = tdsRaw * (VREF / ADC_RES);
  currentTdsPpm = (133.42 * currentTdsVolt * currentTdsVolt * currentTdsVolt 
                  - 255.86 * currentTdsVolt * currentTdsVolt 
                  + 857.39 * currentTdsVolt) * 0.5;
  if (currentTdsPpm < 0) currentTdsPpm = 0;

  // 2. Baca Sensor Turbidity (GPIO35)
  int turbRaw = analogRead(PIN_TURBIDITY);
  currentTurbVolt = turbRaw * (VREF / ADC_RES);
  if (currentTurbVolt >= 2.4) {
    currentTurbNtu = 0.0;
  } else {
    currentTurbNtu = (2.4 - currentTurbVolt) * (500.0 / 1.4);
  }
  if (currentTurbNtu < 0) currentTurbNtu = 0;
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("");
  Serial.println("========================================================");
  Serial.println("   ESP32 CHECKER: WI-FI + LED + BUTTON + TDS + TURBIDITY");
  Serial.println("========================================================");
  Serial.println(" Pin Mapping:");
  Serial.println("   [R] LED Merah   : GPIO 23");
  Serial.println("   [Y] LED Kuning  : GPIO 22");
  Serial.println("   [G] LED Hijau   : GPIO 25");
  Serial.println("   [W] LED Putih   : GPIO 33 (Standby Solid / Blink saat Uji)");
  Serial.println("   [B] LED Biru    : GPIO 2  (Wi-Fi Status)");
  Serial.println("   [BTN] Tombol    : GPIO 32 (INPUT_PULLUP)");
  Serial.println("   [TDS] Sinyal    : GPIO 34 (ADC1)");
  Serial.println("   [TURB] Sinyal   : GPIO 35 (ADC1)");
  Serial.println("--------------------------------------------------------");

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  #if defined(ESP32)
    analogSetAttenuation(ADC_11db); // ADC Range 0 - 3.3V
    pinMode(PIN_TDS, INPUT);
    pinMode(PIN_TURBIDITY, INPUT);
  #endif

  // Initial State LED
  digitalWrite(PIN_LED_WHITE, HIGH); // LED Putih Solid ON
  digitalWrite(PIN_LED_BLUE, LOW);
  setRgb(false, false, false);

  // Inisialisasi Wi-Fi Non-blocking (Background connect)
  Serial.print("[WIFI] Menghubungkan ke: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  server.begin();
  Serial.println("[TCP] Server siap di port 8080.");
  Serial.println("[INFO] LED Biru kedip 2x lalu jeda (mencari Wi-Fi).");
  Serial.println("[INFO] LED Putih Solid ON. Tekan Tombol (D32) untuk pesta 10 detik!\n");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  unsigned long now = millis();

  // 1. Update Indikator LED Biru (Status Wi-Fi)
  updateBlueWifiLed(now);

  // 2. Notifikasi saat Wi-Fi pertama kali berhasil terhubung
  static bool wifiAnnounced = false;
  if (WiFi.status() == WL_CONNECTED && !wifiAnnounced) {
    wifiAnnounced = true;
    Serial.println("\n[✓ WIFI TERHUBUNG!]");
    Serial.print(" IP ESP32 : "); Serial.println(WiFi.localIP());
    Serial.println(" LED Biru : MENYALA TERUS (SOLID ON)\n");
  } else if (WiFi.status() != WL_CONNECTED) {
    wifiAnnounced = false;
  }

  // 3. Terima Koneksi Client TCP Laptop (live_viewer.py)
  if (!client || !client.connected()) {
    client = server.available();
  }

  // 4. Cek Tombol Push Button
  if (!isPartyMode && checkButtonPressed()) {
    isPartyMode = true;
    partyStartTime = now;
    animStep = 0;
    lastRemainingSec = -1;

    Serial.println("\n========================================================");
    Serial.println("[RUN] TOMBOL DITEKAN! MEMULAI ANIMASI PESTA 10 DETIK!");
    Serial.println("========================================================");
  }

  // 5. Mode Animasi 10 Detik
  if (isPartyMode) {
    unsigned long elapsed = now - partyStartTime;
    int remainingSec = (int)((PARTY_DURATION_MS - elapsed + 999) / 1000);

    // Cetak Countdown tiap detik
    if (remainingSec != lastRemainingSec && remainingSec >= 0) {
      lastRemainingSec = remainingSec;
      Serial.print("[TIMER 10s] Sisa: ");
      Serial.print(remainingSec);
      Serial.print("s | TDS: ");
      Serial.print(currentTdsPpm, 1);
      Serial.print(" PPM | Turb: ");
      Serial.print(currentTurbNtu, 1);
      Serial.println(" NTU");
    }

    // A. Kedip-kedip LED Putih (Blink 120 ms)
    if (now - lastWhiteBlink >= 120) {
      lastWhiteBlink = now;
      whiteState = !whiteState;
      digitalWrite(PIN_LED_WHITE, whiteState ? HIGH : LOW);
    }

    // B. Joget Asik LED Merah, Kuning, Hijau
    if (elapsed < 4000) {
      // Fase 1: Knight Rider Wave (Merah -> Kuning -> Hijau -> Kuning)
      if (now - lastAnimStep >= 130) {
        lastAnimStep = now;
        animStep = (animStep + 1) % 4;
        switch (animStep) {
          case 0: setRgb(true,  false, false); break;
          case 1: setRgb(false, true,  false); break;
          case 2: setRgb(false, false, true);  break;
          case 3: setRgb(false, true,  false); break;
        }
      }
    } else if (elapsed < 7000) {
      // Fase 2: Strobo Bergantian ([Merah+Hijau] vs [Kuning])
      if (now - lastAnimStep >= 100) {
        lastAnimStep = now;
        animStep = (animStep + 1) % 2;
        if (animStep == 0) setRgb(true, false, true);
        else setRgb(false, true, false);
      }
    } else {
      // Fase 3: Turbo Chase Cepat
      if (now - lastAnimStep >= 75) {
        lastAnimStep = now;
        animStep = (animStep + 1) % 3;
        switch (animStep) {
          case 0: setRgb(true,  false, false); break;
          case 1: setRgb(false, true,  false); break;
          case 2: setRgb(false, false, true);  break;
        }
      }
    }

    // C. Selesai 10 Detik
    if (elapsed >= PARTY_DURATION_MS) {
      isPartyMode = false;

      // Kedip kompak 2x tanda sukses
      setRgb(false, false, false);
      digitalWrite(PIN_LED_WHITE, LOW);
      delay(150);
      setRgb(true, true, true);
      digitalWrite(PIN_LED_WHITE, HIGH);
      delay(200);
      setRgb(false, false, false);
      digitalWrite(PIN_LED_WHITE, LOW);
      delay(150);
      setRgb(true, true, true);
      digitalWrite(PIN_LED_WHITE, HIGH);
      delay(250);

      // Kembali ke Standby
      setRgb(false, false, false);
      digitalWrite(PIN_LED_WHITE, HIGH); // LED Putih kembali solid ON

      Serial.println("\n[OK] Animasi 10 detik tuntas!");
      Serial.println("[STANDBY] LED Putih Solid ON. Siap dipencet lagi!\n");
    }
  } else {
    // Mode Standby
    digitalWrite(PIN_LED_WHITE, HIGH);
    setRgb(false, false, false);
  }

  // 6. Loop Sampling Sensor (10 Hz = 100 ms)
  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;
    sampleId++;

    readSensors();

    // Stream JSON via Wi-Fi TCP jika ada client (live_viewer.py)
    if (client && client.connected()) {
      String json = "{";
      json += "\"sample_id\":" + String(sampleId) + ",";
      json += "\"tds_raw\":" + String((int)(currentTdsVolt * ADC_RES / VREF)) + ",";
      json += "\"tds_voltage\":" + String(currentTdsVolt, 3) + ",";
      json += "\"tds_ppm\":" + String(currentTdsPpm, 1) + ",";
      json += "\"turbidity_voltage\":" + String(currentTurbVolt, 3) + ",";
      json += "\"turbidity_ntu\":" + String(currentTurbNtu, 1) + ",";
      json += "\"is_testing\":" + String(isPartyMode ? "true" : "false") + ",";
      json += "\"edge_ai_class\":\"" + String(isPartyMode ? "Testing 10s" : "Standby") + "\"";
      json += "}\n";
      client.print(json);
    }

    // Monitor via Serial setiap 1 detik saat standby
    if (!isPartyMode && (sampleId % 10 == 0)) {
      Serial.print("[LIVE] TDS: ");
      Serial.print(currentTdsPpm, 1);
      Serial.print(" PPM ("); Serial.print(currentTdsVolt, 2); Serial.print("V)");
      Serial.print(" | Turb: ");
      Serial.print(currentTurbNtu, 1);
      Serial.print(" NTU ("); Serial.print(currentTurbVolt, 2); Serial.print("V)");
      Serial.print(" | Wi-Fi: ");
      Serial.println((WiFi.status() == WL_CONNECTED) ? "TERHUBUNG (LED Biru ON)" : "Mencari... (LED Biru Kedip 2x)");
    }
  }
}
