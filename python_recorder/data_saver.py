import os
import csv
from datetime import datetime
from config import BASE_DATA_DIR

class DataSaver:
    def __init__(self, label: str):
        self.label = label
        self.ensure_directory()
        
        # Format filename: data_<label>_<YYYYMMDD_HHMMSS>.csv
        timestamp_str = datetime.now().strftime("%Y%m%d_%H%M%S")
        safe_label = "".join(c for c in label if c.isalnum() or c in ("_", "-")).rstrip()
        if not safe_label:
            safe_label = "unlabeled"
            
        self.filename = f"data_{safe_label}_{timestamp_str}.csv"
        self.filepath = os.path.join(BASE_DATA_DIR, self.filename)
        self.file_handle = None
        self.writer = None
        self.recorded_count = 0

    def ensure_directory(self):
        """Creates dataset output directory if it does not exist."""
        if not os.path.exists(BASE_DATA_DIR):
            os.makedirs(BASE_DATA_DIR)

    def start(self, fieldnames=None):
        """Opens the CSV file and writes header."""
        if fieldnames is None:
            fieldnames = [
                "pc_timestamp", 
                "esp_timestamp_ms", 
                "label", 
                "sample_id", 
                "tds_raw", 
                "tds_voltage", 
                "tds_ppm", 
                "turbidity_raw", 
                "turbidity_voltage", 
                "turbidity_ntu"
            ]
            
        self.fieldnames = fieldnames
        self.file_handle = open(self.filepath, mode='w', newline='', encoding='utf-8')
        self.writer = csv.DictWriter(self.file_handle, fieldnames=self.fieldnames)
        self.writer.writeheader()
        self.file_handle.flush()
        print(f"[Saver] File CSV dibuat: {self.filepath}")

    def save_sample(self, data_dict: dict):
        """Saves a single data row to CSV file."""
        if not self.writer:
            return

        # Add PC timestamp if not present
        if "pc_timestamp" not in data_dict:
            data_dict["pc_timestamp"] = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        
        # Inject label
        data_dict["label"] = self.label

        # Ensure all columns exist
        row = {k: data_dict.get(k, "") for k in self.fieldnames}
        self.writer.writerow(row)
        self.file_handle.flush()
        self.recorded_count += 1

    def close(self):
        """Closes the CSV file."""
        if self.file_handle:
            self.file_handle.close()
            print(f"\n[Saver] Selesai merekam. Total data tersimpan: {self.recorded_count} baris -> {self.filepath}")
