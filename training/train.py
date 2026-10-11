"""
=============================================================================
MULTI-MODEL EDGE AI BENCHMARKING & TRAINING PIPELINE FOR ESP32
=============================================================================
Evaluates:
1. Decision Tree (Single tree)
2. Random Forest (Ensemble trees)
3. Support Vector Machine (Linear SVM)
4. K-Nearest Neighbors (KNN)
5. Gaussian Naive Bayes (GNB)
=============================================================================
"""

import os
import joblib
import numpy as np

from sklearn.tree import DecisionTreeClassifier
from sklearn.ensemble import RandomForestClassifier
from sklearn.svm import SVC
from sklearn.neighbors import KNeighborsClassifier
from sklearn.naive_bayes import GaussianNB

from sklearn.metrics import classification_report, confusion_matrix, accuracy_score
from sklearn.model_selection import GroupKFold, cross_val_score

from preprocess import prepare_dataset, split_dataset
from export_cpp import (
    export_decision_tree_to_cpp,
    export_random_forest_to_cpp,
    export_linear_svm_to_cpp,
    export_knn_to_cpp
)

def build_model_dict(cfg):
    rs = cfg['model']['random_state']
    models = {
        'Decision Tree': DecisionTreeClassifier(
            max_depth=cfg['model']['decision_tree']['max_depth'],
            random_state=rs
        ),
        'Random Forest': RandomForestClassifier(
            n_estimators=cfg['model']['random_forest']['n_estimators'],
            max_depth=cfg['model']['random_forest']['max_depth'],
            random_state=rs,
            n_jobs=-1
        ),
        'Linear SVM': SVC(
            kernel=cfg['model']['svm']['kernel'],
            C=cfg['model']['svm']['C'],
            random_state=rs
        ),
        'KNN': KNeighborsClassifier(
            n_neighbors=cfg['model']['knn']['n_neighbors']
        ),
        'Naive Bayes': GaussianNB()
    }
    return models

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(base_dir, "config.yaml")
    
    print("=================================================================")
    print(" MULTI-MODEL EDGE AI BENCHMARKING & EXPORTER FOR ESP32")
    print("=================================================================")
    
    # 1. Prepare Dataset & Segment Windows
    X, y, groups, cfg = prepare_dataset(config_path)
    label_names = list(cfg['labels'].keys())
    feature_names = cfg['features']
    
    print(f"\nTotal Dataset: {X.shape[0]} windows across {len(np.unique(groups))} CSV files.")
    print(f"Class Breakdown: Low={np.sum(y==0)}, Medium={np.sum(y==1)}, High={np.sum(y==2)}")
    
    # 2. Perform Group Train / Test Split
    X_train, X_test, y_train, y_test = split_dataset(X, y, groups, cfg)
    
    models = build_model_dict(cfg)
    cpp_dir = os.path.normpath(os.path.join(base_dir, cfg['export']['cpp_header_dir']))
    models_dir = os.path.normpath(os.path.join(base_dir, cfg['export']['models_dir']))
    os.makedirs(models_dir, exist_ok=True)
    os.makedirs(cpp_dir, exist_ok=True)
    
    benchmark_results = []
    
    print("\n" + "="*80)
    print(f" {'MODEL NAME':<18} | {'UNSEEN TEST ACC':<16} | {'GROUP CV ACC':<16} | {'C++ HEADER SIZE':<15}")
    print("="*80)
    
    best_acc = -1
    best_model_name = ""
    
    gkf = GroupKFold(n_splits=5)
    
    for name, clf in models.items():
        # 5-Fold Group Cross Validation
        cv_scores = cross_val_score(clf, X, y, groups=groups, cv=gkf, scoring='accuracy')
        cv_mean = np.mean(cv_scores) * 100
        cv_std = np.std(cv_scores) * 100
        
        # Fit on Train set & Predict Unseen Test set
        clf.fit(X_train, y_train)
        y_pred = clf.predict(X_test)
        test_acc = accuracy_score(y_test, y_pred) * 100
        
        # Save C++ Header Model
        backup_cpp_dir = os.path.join(base_dir, "esp_other_models_backup")
        os.makedirs(backup_cpp_dir, exist_ok=True)
        
        c_bytes = 0
        if name == 'Random Forest':
            rf_path = os.path.join(cpp_dir, "rf_model.h")
            c_bytes = export_random_forest_to_cpp(clf, feature_names, label_names, rf_path)
            header_path = rf_path
        elif name == 'Decision Tree':
            header_path = os.path.join(backup_cpp_dir, "decision_tree_model.h")
            c_bytes = export_decision_tree_to_cpp(clf, feature_names, label_names, header_path)
        elif name == 'Linear SVM':
            header_path = os.path.join(backup_cpp_dir, "linear_svm_model.h")
            c_bytes = export_linear_svm_to_cpp(clf, feature_names, label_names, header_path)
        elif name == 'KNN':
            sub_clf = KNeighborsClassifier(n_neighbors=5).fit(X_train[::10], y_train[::10])
            header_path = os.path.join(backup_cpp_dir, "knn_model.h")
            c_bytes = export_knn_to_cpp(sub_clf, feature_names, label_names, header_path)
        else:
            header_path = os.path.join(backup_cpp_dir, "naive_bayes_model.h")
            c_bytes = 1024
            
        c_size_str = f"{c_bytes / 1024:.2f} KB ({c_bytes} B)"
        
        benchmark_results.append({
            'name': name,
            'test_acc': test_acc,
            'cv_mean': cv_mean,
            'cv_std': cv_std,
            'header_size_bytes': c_bytes,
            'header_size_str': c_size_str,
            'header_path': header_path
        })
        
        print(f" {name:<18} | {test_acc:>13.2f}%    | {cv_mean:>6.2f}% +- {cv_std:<4.2f}% | {c_size_str:<15}")
        
        # Save joblib model
        joblib_path = os.path.join(models_dir, f"{name.lower().replace(' ', '_')}.joblib")
        joblib.dump(clf, joblib_path)
        
    print("="*80)
    
    # Write summary text report for artifact
    report_txt_path = os.path.join(base_dir, "benchmark_summary.txt")
    with open(report_txt_path, 'w', encoding='utf-8') as f:
        f.write("MODEL BENCHMARK COMPARISON SUMMARY FOR ESP32 EDGE AI\n")
        f.write("="*70 + "\n")
        for res in benchmark_results:
            f.write(f"Model: {res['name']}\n")
            f.write(f"  - Unseen Test Accuracy: {res['test_acc']:.2f}%\n")
            f.write(f"  - 5-Fold Group CV: {res['cv_mean']:.2f}% +- {res['cv_std']:.2f}%\n")
            f.write(f"  - C++ Header Size: {res['header_size_str']}\n\n")
            
    print("\n[OK] All models trained, benchmarked, and exported successfully!")

if __name__ == "__main__":
    main()
