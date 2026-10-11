"""
=============================================================================
C++ HEADER MODEL EXPORTER FOR MULTI-MODEL EDGE AI (ESP32)
=============================================================================
Supports exporting:
1. Decision Tree (Single tree, ultra small Flash footprint ~1.5 KB)
2. Random Forest (Ensemble trees, fast & robust ~8-15 KB)
3. Linear SVM / Logistic Regression (Matrix multiplication, tiny ~0.5 KB)
4. K-Nearest Neighbors / Naive Bayes (Fast C++ loops)
=============================================================================
"""

import os
import numpy as np

def export_decision_tree_to_cpp(clf, feature_names, class_names, output_header_path):
    n_classes = len(class_names)
    tree_structure = clf.tree_
    
    def recurse(node_id, depth):
        indent = "    " * depth
        if tree_structure.feature[node_id] != -2:  # non-leaf node
            feat = tree_structure.feature[node_id]
            thresh = tree_structure.threshold[node_id]
            left = tree_structure.children_left[node_id]
            right = tree_structure.children_right[node_id]
            
            s = f"{indent}if (x[{feat}] <= {thresh:.4f}f) {{\n"
            s += recurse(left, depth + 1)
            s += f"{indent}}} else {{\n"
            s += recurse(right, depth + 1)
            s += f"{indent}}}\n"
            return s
        else:  # leaf node
            vals = tree_structure.value[node_id][0]
            pred_class = int(np.argmax(vals))
            return f"{indent}return {pred_class};\n"

    tree_code = recurse(0, 2)
    
    cpp_code = f"""/*
 * =============================================================================
 * EMBEDDED DECISION TREE CLASSIFIER FOR ESP32 (EDGE AI)
 * Generated automatically for 5-Second Window Classification
 * =============================================================================
 */

#ifndef DT_MODEL_H
#define DT_MODEL_H

#include <Arduino.h>

class EdgeWaterClassifier {{
public:
    static int predict(const float x[8]) {{
{tree_code}
    }}

    static const char* predictLabel(const float x[8]) {{
        int c = predict(x);
        switch (c) {{
            case 0: return "Low";
            case 1: return "Medium";
            case 2: return "High";
            default: return "Unknown";
        }}
    }}
}};

#endif // DT_MODEL_H
"""
    os.makedirs(os.path.dirname(output_header_path), exist_ok=True)
    with open(output_header_path, 'w', encoding='utf-8') as f:
        f.write(cpp_code)
        
    return len(cpp_code)

def export_random_forest_to_cpp(clf, feature_names, class_names, output_header_path):
    n_classes = len(class_names)
    
    cpp_code = f"""/*
 * =============================================================================
 * EMBEDDED RANDOM FOREST CLASSIFIER FOR ESP32 (EDGE AI)
 * Generated automatically for 5-Second Window Classification
 * =============================================================================
 */

#ifndef RF_MODEL_H
#define RF_MODEL_H

#include <Arduino.h>

class EdgeWaterClassifier {{
public:
    static int predict(const float x[8]) {{
        float votes[{n_classes}] = {{0.0f}};
        
"""
    for tree_idx, tree in enumerate(clf.estimators_):
        tree_structure = tree.tree_
        
        def recurse(node_id, depth):
            indent = "    " * depth
            if tree_structure.feature[node_id] != -2:
                feat = tree_structure.feature[node_id]
                thresh = tree_structure.threshold[node_id]
                left = tree_structure.children_left[node_id]
                right = tree_structure.children_right[node_id]
                
                s = f"{indent}if (x[{feat}] <= {thresh:.4f}f) {{\n"
                s += recurse(left, depth + 1)
                s += f"{indent}}} else {{\n"
                s += recurse(right, depth + 1)
                s += f"{indent}}}\n"
                return s
            else:
                vals = tree_structure.value[node_id][0]
                pred_class = int(np.argmax(vals))
                return f"{indent}votes[{pred_class}] += 1.0f;\n"

        cpp_code += f"        // --- Tree {tree_idx} ---\n"
        cpp_code += recurse(0, 2)
        cpp_code += "\n"

    cpp_code += f"""
        int best_class = 0;
        float max_votes = votes[0];
        for (int i = 1; i < {n_classes}; i++) {{
            if (votes[i] > max_votes) {{
                max_votes = votes[i];
                best_class = i;
            }}
        }}
        return best_class; // 0: Low, 1: Medium, 2: High
    }}

    static const char* predictLabel(const float x[8]) {{
        int c = predict(x);
        switch (c) {{
            case 0: return "Low";
            case 1: return "Medium";
            case 2: return "High";
            default: return "Unknown";
        }}
    }}
}};

#endif // RF_MODEL_H
"""
    os.makedirs(os.path.dirname(output_header_path), exist_ok=True)
    with open(output_header_path, 'w', encoding='utf-8') as f:
        f.write(cpp_code)
        
    return len(cpp_code)

def export_linear_svm_to_cpp(clf, feature_names, class_names, output_header_path):
    n_classes = len(class_names)
    n_features = len(feature_names)
    
    coef = clf.coef_  # shape (n_classes, n_features) or (1, n_features)
    intercept = clf.intercept_
    
    if coef.shape[0] == 1:
        # Binary or single row expanded
        coef_str = "{" + ", ".join([f"{c:.4f}f" for c in coef[0]]) + "}"
    else:
        coef_rows = []
        for row in coef:
            coef_rows.append("{" + ", ".join([f"{c:.4f}f" for c in row]) + "}")
        coef_str = "{\n            " + ",\n            ".join(coef_rows) + "\n        }"
        
    intercept_str = "{" + ", ".join([f"{i:.4f}f" for i in intercept]) + "}"
    
    cpp_code = f"""/*
 * =============================================================================
 * EMBEDDED LINEAR SVM CLASSIFIER FOR ESP32 (EDGE AI)
 * Generated automatically for 5-Second Window Classification
 * =============================================================================
 */

#ifndef SVM_MODEL_H
#define SVM_MODEL_H

#include <Arduino.h>

class EdgeWaterClassifier {{
public:
    static int predict(const float x[8]) {{
        static const float weights[{n_classes}][{n_features}] = {coef_str};
        static const float bias[{n_classes}] = {intercept_str};

        int best_class = 0;
        float max_score = -999999.0f;

        for (int c = 0; c < {n_classes}; c++) {{
            float score = bias[c];
            for (int j = 0; j < {n_features}; j++) {{
                score += weights[c][j] * x[j];
            }}
            if (score > max_score) {{
                max_score = score;
                best_class = c;
            }}
        }}
        return best_class;
    }}

    static const char* predictLabel(const float x[8]) {{
        int c = predict(x);
        switch (c) {{
            case 0: return "Low";
            case 1: return "Medium";
            case 2: return "High";
            default: return "Unknown";
        }}
    }}
}};

#endif // SVM_MODEL_H
"""
    os.makedirs(os.path.dirname(output_header_path), exist_ok=True)
    with open(output_header_path, 'w', encoding='utf-8') as f:
        f.write(cpp_code)
        
    return len(cpp_code)

def export_knn_to_cpp(clf, feature_names, class_names, output_header_path):
    X_train = clf._fit_X
    y_train = clf._y
    n_samples, n_features = X_train.shape
    n_classes = len(class_names)
    k = clf.n_neighbors

    # Format training exemplars into C++ array
    samples_rows = []
    for row in X_train:
        samples_rows.append("{" + ", ".join([f"{v:.3f}f" for v in row]) + "}")
    samples_str = "{\n            " + ",\n            ".join(samples_rows) + "\n        }"
    labels_str = "{" + ", ".join([str(lbl) for lbl in y_train]) + "}"

    cpp_code = f"""/*
 * =============================================================================
 * EMBEDDED K-NEAREST NEIGHBORS (KNN) CLASSIFIER FOR ESP32 (EDGE AI)
 * Generated automatically for 5-Second Window Classification
 * =============================================================================
 */

#ifndef KNN_MODEL_H
#define KNN_MODEL_H

#include <Arduino.h>

class EdgeWaterClassifier {{
public:
    static int predict(const float x[8]) {{
        static const float samples[{n_samples}][{n_features}] = {samples_str};
        static const int labels[{n_samples}] = {labels_str};

        float dists[{n_samples}];
        for (int i = 0; i < {n_samples}; i++) {{
            float d = 0;
            for (int j = 0; j < {n_features}; j++) {{
                float diff = x[j] - samples[i][j];
                d += diff * diff;
            }}
            dists[i] = d;
        }}

        // Simple K-Nearest Vote (k={k})
        int votes[{n_classes}] = {{0}};
        for (int step = 0; step < {k}; step++) {{
            float min_d = 9999999.0f;
            int min_idx = 0;
            for (int i = 0; i < {n_samples}; i++) {{
                if (dists[i] < min_d) {{
                    min_d = dists[i];
                    min_idx = i;
                }}
            }}
            votes[labels[min_idx]]++;
            dists[min_idx] = 99999999.0f; // mark visited
        }}

        int best_class = 0;
        int max_v = votes[0];
        for (int c = 1; c < {n_classes}; c++) {{
            if (votes[c] > max_v) {{
                max_v = votes[c];
                best_class = c;
            }}
        }}
        return best_class;
    }}

    static const char* predictLabel(const float x[8]) {{
        int c = predict(x);
        switch (c) {{
            case 0: return "Low";
            case 1: return "Medium";
            case 2: return "High";
            default: return "Unknown";
        }}
    }}
}};

#endif // KNN_MODEL_H
"""
    os.makedirs(os.path.dirname(output_header_path), exist_ok=True)
    with open(output_header_path, 'w', encoding='utf-8') as f:
        f.write(cpp_code)
        
    return len(cpp_code)
