import os

# Helper pembaca file .env tanpa dependensi tambahan
def load_env_file():
    env_path = os.path.join(os.path.dirname(__file__), ".env")
    if os.path.exists(env_path):
        with open(env_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    key, val = line.split("=", 1)
                    os.environ.setdefault(key.strip(), val.strip())

load_env_file()

# ESP32 Wi-Fi & TCP Configuration
ESP_IP = os.getenv("ESP_IP", "192.168.1.9")
ESP_PORT = int(os.getenv("ESP_PORT", "8080"))

# HiveMQ Cloud MQTT Configuration
MQTT_BROKER = os.getenv("MQTT_BROKER", "4ecff5933a704e218192a9a9390c3580.s1.eu.hivemq.cloud")
MQTT_PORT = int(os.getenv("MQTT_PORT", "8883"))
MQTT_USER = os.getenv("MQTT_USER", "Aquasentry")
MQTT_PASS = os.getenv("MQTT_PASS", "admin123")

# Sampling & Recording Settings
TARGET_SAMPLING_RATE_HZ = 10 # 10 Hz (10 samples per second)
SAMPLE_INTERVAL_MS = 100     # 100 milliseconds between samples

# Output Directory Settings
BASE_DATA_DIR = os.path.join(os.path.dirname(__file__), "dataset")
