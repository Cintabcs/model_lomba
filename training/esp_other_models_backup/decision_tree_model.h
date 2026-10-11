/*
 * =============================================================================
 * EMBEDDED DECISION TREE CLASSIFIER FOR ESP32 (EDGE AI)
 * Generated automatically for 5-Second Window Classification
 * =============================================================================
 */

#ifndef DT_MODEL_H
#define DT_MODEL_H

#include <Arduino.h>

class EdgeWaterClassifier {
public:
    static int predict(const float x[8]) {
        if (x[4] <= 132.1510f) {
            return 0;
        } else {
            if (x[4] <= 339.5690f) {
                return 1;
            } else {
                if (x[3] <= 273.4500f) {
                    if (x[0] <= 150.4830f) {
                        return 2;
                    } else {
                        return 1;
                    }
                } else {
                    return 2;
                }
            }
        }

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

#endif // DT_MODEL_H
