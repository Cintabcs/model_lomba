import sys
import time
import threading
from data_client import ESPClient
from data_saver import DataSaver
import config

def print_banner():
    print("=" * 70)
    print("   ESP32 WATER QUALITY DATA ACQUISITION & RECORDER (10 Hz)   ")
    print("   Sensor: TDS & Turbidity | Mode Wi-Fi Rumah / AP Hotspot    ")
    print("=" * 70)

def main():
    print_banner()
    
    current_ip = config.ESP_IP

    while True:
        print(f"\n--- MENU PEREKAMAN DATA (IP ESP32 Target: {current_ip}) ---")
        print("1. Hubungkan ke ESP32 & Mulai Rekam Data Baru")
        print("2. Ubah Alamat IP ESP32 (Jika pakai Wi-Fi Rumah)")
        print("3. Uji Koneksi ke ESP32")
        print("4. Keluar")
        
        choice = input("\nPilih menu (1-4): ").strip()

        if choice == "2":
            new_ip = input(f"Masukkan IP Address ESP32 yang muncul di Serial Monitor (sekarang '{current_ip}'): ").strip()
            if new_ip:
                current_ip = new_ip
                config.ESP_IP = new_ip
                print(f"[✓] IP ESP32 diubah menjadi: {current_ip}")
            continue

        elif choice == "3":
            client = ESPClient(ip=current_ip)
            success = client.connect()
            if success:
                client.disconnect()
            continue

        elif choice == "4":
            print("Terima kasih! Program selesai.")
            sys.exit(0)

        elif choice == "1":
            print("\nPilih LABEL/KATEGORI data:")
            print("1. low")
            print("2. medium")
            print("3. high")
            lbl_choice = input("Masukkan pilihan (1/2/3) atau ketik manual: ").strip().lower()
            
            if lbl_choice == '1' or lbl_choice == 'low':
                label = 'low'
            elif lbl_choice == '2' or lbl_choice == 'medium':
                label = 'medium'
            elif lbl_choice == '3' or lbl_choice == 'high':
                label = 'high'
            else:
                label = lbl_choice # Allow manual typing fallback
                
            if not label:
                print("[!] Label tidak boleh kosong!")
                continue

            client = ESPClient(ip=current_ip)
            print(f"\n[+] Label terpilih: '{label}'")
            print(f"[+] Menghubungkan ke IP ESP32: {current_ip}...")
            
            # Connect to ESP
            if not client.connect():
                print("[!] Gagal terhubung ke ESP32!")
                print("[Tips] Jika pakai Wi-Fi Rumah: Pastikan ESP32 & Laptop terhubung di Wi-Fi yang sama.")
                print("[Tips] Cek IP ESP32 di Serial Monitor Arduino IDE (Menu nomor 2 untuk ubah IP).")
                continue

            saver = DataSaver(label=label)
            saver.start()

            stop_requested = False

            def stop_check():
                return stop_requested

            def listen_for_exit():
                nonlocal stop_requested
                input("\n>>> TEKAN [ENTER] KAPAN SAJA UNTUK BERHENTI MEREKAM <<<\n\n")
                stop_requested = True

            exit_thread = threading.Thread(target=listen_for_exit, daemon=True)
            exit_thread.start()

            print(f"[*] Merekam data TDS & Turbidity pada 10 Hz...")

            start_time = time.time()

            def on_data_received(data_dict: dict):
                saver.save_sample(data_dict)
                elapsed = time.time() - start_time
                count = saver.recorded_count
                
                tds_ppm = data_dict.get('tds_ppm', '-')
                tds_volt = data_dict.get('tds_voltage', '-')
                turb_ntu = data_dict.get('turbidity_ntu', '-')
                turb_volt = data_dict.get('turbidity_voltage', '-')
                
                status_str = (
                    f"\r[REKAM] #{count:04d} | Waktu: {elapsed:5.1f}s | "
                    f"TDS: {tds_ppm} ppm ({tds_volt}V) | "
                    f"Turbidity: {turb_ntu} NTU ({turb_volt}V) "
                )
                print(status_str, end="", flush=True)

            try:
                client.stream_data(on_data_callback=on_data_received, stop_check=stop_check)
            except KeyboardInterrupt:
                print("\n[!] Recording dihentikan oleh pengguna (Ctrl+C).")
                stop_requested = True
            finally:
                client.disconnect()
                saver.close()
                print(f"[✓] Sesi perekaman label '{label}' berhasil disimpan!\n")

        else:
            print("[!] Pilihan tidak valid.")

if __name__ == "__main__":
    main()
