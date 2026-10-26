"""
=============================================================================
ESP32 EDGE AI OPERATOR ASSISTANT (PRESET LOW / MEDIUM / HIGH INJECTOR)
=============================================================================
Fitur:
1. Hubungkan ke ESP32 secara wireless via Wi-Fi TCP Port 8080.
2. Memilih Preset Kategori (LOW, MEDIUM, HIGH) langsung dari laptop.
3. Memberikan aba-aba/CUE yang jelas saat sistem sudah siap.
4. Mendeteksi tekanan tombol Push Button (D32) di ESP32 dan memantau
   hitung mundur 30 detik serta hasil klasifikasi AI secara realtime!
5. Mendukung pembatalan sesi (Cancel) kapan saja saat tombol ditekan kembali.
=============================================================================
"""

import sys
import os
import time
import json
import socket
import threading

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from config import ESP_IP, ESP_PORT

current_state = {
    "connected": False,
    "sample_id": 0,
    "tds_ppm": 0.0,
    "turbidity_ntu": 0.0,
    "preset": "LOW",
    "is_testing": False,
    "test_elapsed_sec": 0,
    "edge_ai_class": "Standby",
    "cancelled": False
}

client_socket = None
lock = threading.Lock()
stop_flag = False

def connect_to_esp(ip=ESP_IP, port=ESP_PORT, max_attempts=6):
    global client_socket
    print(f"\n[+] Menghubungkan ke ESP32 di {ip}:{port}...")
    for attempt in range(1, max_attempts + 1):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(2.0)
            s.connect((ip, port))
            s.settimeout(0.5)
            client_socket = s
            print(f"[✓] BERHASIL TERHUBUNG KE ESP32 ({ip}:{port})!\n")
            return True
        except Exception:
            sys.stdout.write(f"\r[.] Menunggu ESP32 online di {ip}... (Percobaan {attempt}/{max_attempts}) ")
            sys.stdout.flush()
            time.sleep(1.2)
            
    print(f"\n[!] Belum dapat terhubung ke {ip}:{port}.")
    return False

def receive_thread():
    global client_socket, stop_flag
    buffer = ""
    while not stop_flag:
        if not client_socket:
            time.sleep(0.1)
            continue
        try:
            data = client_socket.recv(1024).decode('utf-8', errors='ignore')
            if not data:
                with lock:
                    current_state["connected"] = False
                time.sleep(0.5)
                continue
            
            buffer += data
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if not line:
                    continue
                try:
                    obj = json.loads(line)
                    with lock:
                        current_state["connected"] = True
                        current_state["sample_id"] = obj.get("sample_id", current_state["sample_id"])
                        current_state["tds_ppm"] = obj.get("tds_ppm", current_state["tds_ppm"])
                        current_state["turbidity_ntu"] = obj.get("turbidity_ntu", current_state["turbidity_ntu"])
                        current_state["preset"] = obj.get("preset", current_state["preset"])
                        current_state["is_testing"] = obj.get("is_testing", False)
                        current_state["test_elapsed_sec"] = obj.get("test_elapsed_sec", 0)
                        current_state["edge_ai_class"] = obj.get("edge_ai_class", current_state["edge_ai_class"])
                        current_state["cancelled"] = obj.get("cancelled", False)
                except json.JSONDecodeError:
                    pass
        except socket.timeout:
            continue
        except Exception:
            with lock:
                current_state["connected"] = False
            time.sleep(0.5)

def send_command(cmd_str):
    global client_socket
    if client_socket:
        try:
            client_socket.sendall((cmd_str + "\n").encode('utf-8'))
            return True
        except Exception as e:
            print(f"[!] Gagal kirim perintah: {e}")
            return False
    return False

def wait_for_test_session(selected_name):
    """Menampilkan Cue dan memantau sesi pengujian 30 detik."""
    print("\n" + "=" * 70)
    print(f" [✓] PRESET BERHASIL DIKIRIM KE ESP32 : [{selected_name}]")
    print(" 📢 [CUE / ABA-ABA UNTUKMU]:")
    print(" >> SISTEM SUDAH SIAP & AMAN! <<")
    print(" >> Silakan KLIK PUSH BUTTON (D32) di ESP32 sekarang! <<")
    print(" (Catatan: Jika sedang jalan dan ingin cancel, klik tombol sekali lagi)")
    print("=" * 70)
    print(" Menunggu kamu menekan tombol di ESP32... (Tekan Ctrl+C untuk batal)\n")

    try:
        # 1. Menunggu tombol ditekan di PCB
        while True:
            with lock:
                testing = current_state["is_testing"]
                turb = current_state["turbidity_ntu"]
                tds = current_state["tds_ppm"]
            
            if testing:
                print("\n" + "#" * 70)
                print(" 🎉 [TOMBOL DITEKAN!] PENGUJIAN 30 DETIK RESMI DIMULAI!")
                print(" Status Fisik PCB: LED Biru (D33) Sedang Berkedip Cepat!")
                print(" Tips: Klik tombol sekali lagi jika ingin membatalkan pengujian.")
                print("#" * 70)
                break
            
            sys.stdout.write(f"\r [STANDBY] Simulasi TDS: {tds:5.1f} PPM | Turbidity: {turb:5.1f} NTU | Menunggu tombol...   ")
            sys.stdout.flush()
            time.sleep(0.2)

        # 2. Proses pengujian 30 detik
        last_sec = -1
        while True:
            with lock:
                testing = current_state["is_testing"]
                elapsed = current_state["test_elapsed_sec"]
                turb = current_state["turbidity_ntu"]
                tds = current_state["tds_ppm"]
                ai_class = current_state["edge_ai_class"]
                was_cancelled = current_state.get("cancelled", False)

            if not testing:
                if was_cancelled:
                    print("\n\n" + "=" * 70)
                    print(" 🛑 [DIBATALKAN] Pengujian 30 detik dibatalkan oleh pengguna!")
                    print(" Status Fisik PCB : LED Hasil Padam & LED Biru Kembali Solid ON.")
                    print("=" * 70)
                    time.sleep(1.0)
                    break

                # Selesai 30 detik normal!
                print("\n\n" + "=" * 70)
                print(f" [✓ SELESAI!] PENGUJIAN 30 DETIK TELAH TUNTAS!")
                print(f" HASIL AKHIR TERKUNCI : [{ai_class.upper()}]")
                if ai_class == "Low":
                    print(" Status LED Fisik PCB : 🟢 LED HIJAU (D25) Menyala 10 Detik (Auto-Off)!")
                elif ai_class == "Medium":
                    print(" Status LED Fisik PCB : 🟡 LED KUNING (D22) Menyala 10 Detik (Auto-Off)!")
                elif ai_class == "High":
                    print(" Status LED Fisik PCB : 🔴 LED MERAH (D23) Menyala 10 Detik (Auto-Off)!")
                print(" Status LED Status    : 🔵 LED Biru (D33) Kembali Solid ON.")
                print("=" * 70)
                try:
                    input("\n Tekan Enter untuk kembali ke Menu...")
                except (EOFError, KeyboardInterrupt):
                    pass
                break

            if elapsed != last_sec:
                last_sec = elapsed
                sisa = max(0, 30 - elapsed)
                led_info = ""
                if ai_class == "Low": led_info = "[LED Hijau]"
                elif ai_class == "Medium": led_info = "[LED Kuning]"
                elif ai_class == "High": led_info = "[LED Merah]"

                print(f" ⏳ Waktu: {elapsed:2d}s / 30s (Sisa {sisa:2d}s) | TDS: {tds:5.1f} PPM | Turb: {turb:5.1f} NTU => AI: {ai_class:<8} {led_info}")
            
            time.sleep(0.1)

    except KeyboardInterrupt:
        print("\n[!] Kembali ke menu utama.")

def main():
    global client_socket, stop_flag

    print("=" * 70)
    print("   ESP32 EDGE AI - OPERATOR ASSISTANT (PRESET INJECTOR)   ")
    print(f"   Target ESP32: {ESP_IP} | Port: {ESP_PORT}             ")
    print("=" * 70)

    target_ip = ESP_IP
    while not connect_to_esp(target_ip, ESP_PORT):
        try:
            alt = input(f"[?] Masukkan IP ESP32 (atau Enter untuk coba lagi {target_ip}): ").strip()
            if alt:
                target_ip = alt
        except (EOFError, KeyboardInterrupt):
            print("\n[✓] Keluar.")
            return

    # Background thread penerima data
    t = threading.Thread(target=receive_thread, daemon=True)
    t.start()
    time.sleep(1.0)

    try:
        while True:
            with lock:
                preset = current_state["preset"]
                tds = current_state["tds_ppm"]
                turb = current_state["turbidity_ntu"]

            print("\n" + "=" * 70)
            print("                 PILIH KATEGORI YANG INGIN DIUJI")
            print("=" * 70)
            print(f" [Preset Terpilih Saat Ini] : {preset} (TDS ~{tds:.0f} PPM, Turb ~{turb:.0f} NTU)")
            print("-" * 70)
            print(" [1] LOW    (Air Bersih) -> Target Output AI: 🟢 LED HIJAU  (D25)")
            print(" [2] MEDIUM (Air Sedang) -> Target Output AI: 🟡 LED KUNING (D22)")
            print(" [3] HIGH   (Air Kotor)  -> Target Output AI: 🔴 LED MERAH  (D23)")
            print(" [Q] Keluar dari Program")
            print("=" * 70)

            try:
                choice = input(" Masukkan pilihanmu (1/2/3/Q): ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                print("\n\n[✓] Program dihentikan.")
                break

            if choice == "1":
                send_command("SET_PRESET:LOW")
                wait_for_test_session("LOW (Air Bersih)")

            elif choice == "2":
                send_command("SET_PRESET:MEDIUM")
                wait_for_test_session("MEDIUM (Air Sedang)")

            elif choice == "3":
                send_command("SET_PRESET:HIGH")
                wait_for_test_session("HIGH (Air Kotor)")

            elif choice == "q":
                print("\n[✓] Keluar. Sukses selalu bro!")
                break
            else:
                print("[!] Pilihan tidak dikenali. Ketik 1, 2, atau 3.")

    except KeyboardInterrupt:
        print("\n\n[✓] Program dihentikan.")
    finally:
        stop_flag = True
        if client_socket:
            client_socket.close()

if __name__ == "__main__":
    main()
