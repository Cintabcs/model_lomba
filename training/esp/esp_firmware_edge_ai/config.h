#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// KONFIGURASI PIN HARDWARE PCB (ESP32)
// =============================================================================
#if defined(ESP32)
  const int PIN_TDS           = 34; // Sinyal Analog TDS (GPIO 34 / ADC1)
  const int PIN_TURBIDITY     = 35; // Sinyal Analog Turbidity (GPIO 35 / ADC1)

  // Indikator LED Klasifikasi Hasil
  const int PIN_LED_RED       = 23; // LED Merah  -> High (Air Kotor / Keruh) (D23)
  const int PIN_LED_YELLOW    = 22; // LED Kuning -> Medium (Air Sedang)      (D22)
  const int PIN_LED_GREEN     = 25; // LED Hijau  -> Low (Air Bersih)         (D25)

  // Indikator Status & Tombol Trigger
  const int PIN_LED_STATUS    = 33; // LED Status (Biru) -> Solid ON saat Standby, Blink saat Uji 30s (D33)
  const int PIN_BUTTON        = 32; // Push Button (D32, INPUT_PULLUP ke GND)

  const float VREF            = 3.3f;       
  const float ADC_RESOLUTION  = 4095.0f; // 12-bit ADC ESP32
#else
  const int PIN_TDS           = A0;
  const int PIN_TURBIDITY     = A0;
  const int PIN_LED_GREEN     = D1;
  const int PIN_LED_YELLOW    = D2;
  const int PIN_LED_RED       = D3;
  const int PIN_LED_STATUS    = D4;
  const int PIN_BUTTON        = D5;
  const float VREF            = 3.3f;
  const float ADC_RESOLUTION  = 1023.0f;
#endif

// =============================================================================
// KONFIGURASI WI-FI & CAPTIVE PORTAL
// =============================================================================
const char *AP_SSID           = "ESP32_WaterAI";
const char *AP_PASSWORD       = "12345678"; // Password hotspot ESP32
const byte DNS_PORT           = 53;
const int SERVER_PORT         = 8080;

// =============================================================================
// PARAMETER EDGE AI & PENGUJIAN 30 DETIK
// =============================================================================
const int WINDOW_SIZE                  = 50;    // 5 Detik = 50 Sampel pada 10 Hz
const unsigned long TEST_DURATION_MS   = 30000; // 30 Detik
const unsigned long SAMPLE_INTERVAL_MS = 100;   // 10 Hz = 100 ms
const unsigned long BLINK_INTERVAL_MS  = 150;   // Kedip cepat 150 ms saat pengujian aktif

#endif // CONFIG_H
