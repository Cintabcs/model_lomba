"""
=============================================================================
SCRIPT VISUALISASI SCATTER PLOT 3 KELAS (TDS VS TURBIDITY)
=============================================================================
Menghasilkan 2 varian gambar resolusi tinggi (300 DPI):
1. Dark Theme  (Background Hitam)
2. Light Theme (Background Putih / untuk Paper & Skripsi)
=============================================================================
"""

import os
import glob
import pandas as pd
import matplotlib.pyplot as plt

def load_data():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    dataset_dir = os.path.join(base_dir, "..", "python_recorder", "dataset")
    
    low_files = glob.glob(os.path.join(dataset_dir, "data_low*.csv")) + glob.glob(os.path.join(dataset_dir, "dummy", "dummy_low*.csv"))
    med_files = glob.glob(os.path.join(dataset_dir, "data_medium*.csv")) + glob.glob(os.path.join(dataset_dir, "dummy", "dummy_medium*.csv"))
    high_files = glob.glob(os.path.join(dataset_dir, "data_high*.csv")) + glob.glob(os.path.join(dataset_dir, "dummy", "dummy_high*.csv"))
    
    print(f"[+] Membaca {len(low_files)} file Low, {len(med_files)} file Medium, {len(high_files)} file High...")
    
    df_low = pd.concat([pd.read_csv(f) for f in low_files], ignore_index=True)
    df_med = pd.concat([pd.read_csv(f) for f in med_files], ignore_index=True)
    df_high = pd.concat([pd.read_csv(f) for f in high_files], ignore_index=True)
    
    # Filter noise pembacaan 0 saat inisialisasi sensor
    df_low = df_low[df_low['tds_ppm'] > 5]
    df_med = df_med[df_med['tds_ppm'] > 5]
    df_high = df_high[df_high['tds_ppm'] > 5]
    
    return df_low, df_med, df_high

def generate_dark_plot(df_low, df_med, df_high, save_paths):
    plt.close('all')
    plt.style.use('dark_background')
    fig, ax = plt.subplots(figsize=(11, 7.5), dpi=300)

    ax.scatter(df_low['tds_ppm'], df_low['turbidity_ntu'], color='#00ff44', alpha=0.25, s=8, label='Low', marker='o')
    ax.scatter(df_med['tds_ppm'], df_med['turbidity_ntu'], color='#ff9900', alpha=0.35, s=12, label='Medium', marker='x')
    ax.scatter(df_high['tds_ppm'], df_high['turbidity_ntu'], color='#ff2255', alpha=0.35, s=12, label='High', marker='^')

    ax.set_title('Distribusi Data Sensor 3 Kelas: Low vs Medium vs High', fontsize=15, pad=15, fontweight='bold', color='white')
    ax.set_xlabel('TDS (ppm)', fontsize=12, labelpad=10, color='white')
    ax.set_ylabel('Turbidity (NTU)', fontsize=12, labelpad=10, color='white')

    ax.set_xlim(0, 560)
    ax.set_ylim(-10, 600)

    ax.grid(True, linestyle='--', alpha=0.2, color='#888888')
    legend = ax.legend(loc='upper left', frameon=True, facecolor='#161b22', edgecolor='#30363d', fontsize=11, markerscale=1.8)
    for lh in legend.legend_handles:
        lh.set_alpha(1.0)
    for text in legend.get_texts():
        text.set_color('white')

    for p in save_paths:
        plt.savefig(p, bbox_inches='tight', facecolor=fig.get_facecolor())
        print(f"[OK] Berhasil menyimpan Dark plot ke: {p}")
    plt.close(fig)

def generate_light_plot(df_low, df_med, df_high, save_paths):
    plt.close('all')
    plt.style.use('default')
    fig, ax = plt.subplots(figsize=(11, 7.5), dpi=300, facecolor='white')

    # Palet warna tajam untuk latar putih (Emerald Green, Amber Orange, Crimson Red)
    ax.scatter(df_low['tds_ppm'], df_low['turbidity_ntu'], color='#059669', alpha=0.30, s=8, label='Low', marker='o')
    ax.scatter(df_med['tds_ppm'], df_med['turbidity_ntu'], color='#d97706', alpha=0.40, s=12, label='Medium', marker='x')
    ax.scatter(df_high['tds_ppm'], df_high['turbidity_ntu'], color='#dc2626', alpha=0.35, s=12, label='High', marker='^')

    ax.set_title('Distribusi Data Sensor 3 Kelas: Low vs Medium vs High', fontsize=15, pad=15, fontweight='bold', color='#111827')
    ax.set_xlabel('TDS (ppm)', fontsize=12, labelpad=10, color='#1f2937', fontweight='semibold')
    ax.set_ylabel('Turbidity (NTU)', fontsize=12, labelpad=10, color='#1f2937', fontweight='semibold')

    ax.set_xlim(0, 560)
    ax.set_ylim(-10, 600)

    ax.tick_params(colors='#1f2937', labelsize=10)
    ax.grid(True, linestyle='--', alpha=0.5, color='#e5e7eb')
    
    # Legend bersih profesional dengan icon tebal jelas
    legend = ax.legend(loc='upper left', frameon=True, facecolor='#ffffff', edgecolor='#cbd5e1', fontsize=11, markerscale=1.8)
    for lh in legend.legend_handles:
        lh.set_alpha(1.0)
    for text in legend.get_texts():
        text.set_color('#1e293b')

    # Border frame grafik
    for spine in ax.spines.values():
        spine.set_color('#94a3b8')
        spine.set_linewidth(1.0)

    for p in save_paths:
        plt.savefig(p, bbox_inches='tight', facecolor='white')
        print(f"[OK] Berhasil menyimpan Light plot ke: {p}")
    plt.close(fig)

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.abspath(os.path.join(base_dir, ".."))
    
    df_low, df_med, df_high = load_data()
    
    # Path tujuan Dark Theme
    dark_paths = [
        os.path.join(root_dir, "scatter_plot_3_kelas.png"),
        os.path.join(root_dir, "scatter_plot_dark.png"),
        os.path.join(base_dir, "scatter_plot_dark.png")
    ]
    
    # Path tujuan Light Theme
    light_paths = [
        os.path.join(root_dir, "scatter_plot_light.png"),
        os.path.join(base_dir, "scatter_plot_light.png")
    ]
    
    print("\n--- Generating Dark Theme Plot ---")
    generate_dark_plot(df_low, df_med, df_high, dark_paths)
    
    print("\n--- Generating Light Theme Plot ---")
    generate_light_plot(df_low, df_med, df_high, light_paths)
    
    print("\n[SELESAI] Semua varian grafik berhasil dibuat!")

if __name__ == "__main__":
    main()
