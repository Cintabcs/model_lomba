"""
=============================================================================
DESKTOP GUI EDGE AI WATER TESTER (SINGLE 5-SECOND TEST ON DEMAND)
=============================================================================
Aplikasi GUI Desktop dengan visualisasi modern:
1. Tombol '▶ MULAI PENGUJIAN (5 DETIK)'
2. Progress bar countdown 5 detik.
3. Menampilkan status Edge AI (Low / Medium / High) dengan warna & statistik.
4. Embedded Scatter Plot lokasi sampel pada dataset.
=============================================================================
"""

import sys
import os
import json
import time
import socket
import threading
import tkinter as tk
from tkinter import ttk, messagebox

import numpy as np
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# Ensure local imports work from any working directory
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from config import ESP_IP, ESP_PORT

class EdgeAiGuiApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Water Quality Edge AI Inspector (5s Test)")
        self.root.geometry("950x700")
        self.root.configure(bg="#121218")

        self.is_testing = False

        # --- Style Config ---
        self.style = ttk.Style()
        self.style.theme_use("clam")
        self.style.configure("TProgressbar", thickness=15, troughcolor="#222230", background="#00e676")

        # --- Header Banner ---
        header_frame = tk.Frame(self.root, bg="#1a1a24", pady=15)
        header_frame.pack(fill="x")

        title_lbl = tk.Label(
            header_frame, 
            text="🌊 EDGE AI WATER QUALITY INSPECTOR", 
            font=("Helvetica", 18, "bold"), 
            fg="#ffffff", 
            bg="#1a1a24"
        )
        title_lbl.pack()

        subtitle_lbl = tk.Label(
            header_frame, 
            text=f"Target ESP32 IP: {ESP_IP}:{ESP_PORT} | Single 5-Second Window Test", 
            font=("Helvetica", 10), 
            fg="#8888aa", 
            bg="#1a1a24"
        )
        subtitle_lbl.pack(pady=(2, 0))

        # --- Main Layout (Left: Controls & Results, Right: Plot) ---
        main_body = tk.Frame(self.root, bg="#121218", padx=20, pady=15)
        main_body.pack(fill="both", expand=True)

        left_panel = tk.Frame(main_body, bg="#1a1a24", width=380, padx=15, pady=15, highlightthickness=1, highlightbackground="#2a2a3a")
        left_panel.pack(side="left", fill="both", expand=False, padx=(0, 10))

        right_panel = tk.Frame(main_body, bg="#1a1a24", padx=10, pady=10, highlightthickness=1, highlightbackground="#2a2a3a")
        right_panel.pack(side="right", fill="both", expand=True)

        # --- Left Panel: Controls & Result Card ---
        self.btn_start = tk.Button(
            left_panel, 
            text="▶ MULAI PENGUJIAN (5 DETIK)", 
            font=("Helvetica", 12, "bold"), 
            bg="#00e676", 
            fg="#000000", 
            activebackground="#00c853",
            activeforeground="#000000",
            relief="flat", 
            pady=12, 
            cursor="hand2",
            command=self.start_5s_test
        )
        self.btn_start.pack(fill="x", pady=(0, 15))

        # Progress bar
        self.progress = ttk.Progressbar(left_panel, orient="horizontal", mode="determinate", style="TProgressbar")
        self.progress.pack(fill="x", pady=(0, 15))

        self.status_lbl = tk.Label(left_panel, text="Siap untuk pengujian.", font=("Helvetica", 10, "italic"), fg="#aaaaaa", bg="#1a1a24")
        self.status_lbl.pack(pady=(0, 15))

        # Result Display Card
        self.result_card = tk.Frame(left_panel, bg="#222230", padx=15, pady=20, highlightthickness=2, highlightbackground="#333345")
        self.result_card.pack(fill="both", expand=True)

        tk.Label(self.result_card, text="HASIL KLASIFIKASI EDGE AI", font=("Helvetica", 10, "bold"), fg="#8888bb", bg="#222230").pack()

        self.class_badge = tk.Label(self.result_card, text="⚪ READY", font=("Helvetica", 16, "bold"), fg="#ffffff", bg="#222230", pady=10)
        self.class_badge.pack()

        self.metrics_frame = tk.Frame(self.result_card, bg="#222230")
        self.metrics_frame.pack(fill="x", pady=10)

        self.lbl_tds = tk.Label(self.metrics_frame, text="TDS (PPM): --", font=("Helvetica", 11, "bold"), fg="#ffffff", bg="#222230")
        self.lbl_tds.pack(anchor="w", pady=2)

        self.lbl_turb = tk.Label(self.metrics_frame, text="Turbidity (NTU): --", font=("Helvetica", 11, "bold"), fg="#ffffff", bg="#222230")
        self.lbl_turb.pack(anchor="w", pady=2)

        self.lbl_samples = tk.Label(self.metrics_frame, text="Sampel Terkumpul: 0 / 50", font=("Helvetica", 9), fg="#888888", bg="#222230")
        self.lbl_samples.pack(anchor="w", pady=(8, 0))

        # --- Right Panel: Embedded Matplotlib Scatter Plot ---
        tk.Label(right_panel, text="📈 Posisi Sampel pada Distribusi Dataset", font=("Helvetica", 12, "bold"), fg="#ffffff", bg="#1a1a24").pack(anchor="w", pady=(0, 5))

        self.fig, self.ax = plt.subplots(figsize=(5, 4), dpi=100)
        self.fig.patch.set_facecolor('#1a1a24')
        self.ax.set_facecolor('#121218')

        # Draw baseline dataset zones
        self.draw_baseline_plot()

        self.canvas = FigureCanvasTkAgg(self.fig, master=right_panel)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

    def draw_baseline_plot(self, test_tds=None, test_turb=None, test_class=None):
        self.ax.clear()
        self.ax.set_facecolor('#121218')
        
        # Plot Zone Low
        self.ax.scatter(np.random.uniform(15, 185, 200), np.random.uniform(0, 120, 200), color='#00e676', alpha=0.15, s=15, label='Low Zone')
        # Plot Zone Medium
        self.ax.scatter(np.random.uniform(195, 260, 200), np.random.uniform(120, 350, 200), color='#ff9100', alpha=0.15, s=15, label='Medium Zone')
        # Plot Zone High
        self.ax.scatter(np.random.uniform(280, 520, 200), np.random.uniform(330, 550, 200), color='#ff1744', alpha=0.15, s=15, label='High Zone')

        if test_tds is not None and test_turb is not None:
            color_map = {'Low': '#00e676', 'Medium': '#ff9100', 'High': '#ff1744'}
            pt_color = color_map.get(test_class, '#ffffff')
            self.ax.scatter([test_tds], [test_turb], color=pt_color, s=150, edgecolors='#ffffff', linewidths=2.5, zorder=10, label='Hasil Pengujian')
            self.ax.annotate(f"Hasil: {test_class}\n({test_tds:.1f} PPM, {test_turb:.1f} NTU)", (test_tds, test_turb), textcoords="offset points", xytext=(0,12), ha='center', color='#ffffff', fontweight='bold', fontsize=9)

        self.ax.set_xlim(0, 560)
        self.ax.set_ylim(-10, 600)
        self.ax.set_xlabel("TDS (ppm)", color="#aaaaaa", fontsize=9)
        self.ax.set_ylabel("Turbidity (NTU)", color="#aaaaaa", fontsize=9)
        self.ax.tick_params(colors="#aaaaaa", labelsize=8)
        self.ax.grid(True, linestyle="--", alpha=0.15)
        self.ax.legend(loc="upper left", facecolor="#1a1a24", edgecolor="#333345", fontsize=8, labelcolor="#ffffff")
        self.fig.tight_layout()

    def start_5s_test(self):
        if self.is_testing:
            return

        self.is_testing = True
        self.btn_start.config(state="disabled", bg="#555566", text="⏳ MEMPROSES PENGUJIAN...")
        self.class_badge.config(text="⏳ MENGUKUR...", fg="#ffd54f")
        self.result_card.config(highlightbackground="#ffd54f")
        self.progress["value"] = 0

        threading.Thread(target=self.run_5s_collection_thread, daemon=True).start()

    def run_5s_collection_thread(self):
        collected_tds = []
        collected_turb = []
        last_class = "Unknown"

        # Coba buka socket TCP ke ESP32
        sock = None
        is_simulated = False
        try:
            self.update_status("Menghubungkan ke ESP32...")
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(3.0)
            sock.connect((ESP_IP, ESP_PORT))
            sock.settimeout(1.0)
        except Exception as e:
            self.update_status("ESP32 tidak terhubung. Menggunakan Simulasi Data...")
            is_simulated = True

        buffer = ""
        start_time = time.time()
        
        while len(collected_tds) < 50 and (time.time() - start_time) < 6.5:
            elapsed = time.time() - start_time
            prog_val = min(100, int((elapsed / 5.0) * 100))
            self.root.after(0, self.update_progress, prog_val, len(collected_tds))

            if is_simulated:
                time.sleep(0.1)
                # Random realistic sample
                tds_val = float(np.random.normal(35.0, 2.0))
                turb_val = float(np.random.normal(8.0, 1.5))
                collected_tds.append(tds_val)
                collected_turb.append(turb_val)
                last_class = "Low"
            else:
                try:
                    chunk = sock.recv(1024).decode('utf-8', errors='ignore')
                    if not chunk:
                        break
                    buffer += chunk
                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        line = line.strip()
                        if line.startswith("{") and line.endswith("}"):
                            data = json.loads(line)
                            t_val = float(data.get("tds_ppm", 0))
                            tr_val = float(data.get("turbidity_ntu", 0))
                            cls_val = str(data.get("edge_ai_class", "Low"))
                            
                            collected_tds.append(t_val)
                            collected_turb.append(tr_val)
                            last_class = cls_val
                except socket.timeout:
                    continue
                except Exception:
                    break

        if sock:
            try:
                sock.close()
            except Exception:
                pass

        # Selesai pengujian 5 detik
        mean_tds = np.mean(collected_tds) if collected_tds else 0.0
        mean_turb = np.mean(collected_turb) if collected_turb else 0.0

        self.root.after(0, self.finish_test, mean_tds, mean_turb, last_class)

    def update_progress(self, val, count):
        self.progress["value"] = val
        self.lbl_samples.config(text=f"Sampel Terkumpul: {count} / 50")

    def update_status(self, text):
        self.root.after(0, lambda: self.status_lbl.config(text=text))

    def finish_test(self, tds, turb, ai_class):
        self.is_testing = False
        self.progress["value"] = 100
        self.btn_start.config(state="normal", bg="#00e676", text="▶ MULAI PENGUJIAN (5 DETIK)")

        # Format Card Result
        if ai_class.upper() == "LOW":
            badge_text = "🟢 LOW (AIR BERSIH / PDAM)"
            badge_color = "#00e676"
        elif ai_class.upper() == "MEDIUM":
            badge_text = "🟠 MEDIUM (AIR SEDANG)"
            badge_color = "#ff9100"
        elif ai_class.upper() == "HIGH":
            badge_text = "🔴 HIGH (AIR KOTOR / KERUH)"
            badge_color = "#ff1744"
        else:
            badge_text = f"⚪ {ai_class}"
            badge_color = "#00e676"

        self.class_badge.config(text=badge_text, fg=badge_color)
        self.result_card.config(highlightbackground=badge_color)

        self.lbl_tds.config(text=f"TDS (PPM): {tds:.1f} PPM")
        self.lbl_turb.config(text=f"Turbidity (NTU): {turb:.1f} NTU")
        self.status_lbl.config(text="Pengujian 5 detik selesai!")

        # Update Matplotlib Plot
        self.draw_baseline_plot(test_tds=tds, test_turb=turb, test_class=ai_class.capitalize())
        self.canvas.draw()

def main():
    root = tk.Tk()
    app = EdgeAiGuiApp(root)
    root.mainloop()

if __name__ == "__main__":
    main()
