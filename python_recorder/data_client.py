import socket
import json
import time
from typing import Callable, Optional
from config import ESP_IP, ESP_PORT

class ESPClient:
    def __init__(self, ip: str = ESP_IP, port: int = ESP_PORT):
        self.ip = ip
        self.port = port
        self.socket: Optional[socket.socket] = None
        self.is_connected = False

    def connect(self, timeout: int = 5) -> bool:
        """Connects to the ESP Hotspot AP socket server."""
        try:
            print(f"[Client] Menghubungkan ke ESP32 ({self.ip}:{self.port})...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(timeout)
            self.socket.connect((self.ip, self.port))
            self.is_connected = True
            print(f"[Client] Berhasil terhubung ke Hotspot ESP32!")
            return True
        except Exception as e:
            print(f"[Client] Gagal terhubung ke {self.ip}:{self.port} - Error: {e}")
            self.is_connected = False
            return False

    def stream_data(self, on_data_callback: Callable[[dict], None], stop_check: Callable[[], bool]):
        """
        Receives lines of JSON stream from ESP32 at 10 Hz and invokes callback for each line.
        """
        if not self.socket or not self.is_connected:
            print("[Client] Socket tidak terhubung.")
            return

        buffer = ""
        self.socket.settimeout(1.0) # non-blocking timeout for clean exit check

        while not stop_check():
            try:
                chunk = self.socket.recv(1024).decode('utf-8', errors='ignore')
                if not chunk:
                    print("[Client] Koneksi terputus oleh ESP32.")
                    break

                buffer += chunk
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue

                    try:
                        # Attempt parsing JSON data sent by ESP
                        data_json = json.loads(line)
                        on_data_callback(data_json)
                    except json.JSONDecodeError:
                        # Fallback for CSV format if ESP sends CSV
                        parts = line.split(",")
                        if len(parts) >= 2:
                            data_fallback = {
                                "esp_timestamp_ms": parts[0],
                                "sample_id": parts[1] if len(parts) > 1 else "0",
                                "tds_ppm": parts[2] if len(parts) > 2 else "0",
                                "turbidity_ntu": parts[3] if len(parts) > 3 else "0",
                            }
                            on_data_callback(data_fallback)
            except socket.timeout:
                continue
            except Exception as e:
                print(f"[Client] Error saat menerima data: {e}")
                break

    def disconnect(self):
        """Disconnects the socket."""
        self.is_connected = False
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
            print("[Client] Koneksi dengan ESP32 ditutup.")
