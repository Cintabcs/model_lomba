"""
=============================================================================
LIVE SENSOR MONITOR VIA WI-FI (TDS & TURBIDITY REALTIME STREAM)
=============================================================================
Menghubungkan Laptop ke ESP32 secara wireless melalui Wi-Fi Rumah:
SSID: MyRepublic_C2222847
Port: 8080
=============================================================================
"""

import sys
import time
import json
import socket
import os

# Default IP dari config.py
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from config import ESP_IP, ESP_PORT

def find_esp_ip(default_ip=ESP_IP, port=ESP_PORT):
    # Tes dulu IP default
    print(f"[+] Mencoba koneksi ke ESP32 di {default_ip}:{port}...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(1.5)
    try:
        s.connect((default_ip, port))
        s.close()
        return default_ip
    except Exception:
        s.close()
    
    print(f"[!] Tidak dapat terhubung ke {default_ip}. Memeriksa subnet 192.168.1.x...")
    prefix = ".".join(default_ip.split(".")[:3])
    # Quick scan beberapa IP umum
    for last_octet in range(2, 25):
        ip = f"{prefix}.{last_octet}"
        if ip == default_ip:
            continue
        test_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        test_sock.settimeout(0.15)
        try:
            test_sock.connect((ip, port))
            test_sock.close()
            print(f"[✓] Ditemukan ESP32 di IP: {ip}!")
            return ip
        except Exception:
            test_sock.close()
            
    manual = input(f"[?] Masukkan IP ESP32 manual (tekan Enter untuk tetap pakai {default_ip}): ").strip()
    return manual if manual else default_ip

def main():
    print("=" * 70)
    print("   ESP32 WIRELESS SENSOR MONITOR (TDS & TURBIDITY LIVE VIEWER)   ")
    print("   Mode Wi-Fi: MyRepublic_C2222847                              ")
    print("=" * 70)

    target_ip = find_esp_ip()

    print(f"\n[+] Menghubungkan ke ESP32 ({target_ip}:{ESP_PORT})...")
    
    while True:
        try:
            client_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client_sock.settimeout(10.0)
            client_sock.connect((target_ip, ESP_PORT))
            print(f"[✓] BERHASIL TERHUBUNG KE ESP32!\n")
            print(" Tekan Ctrl+C kapan saja untuk keluar.\n")
            print("-" * 70)
            print(f"{'SAMPEL':<8} | {'TDS (PPM)':<12} | {'TURBIDITY (NTU)':<17} | {'STATUS / AI':<18} | {'TOMBOL':<8}")
            print("-" * 70)

            buffer = ""
            while True:
                data = client_sock.recv(1024).decode('utf-8', errors='ignore')
                if not data:
                    print("\n[!] Koneksi terputus dari ESP32.")
                    break
                
                buffer += data
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        obj = json.loads(line)
                        sample_id = obj.get("sample_id", 0)
                        tds_ppm = obj.get("tds_ppm", 0.0)
                        tds_v = obj.get("tds_voltage", 0.0)
                        turb_ntu = obj.get("turbidity_ntu", 0.0)
                        turb_v = obj.get("turbidity_voltage", 0.0)
                        ai_class = obj.get("edge_ai_class", "Standby")
                        is_test = obj.get("is_testing", False)

                        btn_status = "UJI 30s" if is_test else "STANDBY"

                        tds_str = f"{tds_ppm:.1f} ({tds_v:.2f}V)"
                        turb_str = f"{turb_ntu:.1f} ({turb_v:.2f}V)"

                        print(f"#{sample_id:<7} | {tds_str:<12} | {turb_str:<17} | {ai_class:<18} | {btn_status:<8}")
                    except json.JSONDecodeError:
                        pass
        except KeyboardInterrupt:
            print("\n\n[✓] Monitor dihentikan oleh pengguna.")
            sys.exit(0)
        except Exception as e:
            print(f"\n[!] Error koneksi: {e}")
            retry = input("Coba konek ulang? (y/n / ganti IP): ").strip().lower()
            if retry == 'n':
                break
            elif "." in retry:
                target_ip = retry

if __name__ == "__main__":
    main()
