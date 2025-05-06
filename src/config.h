#ifndef CONFIG_H
#define CONFIG_H
 

const float MAGNETIC_DECLINATION = 2; // Example: 1.5° West declination
const float offset_x = 345.0;
const float offset_y = -196.0;

// Calibration offsets for the QMC5883 sensor
const float scale_x = 0.996644295;
const float scale_y = 1.003378378;

// --- Smoothing Parameters ---
const float HEADING_SMOOTHING_FACTOR = 0.6; // Adjust between 0.0 (heavy smoothing) and 1.0 (no smoothing)
double smoothedHeadingX = 0.0;
double smoothedHeadingY = 0.0;
bool firstHeadingReading = true; 

//network credentials
// Network Credentials (Consider using WiFiManager for production)
const char* ssid = "Just One Byte";
const char* password = "Brandevoort11";


#endif 