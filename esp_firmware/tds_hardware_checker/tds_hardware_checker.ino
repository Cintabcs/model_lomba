/*
  =============================================================================
  ESP32 PIN 34 TURBIDITY TESTER
  =============================================================================
  Sketsa ini khusus untuk mengetes sensor Turbidity di GPIO 34.
  Membaca Raw ADC, Voltase, dan NTU secara realtime.
  =============================================================================
*/

const int PIN_TEST = 34; // Colok kabel sinyal Turbidity ke GPIO 34

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================================");
  Serial.println("   TES SENSOR TURBIDITY PADA PIN GPIO 34 (ESP32)");
  Serial.println("========================================================");

  // Set rentang ADC ESP32 ke 0 - 3.3V
  analogSetAttenuation(ADC_11db);
  pinMode(PIN_TEST, INPUT);

  Serial.println("Colok kabel sinyal Turbidity ke GPIO 34.");
  Serial.println("Buka Serial Monitor (115200 baud)...\n");
}

void loop() {
  // 1. Baca Raw ADC dari GPIO 34
  int raw = analogRead(PIN_TEST);
  
  // 2. Hitung Voltase
  float volt = raw * (3.3 / 4095.0);

  // 3. Hitung NTU (Kalibrasi linear custom)
  float ntu = 0.0;
  if (volt >= 2.4) {
    ntu = 0.0;
  } else {
    ntu = (2.4 - volt) * (500.0 / 1.4);
  }
  if (ntu < 0) ntu = 0;

  // 4. Tampilkan ke Serial Monitor
  Serial.print("[GPIO 34 - Turbidity] Raw: ");
  Serial.print(raw);
  Serial.print(" | Tegangan: ");
  Serial.print(volt, 3);
  Serial.print(" V | Turbidity: ");
  Serial.print(ntu, 1);
  Serial.print(" NTU");

  if (raw == 0) {
    Serial.println("  <-- [0V: Kabel lepas / tidak ada sinyal]");
  } else if (raw > 4000) {
    Serial.println("  <-- [Maksimal 3.3V: Short / Floating]");
  } else {
    Serial.println("  <-- [SINYAL AKTIF DARI SENSOR! OK]");
  }

  delay(500); // Update setiap 0.5 detik
}
