# ESP32 Water Quality Data Acquisition (10 Hz) - TDS & Turbidity

Sistem akuisisi data nirkabel dari ESP32/ESP8266 ke Laptop menggunakan **Sensor TDS** dan **Sensor Turbidity (Kekeruhan)** dengan sampling rate **10 Hz (10 sampel/detik)**.

---

## 📶 2 Opsi Koneksi Network

### Opsi A: Gabung ke Wi-Fi Rumah / HP (SANGAT DIREKOMENDASIKAN 👍)
> **Keuntungan**: Laptop Anda **tetap bisa internetan** (browsing, youtube, ngerjain tugas) sambil merekam data dari ESP32!

1. Buka [`esp_firmware.ino`](file:///c:/Users/FILKOM/Documents/Documents/Bali/esp_firmware/esp_firmware.ino).
2. Pastikan `#define USE_HOME_WIFI true`.
3. Isi SSID dan Password Wi-Fi Rumah/HP Anda:
   ```cpp
   const char *wifi_ssid     = "NAMA_WIFI_RUMAH_ANDA";
   const char *wifi_password = "PASSWORD_WIFI_RUMAH";
   ```
4. Upload firmware ke ESP32. Buka Serial Monitor (`115200 baud`) dan **catat IP Address ESP32** (contoh: `192.168.1.15`).
5. Jalankan Python di laptop, pilih Menu `2` dan masukkan IP ESP32 tersebut.

---

### Opsi B: Hotspot AP buatan ESP32
> ESP32 membuat Wi-Fi Hotspot sendiri. Cocok jika sedang di luar ruangan / tanpa Wi-Fi router.

1. Set `#define USE_HOME_WIFI false` pada `esp_firmware.ino`.
2. Hubungkan Laptop ke Wi-Fi `ESP_DA_Logger` (Pass: `12345678`).
3. IP default: `192.168.4.1`.

---

## 📂 Struktur Folder Project

```text
Bali/
├── esp_firmware/
│   └── esp_firmware.ino      # Firmware ESP32 TDS & Turbidity (Mode WiFi Rumah / AP)
├── python_recorder/
│   ├── config.py             # Konfigurasi IP & Port
│   ├── data_client.py        # Client koneksi Socket TCP
│   ├── data_saver.py         # Module perekam data ke file CSV dengan label dinamis
│   ├── main.py               # Aplikasi utama Python (Menu Interaktif & Perekam Data)
│   ├── requirements.txt      # Dependensi Python
│   └── dataset/              # Folder otomatis tempat menyimpan file CSV
└── README.md                 # Panduan penggunaan
```

---

## 📊 Format Output Data (CSV)
Setiap data disimpan otomatis di folder `python_recorder/dataset/`:
`data_<LABEL>_<YYYYMMDD_HHMMSS>.csv`

**Struktur Kolom CSV:**
`pc_timestamp`, `esp_timestamp_ms`, `label`, `sample_id`, `tds_raw`, `tds_voltage`, `tds_ppm`, `turbidity_raw`, `turbidity_voltage`, `turbidity_ntu`
