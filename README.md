# 🌊 AquaSentry - Embedded Edge AI Water Quality Classifier

AquaSentry adalah sistem klasifikasi kualitas air berbasis **Embedded Edge AI (Random Forest)** pada mikrokontroler **ESP32** dengan indikator fisik LED, akuisisi sensor cerdas, antarmuka **Wi-Fi Captive Portal**, serta telemetri ganda (**Wi-Fi TCP Lokal & HiveMQ Cloud MQTT TLS 8883**).

---

## 📌 Fitur Unggulan Sistem

1. **100% Offline Edge AI (Random Forest)**:
   - Menjalankan inferensi Machine Learning langsung di dalam memori ESP32 tanpa butuh koneksi internet.
   - Jendela sampling 5 detik (50 sampel pada 10 Hz) mengekstraksi 8 fitur statistik (*Mean, Std, Min, Max* untuk TDS & Turbidity).
   - Pengujian total berdurasi 30 detik dengan mekanisme *Majority Voting* untuk hasil akhir yang sangat akurat.

2. **Wi-Fi Captive Portal Provisioning (Tanpa Hardcode)**:
   - Jika belum terhubung ke router, ESP32 memancarkan Hotspot setup:
     - **SSID**: `ESP32_WaterAI`
     - **Password**: `12345678`
   - Begitu HP terhubung, otomatis muncul pop-up web dashboard modern untuk memindai Wi-Fi sekitar, memilih jaringan, dan memasukkan password dengan fitur **Lihat Password (👁️ toggle icon)**.
   - Kredensial tersimpan secara permanen di memori Flash NVS (*Non-Volatile Storage*).

3. **Mekanisme Tombol Universal & Anti-Glitch**:
   - Mendukung tipe tombol **Momentary** maupun **Latching (Self-Locking Switch)**: Setiap 1 kali klik selalu memicu aksi secara instan.
   - Fitur **Cancel**: Tekan tombol sekali lagi saat pengujian 30s untuk membatalkan pengujian.
   - **Auto Turn-Off LED**: LED hasil klasifikasi menyala selama 10 detik lalu otomatis padam sendiri untuk menghemat daya.
   - Anti-Glitch Debounce 80 ms mencegah pemicuan palsu akibat gelombang radio / listrik statis.

4. **Konektivitas Ganda (Lokal & Cloud)**:
   - **Lokal (TCP Port 8080)**: Komunikasi cepat latensi rendah dengan laptop via `live_viewer.py`.
   - **Cloud (HiveMQ MQTT TLS Port 8883)**: Streaming telemetri dan kontrol jarak jauh dari mana saja di seluruh dunia via `mqtt_viewer.py`.

---

## 🔌 Pemetaan Pinout PCB (ESP32)

| Komponen / Fungsi | Pin GPIO | Keterangan |
| :--- | :---: | :--- |
| 🔴 **LED Merah** | **GPIO 23 (D23)** | Kategori **High** (Air Kotor / Keruh) |
| 🟡 **LED Kuning** | **GPIO 22 (D22)** | Kategori **Medium** (Air Sedang) |
| 🟢 **LED Hijau** | **GPIO 25 (D25)** | Kategori **Low** (Air Bersih) |
| 🔵 **LED Status** | **GPIO 33 (D33)** | **Solid ON** saat Standby / **Kedip Cepat** saat Uji 30s |
| 🔘 **Push Button** | **GPIO 32 (D32)** | Tombol Mulai Uji / Batalkan Uji (`INPUT_PULLUP`) |
| 💧 **Sensor TDS** | **GPIO 34 (D34)** | Input Analog ADC1 (0 - 3.3V) |
| 🌊 **Sensor Turbidity** | **GPIO 35 (D35)** | Input Analog ADC1 (0 - 3.3V) |

---

## ⚙️ Konfigurasi Kredensial (`.env`)

Untuk script Python, seluruh alamat IP dan kredensial cloud tersimpan di dalam file `.env`:

```env
# ESP32 Local Wi-Fi & TCP IP
ESP_IP=192.168.1.9
ESP_PORT=8080

# HiveMQ Cloud Serverless Broker (TLS Port 8883)
MQTT_BROKER=4ecff5933a704e218192a9a9390c3580.s1.eu.hivemq.cloud
MQTT_PORT=8883
MQTT_USER=Aquasentry
MQTT_PASS=admin123

# ESP32 Captive Portal Hotspot Setup
AP_SSID=ESP32_WaterAI
AP_PASSWORD=12345678
```

---

## 🚀 Panduan Penggunaan

### 1. Upload Firmware ESP32
1. Buka Arduino IDE.
2. Pastikan library **`PubSubClient`** (oleh Nick O'Leary) sudah terpasang:
   - Masuk ke menu **Sketch** ➡️ **Include Library** ➡️ **Manage Libraries...**
   - Cari `PubSubClient`, lalu klik **Install**.
3. Buka folder `esp_firmware_edge_ai/` dan buka file `esp_firmware_edge_ai.ino`.
4. Pilih board **DOIT ESP32 DEVKIT V1** (atau ESP32 Dev Module) dan port COM yang sesuai.
5. Klik **Upload**.

### 2. Hubungkan ESP32 ke Wi-Fi (Captive Portal)
1. Sambungkan HP ke Wi-Fi `ESP32_WaterAI` (Password: `12345678`).
2. Layar pop-up setup akan muncul otomatis (atau buka browser ke `http://192.168.4.1`).
3. Pilih Wi-Fi router Anda, masukkan password, lalu klik **"Hubungkan"**.
4. ESP32 akan memverifikasi koneksi dan menampilkan alamat IP yang didapat (misal `192.168.1.9`).

### 3. Menjalankan Python Injector & Live Viewer

Install dependensi Python terlebih dahulu:
```bash
pip install -r python_injector/requirements.txt
```

#### Opsi A: Melalui Wi-Fi Lokal (TCP Port 8080)
```bash
python python_injector/live_viewer.py
```
- Pilih preset pengujian: `[1] LOW`, `[2] MEDIUM`, `[3] HIGH`.
- Tekan push button di ESP32 untuk memulai hitung mundur 30 detik.
- Pantau data telemetri live di terminal.

#### Opsi B: Melalui HiveMQ Cloud MQTT (Jarak Jauh via Internet)
```bash
python python_injector/mqtt_viewer.py
```
- Menghubungkan secara terenkripsi TLS (Port 8883) ke HiveMQ Cloud.
- Anda dapat menginjeksi preset, memulai tes (`S`), dan membatalkan tes (`C`) dari mana saja di seluruh dunia!
