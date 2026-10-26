"""
=============================================================================
ESP32 EDGE AI OPERATOR ASSISTANT & TDS INJECTOR (WITH CUE WORKFLOW)
=============================================================================
Fitur:
1. Menghubungkan Laptop ke ESP32 secara wireless via Wi-Fi Rumah.
2. Membaca pembacaan sensor Turbidity fisik asli dari ESP32.
3. Memungkinkan operator memilih kategori (Low, Medium, High) untuk
   mengintervensi/menyuntikkan nilai TDS via Wi-Fi ke ESP32.
4. Memberikan CUE / ABA-ABA yang jelas saat sistem sudah aman.
5. Memantau progres pengujian 30 detik & hasil inferensi AI secara realtime!
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

# State Global
current_state = {
    "connected": False,
    "sample_id": 0,
    "tds_ppm": 0.0,
    "turbidity_ntu": 0.0,
    "turbidity_v": 0.0,
    "is_testing": False,
    "test_elapsed_sec": 0,
    "edge_ai_class": "Standby",
    "override_active": False
}

client_socket = None
lock = threading.Lock()
stop_flag = False

def find_esp_ip(default_ip=ESP_IP, port=ESP_PORT):
    print(f"[+] Mencoba koneksi ke ESP32 di {default_ip}:{port}...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(1.2)
    try:
        s.connect((default_ip, port))
        s.close()
        return default_ip
    except Exception:
        s.close()
    
    print(f"[!] Menelusuri subnet Wi-Fi untuk mendeteksi ESP32...")
    prefix = ".".join(default_ip.split(".")[:3])
    for last_octet in range(2, 25):
        ip = f"{prefix}.{last_octet}"
        if ip == default_ip:
            continue
        test_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        test_sock.settimeout(0.12)
        try:
            test_sock.connect((ip, port))
            test_sock.close()
            print(f"[✓] Ditemukan ESP32 di IP: {ip}!")
            return ip
        except Exception:
            test_sock.close()
            
    manual = input(f"[?] Masukkan IP ESP32 manual (Enter untuk {default_ip}): ").strip()
    return manual if manual else default_ip

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
                        current_state["sample_id"] = obj.get("sample_id", 0)
                        current_state["tds_ppm"] = obj.get("tds_ppm", 0.0)
                        current_state["turbidity_ntu"] = obj.get("turbidity_ntu", 0.0)
                        current_state["turbidity_v"] = obj.get("turbidity_voltage", 0.0)
                        current_state["is_testing"] = obj.get("is_testing", False)
                        current_state["test_elapsed_sec"] = obj.get("test_elapsed_sec", 0)
                        current_state["edge_ai_class"] = obj.get("edge_ai_class", "Standby")
                        current_state["override_active"] = obj.get("override_active", False)
                except json.JSONDecodeError:
                    pass
        except socket.timeout:
            continue
        except Exception:
            with lock:
                current_state["connected"] = False
            time.sleep(0.5)

def send_tds_injection(value):
    global client_socket
    if client_socket:
        try:
            cmd = f"SET_TDS:{value:.1f}\n"
            client_socket.sendall(cmd.encode('utf-8'))
            return True
        except Exception as e:
            print(f"[!] Gagal mengirim injeksi: {e}")
            return False
    return False

def wait_for_test_session():
    """Memantau sesi pengujian 30 detik saat tombol di PCB ditekan."""
    print("\n" + "=" * 70)
    print(" 📢 [CUE / ABA-ABA UNTUKMU]:")
    print(" >> SISTEM SUDAH SIAP & AMAN! <<")
    print(" >> Celupkan probe sensor ke air, lalu TEKAN PUSH BUTTON (D32) di ESP32! <<")
    print("=" * 70)
    print(" Menunggu kamu menekan tombol di ESP32... (Tekan Ctrl+C untuk kembali ke menu)\n")

    # Tunggu sampai tombol ditekan (is_testing menjadi True)
    try:
        while True:
            with lock:
                testing = current_state["is_testing"]
                turb = current_state["turbidity_ntu"]
                tds = current_state["tds_ppm"]
            
            if testing:
                print("\n" + "#" * 70)
                print(" 🎉 [TOMBOL TERDETEKSI DITEKAN!] PENGUJIAN 30 DETIK SEDANG BERJALAN...")
                print(" Status Fisik PCB: LED Biru (D33) Sedang Berkedip Cepat!")
                print("#" * 70)
                break
            
            # Tampilkan nilai sensor realtime saat menunggu
            sys.stdout.write(f"\r [STANDBY] Turbidity Asli: {turb:5.1f} NTU | TDS Injeksi: {tds:5.1f} PPM | Menunggu tombol...")
            sys.stdout.flush()
            time.sleep(0.2)

        # Sekarang sedang dalam pengujian 30 detik
        last_sec = -1
        while True:
            with lock:
                testing = current_state["is_testing"]
                elapsed = current_state["test_elapsed_sec"]
                turb = current_state["turbidity_ntu"]
                tds = current_state["tds_ppm"]
                ai_class = current_state["edge_ai_class"]

            if not testing:
                # Selesai 30 detik!
                print("\n\n" + "=" * 70)
                print(f" [✓ SELESAI!] PENGUJIAN 30 DETIK TELAH TUNTAS!")
                print(f" HASIL AKHIR TERKUNCI : [{ai_class.upper()}]")
                if ai_class == "Low":
                    print(" Status LED Fisik PCB : LED HIJAU (D25) TETAP MENYALA!")
                elif ai_class == "Medium":
                    print(" Status LED Fisik PCB : LED KUNING (D22) TETAP MENYALA!")
                elif ai_class == "High":
                    print(" Status LED Fisik PCB : LED MERAH (D23) TETAP MENYALA!")
                print(" Status LED Status    : LED Biru (D33) Kembali Solid ON.")
                print("=" * 70)
                input("\nTekan Enter untuk kembali ke Menu Pilihan...")
                break

            if elapsed != last_sec:
                last_sec = elapsed
                sisa = max(0, 30 - elapsed)
                led_info = ""
                if ai_class == "Low": led_info = "[LED Hijau]"
                elif ai_class == "Medium": led_info = "[LED Kuning]"
                elif ai_class == "High": led_info = "[LED Merah]"

                print(f" ⏳ Waktu: {elapsed:2d}s / 30s (Sisa {sisa:2d}s) | Turb: {turb:5.1f} NTU | TDS: {tds:5.1f} PPM => AI: {ai_class:<8} {led_info}")
            
            time.sleep(0.1)

    except KeyboardInterrupt:
        print("\n[!] Kembali ke menu utama.")

def main():
    global client_socket, stop_flag

    print("=" * 70)
    print("   ESP32 EDGE AI - OPERATOR ASSISTANT & TDS INJECTOR   ")
    print("   Mode Wi-Fi: MyRepublic_C2222847 | Port: 8080        ")
    print("=" * 70)

    target_ip = find_esp_ip()

    try:
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.settimeout(3.0)
        client_socket.connect((target_ip, ESP_PORT))
        client_socket.settimeout(0.5)
        print(f"[✓] Berhasil terhubung ke ESP32 ({target_ip}:{ESP_PORT})!\n")
    except Exception as e:
        print(f"[!] Gagal terhubung ke ESP32: {e}")
        print("Pastikan ESP32 sudah menyala dan terhubung ke Wi-Fi yang sama.")
        return

    # Jalankan background receiver
    t = threading.Thread(target=receive_thread, daemon=True)
    t.start()

    time.sleep(1.0) # Beri waktu menerima data awal

    try:
        while True:
            with lock:
                turb = current_state["turbidity_ntu"]
                tds = current_state["tds_ppm"]
                override = current_state["override_active"]

            inj_status = f"{tds:.1f} PPM" if override else "Sensor Fisik (0.0 PPM / Rusak)"

            print("\n" + "=" * 70)
            print("                 PILIH KATEGORI TDS UNTUK DIUJI")
            print("=" * 70)
            print(f" [Sensor Turbidity Asli] : {turb:.1f} NTU (Membaca air fisik!)")
            print(f" [Status TDS Saat Ini]  : {inj_status}")
            print("-" * 70)
            print(" [1] Preset LOW    (Air Bersih / PDAM) -> Injeksi TDS  85.0 PPM")
            print(" [2] Preset MEDIUM (Air Sedang)        -> Injeksi TDS 245.0 PPM")
            print(" [3] Preset HIGH   (Air Kotor / Keruh) -> Injeksi TDS 480.0 PPM")
            print(" [4] Input Nilai TDS Manual (Ketik Angka)")
            print(" [Q] Keluar dari Program")
            print("=" * 70)

            choice = input(" Masukkan pilihanmu (1/2/3/4/Q): ").strip().lower()

            if choice == "1":
                val = 85.0
                send_tds_injection(val)
                print(f"\n[✓] Injeksi Berhasil: TDS diset ke {val} PPM (Air Bersih)")
                wait_for_test_session()

            elif choice == "2":
                val = 245.0
                send_tds_injection(val)
                print(f"\n[✓] Injeksi Berhasil: TDS diset ke {val} PPM (Air Sedang)")
                wait_for_test_session()

            elif choice == "3":
                val = 480.0
                send_tds_injection(val)
                print(f"\n[✓] Injeksi Berhasil: TDS diset ke {val} PPM (Air Kotor)")
                wait_for_test_session()

            elif choice == "4":
                raw_val = input(" Masukkan angka TDS yang diinginkan (contoh: 150): ").strip()
                try:
                    val = float(raw_val)
                    send_tds_injection(val)
                    print(f"\n[✓] Injeksi Berhasil: TDS diset ke {val:.1f} PPM")
                    wait_for_test_session()
                except ValueError:
                    print("[!] Angka tidak valid!")

            elif choice == "q":
                print("\n[✓] Keluar. Terima kasih bro!")
                break
            else:
                print("[!] Pilihan tidak dikenali. Pilih 1, 2, 3, 4, atau Q.")

    except KeyboardInterrupt:
        print("\n\n[✓] Program dihentikan.")
    finally:
        stop_flag = True
        if client_socket:
            client_socket.close()

if __name__ == "__main__":
    main()
