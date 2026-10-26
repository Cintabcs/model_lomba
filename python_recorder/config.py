# Standard Library Imports
import os

# ESP32 Access Point Configuration
ESP_IP = "192.168.1.9"       # IP aktif ESP32 di router Wi-Fi Rumah
ESP_PORT = 8080             # TCP Data Stream Port

# Sampling & Recording Settings
TARGET_SAMPLING_RATE_HZ = 10 # 10 Hz (10 samples per second)
SAMPLE_INTERVAL_MS = 100     # 100 milliseconds between samples

# Output Directory Settings
BASE_DATA_DIR = os.path.join(os.path.dirname(__file__), "dataset")
