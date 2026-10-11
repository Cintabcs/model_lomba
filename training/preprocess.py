"""
=============================================================================
MODULAR PREPROCESSING & DATASET SPLITTING (GROUP-BASED / TEMPORAL LEAK-FREE)
=============================================================================
"""

import os
import glob
import yaml
import numpy as np
import pandas as pd
from sklearn.model_selection import GroupShuffleSplit, train_test_split

def load_config(config_path="config.yaml"):
    with open(config_path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)

def extract_window_features(window_df):
    tds = window_df['tds_ppm'].values
    turb = window_df['turbidity_ntu'].values
    
    features = [
        np.mean(tds), np.std(tds), np.min(tds), np.max(tds),
        np.mean(turb), np.std(turb), np.min(turb), np.max(turb)
    ]
    return features

def prepare_dataset(config_path="config.yaml"):
    cfg = load_config(config_path)
    base_dir = os.path.dirname(os.path.abspath(config_path))
    
    window_size = cfg['dataset']['window_size']
    step_size = cfg['dataset']['step_size']
    label_map = cfg['labels']
    
    X_list = []
    y_list = []
    groups_list = []  # Tracks file ID for Group-based splitting
    
    csv_files = []
    for rel_path in cfg['dataset']['paths']:
        abs_path = os.path.normpath(os.path.join(base_dir, rel_path))
        found = glob.glob(os.path.join(abs_path, "*.csv"))
        csv_files.extend(found)
        
    print(f"[+] Found {len(csv_files)} CSV dataset files.")
    
    file_id = 0
    total_windows = 0
    
    for fpath in csv_files:
        try:
            df = pd.read_csv(fpath)
            # Remove initial noise / disconnected readings
            df = df[df['tds_ppm'] > 5]
            
            if len(df) < window_size:
                continue
                
            labels = df['label'].values
            
            for start_idx in range(0, len(df) - window_size + 1, step_size):
                win_df = df.iloc[start_idx : start_idx + window_size]
                win_labels = labels[start_idx : start_idx + window_size]
                
                # Majority vote label in window
                unique_lbls, counts = np.unique(win_labels, return_counts=True)
                maj_lbl = unique_lbls[np.argmax(counts)]
                
                if maj_lbl not in label_map:
                    continue
                    
                feats = extract_window_features(win_df)
                X_list.append(feats)
                y_list.append(label_map[maj_lbl])
                groups_list.append(file_id)
                total_windows += 1
                
            file_id += 1
            
        except Exception as e:
            print(f"[!] Error reading {fpath}: {e}")
            
    X = np.array(X_list)
    y = np.array(y_list)
    groups = np.array(groups_list)
    
    print(f"[OK] Extracted {total_windows} 5-second windows across {file_id} distinct CSV files.")
    return X, y, groups, cfg

def split_dataset(X, y, groups, cfg):
    split_strategy = cfg['split']['split_strategy']
    test_size = cfg['split']['test_size']
    random_state = cfg['split']['random_state']
    
    print(f"\n--- DATASET SPLITTING (Strategy: {split_strategy}) ---")
    
    if split_strategy == "group_by_file":
        # GroupShuffleSplit ensures entire CSV files go to Train OR Test
        # PREVENTS TEMPORAL DATA LEAKAGE!
        gss = GroupShuffleSplit(n_splits=1, test_size=test_size, random_state=random_state)
        train_idx, test_idx = next(gss.split(X, y, groups))
        
        X_train, X_test = X[train_idx], X[test_idx]
        y_train, y_test = y[train_idx], y[test_idx]
        
        train_files = len(np.unique(groups[train_idx]))
        test_files = len(np.unique(groups[test_idx]))
        print(f"[OK] Group Split Success: Train on {train_files} files ({len(X_train)} windows) | Test on {test_files} UNSEEN files ({len(X_test)} windows)")
        
    else:
        # Standard Random Stratified Split
        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=test_size, random_state=random_state, shuffle=True, stratify=y
        )
        print(f"[OK] Random Stratified Split: Train {len(X_train)} windows | Test {len(X_test)} windows")
        
    return X_train, X_test, y_train, y_test

if __name__ == "__main__":
    X, y, groups, cfg = prepare_dataset()
    X_train, X_test, y_train, y_test = split_dataset(X, y, groups, cfg)
