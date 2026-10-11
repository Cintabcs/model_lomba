/*
 * =============================================================================
 * EMBEDDED LINEAR SVM CLASSIFIER FOR ESP32 (EDGE AI)
 * Generated automatically for 5-Second Window Classification
 * =============================================================================
 */

#ifndef SVM_MODEL_H
#define SVM_MODEL_H

#include <Arduino.h>

class EdgeWaterClassifier {
public:
    static int predict(const float x[8]) {
        static const float weights[3][8] = {
            {-0.0038f, -0.0000f, -0.0039f, -0.0038f, -0.0043f, -0.0003f, -0.0038f, -0.0048f},
            {-0.0012f, -0.0001f, -0.0008f, -0.0013f, -0.0021f, -0.0003f, -0.0012f, -0.0023f},
            {-0.0137f, -0.0009f, -0.0095f, -0.0126f, -0.0246f, -0.0033f, -0.0116f, -0.0223f}
        };
        static const float bias[3] = {3.5623f, 1.9867f, 30.4154f};

        int best_class = 0;
        float max_score = -999999.0f;

        for (int c = 0; c < 3; c++) {
            float score = bias[c];
            for (int j = 0; j < 8; j++) {
                score += weights[c][j] * x[j];
            }
            if (score > max_score) {
                max_score = score;
                best_class = c;
            }
        }
        return best_class;
    }

    static const char* predictLabel(const float x[8]) {
        int c = predict(x);
        switch (c) {
            case 0: return "Low";
            case 1: return "Medium";
            case 2: return "High";
            default: return "Unknown";
        }
    }
};

#endif // SVM_MODEL_H
