#include <Arduino.h>

/*
  =============================================================================
  ESP32 LED & PUSH BUTTON HARDWARE TESTER (10-SECOND PARTY ANIMATION)
  =============================================================================
  Pinout sesuai jalur PCB:
  - LED Merah   : GPIO 23 (D23)
  - LED Kuning  : GPIO 22 (D22)
  - LED Hijau   : GPIO 25 (D25)
  - LED Putih   : GPIO 33 (D33)
  - Push Button : GPIO 32 (D32) -> Internal INPUT_PULLUP (kaki lain ke GND)

  Cara Kerja:
  1. Standby:
     - LED Putih MENYALA TERUS (Solid ON).
     - LED Merah, Kuning, Hijau MATI.
  2. Saat Tombol (D32) Ditekan:
     - Masuk mode pesta 10 Detik!
     - LED Putih kedap-kedip cepat (indikator proses).
     - LED Merah, Kuning, Hijau bergantian (Running Chase & Disco Strobo).
     - Countdown 10 detik tampil di Serial Monitor (115200 baud).
  3. Setelah 10 Detik:
     - Semua LED berkedip serentak 2x sebagai tanda sukses.
     - LED Putih kembali menyala solid (Standby).
     - Siap dipencet lagi!
  =============================================================================
*/

// Konfigurasi Pin PCB
const int PIN_LED_RED    = 23; // D23
const int PIN_LED_YELLOW = 22; // D22
const int PIN_LED_GREEN  = 25; // D25
const int PIN_LED_WHITE  = 33; // D33
const int PIN_BUTTON     = 32; // D32 (INPUT_PULLUP)

// Variabel Kontrol
bool isPartyMode = false;
unsigned long partyStartTime = 0;
const unsigned long PARTY_DURATION_MS = 10000; // 10 Detik

// Timer Animasi (Non-blocking)
unsigned long lastWhiteBlink = 0;
bool whiteState = false;

unsigned long lastAnimStep = 0;
int animStep = 0;
int lastRemainingSec = -1;

// Matikan / Nyalakan LED Warna
void setRgb(bool r, bool y, bool g) {
  digitalWrite(PIN_LED_RED, r ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW, y ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, g ? HIGH : LOW);
}

// Cek Push Button dengan Debounce
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
      if (currentBtnState == LOW) { // Tombol ditekan (hubung ke GND)
        pressed = true;
      }
    }
  }
  lastBtnState = reading;
  return pressed;
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("");
  Serial.println("========================================================");
  Serial.println("   ESP32 HARDWARE CHECKER: LED & PUSH BUTTON TEST");
  Serial.println("========================================================");
  Serial.println(" Pin Mapping:");
  Serial.println("   [R] LED Merah   : GPIO 23");
  Serial.println("   [Y] LED Kuning  : GPIO 22");
  Serial.println("   [G] LED Hijau   : GPIO 25");
  Serial.println("   [W] LED Putih   : GPIO 33");
  Serial.println("   [B] Push Button : GPIO 32");
  Serial.println("--------------------------------------------------------");

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Initial State: Standby
  digitalWrite(PIN_LED_WHITE, HIGH); // LED Putih Solid ON
  setRgb(false, false, false);

  Serial.println("[STANDBY] LED Putih ON. Tekan Push Button (D32) untuk mulai tes 10 detik!\n");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  unsigned long now = millis();

  // 1. Cek Tombol Ditekan
  if (!isPartyMode && checkButtonPressed()) {
    isPartyMode = true;
    partyStartTime = now;
    animStep = 0;
    lastRemainingSec = -1;

    Serial.println("\n[RUN] TOMBOL DITEKAN! MEMULAI ANIMASI 10 DETIK!");
  }

  // 2. Mode Animasi 10 Detik
  if (isPartyMode) {
    unsigned long elapsed = now - partyStartTime;
    int remainingSec = (int)((PARTY_DURATION_MS - elapsed + 999) / 1000);

    // Cetak Countdown tiap detik
    if (remainingSec != lastRemainingSec && remainingSec >= 0) {
      lastRemainingSec = remainingSec;
      Serial.print("[TIMER] Sisa Waktu Animasi: ");
      Serial.print(remainingSec);
      Serial.println(" Detik...");
    }

    // A. Kedip-kedip LED Putih (Blink tiap 120 ms)
    if (now - lastWhiteBlink >= 120) {
      lastWhiteBlink = now;
      whiteState = !whiteState;
      digitalWrite(PIN_LED_WHITE, whiteState ? HIGH : LOW);
    }

    // B. Animasi Asik LED Merah, Kuning, Hijau
    // Fase 1: 0 - 4 Detik (Running Knight Rider: Merah -> Kuning -> Hijau -> Kuning)
    if (elapsed < 4000) {
      if (now - lastAnimStep >= 130) {
        lastAnimStep = now;
        animStep = (animStep + 1) % 4;
        switch (animStep) {
          case 0: setRgb(true,  false, false); break; // Merah
          case 1: setRgb(false, true,  false); break; // Kuning
          case 2: setRgb(false, false, true);  break; // Hijau
          case 3: setRgb(false, true,  false); break; // Kuning balik
        }
      }
    }
    // Fase 2: 4 - 7 Detik (Strobo Bergantian: [Merah+Hijau] vs [Kuning])
    else if (elapsed < 7000) {
      if (now - lastAnimStep >= 100) {
        lastAnimStep = now;
        animStep = (animStep + 1) % 2;
        if (animStep == 0) {
          setRgb(true, false, true);   // Merah & Hijau bareng
        } else {
          setRgb(false, true, false);  // Kuning
        }
      }
    }
    // Fase 3: 7 - 10 Detik (Disko Cepat Putar: Chase Turbo!)
    else {
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

    // C. Cek apakah 10 Detik sudah berakhir
    if (elapsed >= PARTY_DURATION_MS) {
      isPartyMode = false;

      // Animasi Penutup: Kedip kompak 2x tanda sukses
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
    // Mode Standby: Pastikan LED Putih selalu ON dan warna mati
    digitalWrite(PIN_LED_WHITE, HIGH);
    setRgb(false, false, false);
  }
}
