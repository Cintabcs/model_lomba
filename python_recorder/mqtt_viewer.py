"""
=============================================================================
AQUASENTRY - HIVEMQ CLOUD MQTT LIVE VIEWER & OPERATOR ASSISTANT
=============================================================================
Fitur:
1. Menghubungkan ke Broker HiveMQ Cloud Serverless (Port 8883 TLS).
2. Memantau telemetri real-time (TDS, Turbidity, Prediksi Edge AI).
3. Menampilkan hasil klasifikasi 30 detik (LOW, MEDIUM, HIGH).
4. Mengirim perintah jarak jauh (Preset Injeksi, Start Uji, Cancel Uji).
=============================================================================
"""

import sys
import os
import time
import json
import ssl
import paho.mqtt.client as mqtt

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from config import MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASS

# Topik
TOPIC_TELEMETRY = "aquasentry/water_quality/telemetry"
TOPIC_RESULT    = "aquasentry/water_quality/result"
TOPIC_STATUS    = "aquasentry/status"
TOPIC_COMMAND   = "aquasentry/command"

latest_data = {
    "status": "Menunggu ESP32...",
    "preset": "LOW",
    "tds_ppm": 0.0,
    "turbidity_ntu": 0.0,
    "ai_prediction": "Standby",
    "is_testing": False,
    "elapsed_sec": 0
}

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("\n" + "=" * 70)
        print(" [✓] BERHASIL TERHUBUNG KE HIVEMQ CLOUD (TLS 8883)!")
        print(" Broker: " + MQTT_BROKER)
        print(" Akun  : " + MQTT_USER)
        print("=" * 70)
        client.subscribe([(TOPIC_TELEMETRY, 0), (TOPIC_RESULT, 0), (TOPIC_STATUS, 0)])
    else:
        print(f"[!] Gagal terhubung ke HiveMQ Cloud, kode: {rc}")

def on_message(client, userdata, msg):
    try:
        payload_str = msg.payload.decode('utf-8')
        data = json.loads(payload_str)
        
        if msg.topic == TOPIC_STATUS:
            status_val = data.get("status", "")
            latest_data["status"] = status_val
            if status_val == "online":
                print("\n [📢 CLOUD NOTIF] ESP32 AquaSentry ONLINE di HiveMQ Cloud!")
            elif status_val == "testing_started":
                preset_used = data.get("preset", "LOW")
                print(f"\n [🚀 CLOUD NOTIF] PENGUJIAN 30s DIMULAI DI ESP32! Preset: [{preset_used}]")
            elif status_val == "cancelled":
                print("\n [🛑 CLOUD NOTIF] Pengujian dibatalkan oleh pengguna (Tombol ditekan kembali)!")

        elif msg.topic == TOPIC_TELEMETRY:
            latest_data["is_testing"] = data.get("is_testing", False)
            latest_data["elapsed_sec"] = data.get("elapsed_sec", 0)
            latest_data["preset"] = data.get("preset", "LOW")
            latest_data["tds_ppm"] = data.get("tds_ppm", 0.0)
            latest_data["turbidity_ntu"] = data.get("turbidity_ntu", 0.0)
            latest_data["ai_prediction"] = data.get("ai_prediction", "Standby")

            elapsed = latest_data["elapsed_sec"]
            sisa = max(0, 30 - elapsed)
            tds = latest_data["tds_ppm"]
            turb = latest_data["turbidity_ntu"]
            pred = latest_data["ai_prediction"]

            led_tag = ""
            if pred == "Low": led_tag = "[🟢 LED Hijau]"
            elif pred == "Medium": led_tag = "[🟡 LED Kuning]"
            elif pred == "High": led_tag = "[🔴 LED Merah]"

            print(f" 📡 [MQTT LIVE] Waktu: {elapsed:2d}s/30s (Sisa {sisa:2d}s) | TDS: {tds:5.1f} PPM | Turb: {turb:5.1f} NTU => AI: {pred:<8} {led_tag}")

        elif msg.topic == TOPIC_RESULT:
            verdict = data.get("final_verdict", "Unknown")
            v_low = data.get("vote_low", 0)
            v_med = data.get("vote_medium", 0)
            v_high = data.get("vote_high", 0)
            preset = data.get("preset", "")

            print("\n" + "#" * 70)
            print(f" 🏆 [HASIL AKHIR EDGE AI TERKUNCI DARI CLOUD]: [{verdict.upper()}]")
            print(f" Preset: {preset} | Voting 5s: Low={v_low}, Medium={v_med}, High={v_high}")
            if verdict == "Low":
                print(" Status Fisik PCB: 🟢 LED HIJAU (D25) Menyala 10 Detik!")
            elif verdict == "Medium":
                print(" Status Fisik PCB: 🟡 LED KUNING (D22) Menyala 10 Detik!")
            elif verdict == "High":
                print(" Status Fisik PCB: 🔴 LED MERAH (D23) Menyala 10 Detik!")
            print(" Status LED Status: 🔵 LED Biru (D33) Kembali Solid ON.")
            print("#" * 70 + "\n")

    except Exception as e:
        pass

def main():
    print("=" * 70)
    print("   AQUASENTRY - HIVEMQ CLOUD MQTT DASHBOARD")
    print("   Cloud Broker: " + MQTT_BROKER)
    print("=" * 70)

    # Inisialisasi Paho MQTT Client dengan Callback Version 2
    client_id = f"AquaSentry_Laptop_{int(time.time())}"
    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id)
    except AttributeError:
        client = mqtt.Client(client_id)

    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.tls_set(cert_reqs=ssl.CERT_NONE)
    client.tls_insecure_set(True)

    client.on_connect = on_connect
    client.on_message = on_message

    print(f"[+] Menghubungkan ke HiveMQ Cloud ({MQTT_BROKER}:8883)...")
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
    except Exception as e:
        print(f"[!] Gagal menghubungkan ke HiveMQ Cloud: {e}")
        return

    time.sleep(1.5)

    try:
        while True:
            print("\n" + "-" * 70)
            print(" PILIHAN KONTROL CLOUD MQTT:")
            print(" [1] Kirim Preset LOW    (Air Bersih  -> Target: 🟢 Hijau)")
            print(" [2] Kirim Preset MEDIUM (Air Sedang  -> Target: 🟡 Kuning)")
            print(" [3] Kirim Preset HIGH   (Air Kotor   -> Target: 🔴 Merah)")
            print(" [S] Start Pengujian 30 Detik dari Cloud")
            print(" [C] Cancel / Batalkan Pengujian 30 Detik")
            print(" [Q] Keluar")
            print("-" * 70)

            try:
                pilihan = input(" Masukkan pilihan (1/2/3/S/C/Q): ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                break

            if pilihan == "1":
                client.publish(TOPIC_COMMAND, "SET_PRESET:LOW")
                print(" [✓] Perintah 'SET_PRESET:LOW' berhasil dikirim ke HiveMQ Cloud!")
            elif pilihan == "2":
                client.publish(TOPIC_COMMAND, "SET_PRESET:MEDIUM")
                print(" [✓] Perintah 'SET_PRESET:MEDIUM' berhasil dikirim ke HiveMQ Cloud!")
            elif pilihan == "3":
                client.publish(TOPIC_COMMAND, "SET_PRESET:HIGH")
                print(" [✓] Perintah 'SET_PRESET:HIGH' berhasil dikirim ke HiveMQ Cloud!")
            elif pilihan == "s":
                client.publish(TOPIC_COMMAND, "START_TEST")
                print(" [✓] Perintah 'START_TEST' berhasil dikirim ke HiveMQ Cloud!")
            elif pilihan == "c":
                client.publish(TOPIC_COMMAND, "CANCEL_TEST")
                print(" [✓] Perintah 'CANCEL_TEST' berhasil dikirim ke HiveMQ Cloud!")
            elif pilihan == "q":
                print("\n[✓] Keluar dari MQTT Dashboard.")
                break
            else:
                print("[!] Pilihan tidak dikenali.")

    except KeyboardInterrupt:
        print("\n\n[✓] Program dihentikan.")
    finally:
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()
