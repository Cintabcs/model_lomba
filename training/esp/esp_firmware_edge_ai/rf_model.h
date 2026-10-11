/*
 * =============================================================================
 * EMBEDDED RANDOM FOREST CLASSIFIER FOR ESP32 (EDGE AI)
 * Generated automatically for 5-Second Window Classification
 * =============================================================================
 */

#ifndef RF_MODEL_H
#define RF_MODEL_H

#include <Arduino.h>

class EdgeWaterClassifier {
public:
    static int predict(const float x[8]) {
        float votes[3] = {0.0f};
        
        // --- Tree 0 ---
        if (x[6] <= 123.9500f) {
            if (x[4] <= 262.5390f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[3] <= 280.3000f) {
                if (x[7] <= 432.9000f) {
                    votes[1] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 1 ---
        if (x[7] <= 143.1500f) {
            votes[0] += 1.0f;
        } else {
            if (x[4] <= 338.9330f) {
                votes[1] += 1.0f;
            } else {
                if (x[0] <= 251.4710f) {
                    if (x[7] <= 432.9000f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 2 ---
        if (x[4] <= 132.9930f) {
            votes[0] += 1.0f;
        } else {
            if (x[2] <= 269.8000f) {
                if (x[6] <= 374.1500f) {
                    if (x[0] <= 355.1430f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 3 ---
        if (x[2] <= 173.0000f) {
            if (x[4] <= 262.5390f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[0] <= 275.8490f) {
                votes[1] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 4 ---
        if (x[7] <= 141.9500f) {
            votes[0] += 1.0f;
        } else {
            if (x[3] <= 280.3000f) {
                if (x[2] <= 128.3500f) {
                    votes[2] += 1.0f;
                } else {
                    votes[1] += 1.0f;
                }
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 5 ---
        if (x[7] <= 141.9500f) {
            votes[0] += 1.0f;
        } else {
            if (x[6] <= 325.5500f) {
                if (x[5] <= 77.9841f) {
                    votes[1] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 6 ---
        if (x[0] <= 169.3220f) {
            if (x[5] <= 13.8005f) {
                if (x[7] <= 317.7000f) {
                    votes[0] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[2] <= 270.2500f) {
                if (x[1] <= 30.9131f) {
                    votes[1] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 7 ---
        if (x[3] <= 187.7000f) {
            if (x[1] <= 1.1343f) {
                votes[2] += 1.0f;
            } else {
                if (x[0] <= 63.1010f) {
                    if (x[3] <= 85.4000f) {
                        votes[0] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    votes[0] += 1.0f;
                }
            }
        } else {
            if (x[4] <= 341.2060f) {
                votes[1] += 1.0f;
            } else {
                if (x[0] <= 259.2540f) {
                    if (x[2] <= 136.6000f) {
                        votes[2] += 1.0f;
                    } else {
                        votes[1] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 8 ---
        if (x[4] <= 132.0740f) {
            votes[0] += 1.0f;
        } else {
            if (x[2] <= 270.2500f) {
                if (x[0] <= 267.0050f) {
                    if (x[5] <= 17.2074f) {
                        if (x[0] <= 130.5950f) {
                            votes[2] += 1.0f;
                        } else {
                            if (x[3] <= 280.4000f) {
                                votes[1] += 1.0f;
                            } else {
                                votes[2] += 1.0f;
                            }
                        }
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 9 ---
        if (x[4] <= 132.0740f) {
            votes[0] += 1.0f;
        } else {
            if (x[4] <= 341.2060f) {
                votes[1] += 1.0f;
            } else {
                if (x[7] <= 362.3000f) {
                    if (x[3] <= 287.7500f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 10 ---
        if (x[2] <= 173.0000f) {
            if (x[7] <= 294.5500f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[3] <= 279.7000f) {
                votes[1] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 11 ---
        if (x[5] <= 3.7217f) {
            if (x[2] <= 179.3500f) {
                votes[0] += 1.0f;
            } else {
                if (x[3] <= 290.5000f) {
                    votes[1] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            }
        } else {
            if (x[3] <= 280.3000f) {
                if (x[6] <= 118.5500f) {
                    votes[0] += 1.0f;
                } else {
                    if (x[4] <= 422.5010f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                }
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 12 ---
        if (x[2] <= 178.2000f) {
            if (x[3] <= 233.6500f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[6] <= 324.2500f) {
                votes[1] += 1.0f;
            } else {
                if (x[7] <= 362.0500f) {
                    if (x[2] <= 273.3000f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 13 ---
        if (x[6] <= 123.9500f) {
            if (x[0] <= 176.1300f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[0] <= 267.0050f) {
                if (x[0] <= 167.8600f) {
                    votes[2] += 1.0f;
                } else {
                    if (x[7] <= 409.7500f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                }
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 14 ---
        if (x[3] <= 189.7000f) {
            if (x[5] <= 14.3856f) {
                if (x[6] <= 304.6000f) {
                    votes[0] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[4] <= 339.5690f) {
                votes[1] += 1.0f;
            } else {
                if (x[2] <= 262.3500f) {
                    if (x[2] <= 233.8000f) {
                        votes[2] += 1.0f;
                    } else {
                        votes[1] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 15 ---
        if (x[2] <= 178.2000f) {
            if (x[7] <= 294.2000f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[0] <= 275.4300f) {
                votes[1] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 16 ---
        if (x[0] <= 169.3220f) {
            if (x[1] <= 1.0875f) {
                votes[2] += 1.0f;
            } else {
                if (x[2] <= 11.1500f) {
                    votes[2] += 1.0f;
                } else {
                    if (x[3] <= 242.3500f) {
                        votes[0] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                }
            }
        } else {
            if (x[3] <= 279.7000f) {
                votes[1] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 17 ---
        if (x[7] <= 141.9500f) {
            votes[0] += 1.0f;
        } else {
            if (x[1] <= 1.6519f) {
                votes[2] += 1.0f;
            } else {
                if (x[3] <= 279.8000f) {
                    votes[1] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 18 ---
        if (x[0] <= 169.3220f) {
            if (x[7] <= 319.4500f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[6] <= 325.5500f) {
                if (x[1] <= 58.5971f) {
                    votes[1] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            } else {
                if (x[7] <= 362.0500f) {
                    if (x[3] <= 283.2000f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 19 ---
        if (x[4] <= 132.1510f) {
            votes[0] += 1.0f;
        } else {
            if (x[7] <= 353.0000f) {
                votes[1] += 1.0f;
            } else {
                if (x[7] <= 362.3000f) {
                    if (x[4] <= 339.4140f) {
                        votes[1] += 1.0f;
                    } else {
                        if (x[0] <= 279.2870f) {
                            votes[1] += 1.0f;
                        } else {
                            votes[2] += 1.0f;
                        }
                    }
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 20 ---
        if (x[3] <= 189.7000f) {
            if (x[1] <= 1.1374f) {
                votes[2] += 1.0f;
            } else {
                if (x[7] <= 320.0000f) {
                    votes[0] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            }
        } else {
            if (x[4] <= 338.9330f) {
                votes[1] += 1.0f;
            } else {
                if (x[7] <= 362.0500f) {
                    if (x[2] <= 273.3000f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 21 ---
        if (x[3] <= 189.7000f) {
            if (x[7] <= 317.7000f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[0] <= 267.0050f) {
                if (x[3] <= 279.8000f) {
                    votes[1] += 1.0f;
                } else {
                    votes[2] += 1.0f;
                }
            } else {
                votes[2] += 1.0f;
            }
        }

        // --- Tree 22 ---
        if (x[7] <= 141.9500f) {
            votes[0] += 1.0f;
        } else {
            if (x[6] <= 327.5500f) {
                if (x[6] <= 63.7000f) {
                    votes[2] += 1.0f;
                } else {
                    votes[1] += 1.0f;
                }
            } else {
                if (x[3] <= 273.4500f) {
                    if (x[4] <= 423.5770f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    votes[2] += 1.0f;
                }
            }
        }

        // --- Tree 23 ---
        if (x[3] <= 187.7000f) {
            if (x[7] <= 317.7000f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[7] <= 349.7000f) {
                votes[1] += 1.0f;
            } else {
                if (x[6] <= 325.5500f) {
                    if (x[3] <= 383.1000f) {
                        votes[1] += 1.0f;
                    } else {
                        votes[2] += 1.0f;
                    }
                } else {
                    if (x[4] <= 350.3290f) {
                        if (x[4] <= 350.2140f) {
                            if (x[2] <= 270.8000f) {
                                votes[1] += 1.0f;
                            } else {
                                votes[2] += 1.0f;
                            }
                        } else {
                            votes[1] += 1.0f;
                        }
                    } else {
                        votes[2] += 1.0f;
                    }
                }
            }
        }

        // --- Tree 24 ---
        if (x[3] <= 189.7000f) {
            if (x[6] <= 304.6000f) {
                votes[0] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        } else {
            if (x[3] <= 280.3000f) {
                votes[1] += 1.0f;
            } else {
                votes[2] += 1.0f;
            }
        }


        int best_class = 0;
        float max_votes = votes[0];
        for (int i = 1; i < 3; i++) {
            if (votes[i] > max_votes) {
                max_votes = votes[i];
                best_class = i;
            }
        }
        return best_class; // 0: Low, 1: Medium, 2: High
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

#endif // RF_MODEL_H
