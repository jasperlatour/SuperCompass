#include "config.h"

// ---- Definitions for 'extern const' variables from config.h ----
const char *station_ssid = "REDACTED_WIFI_SSID";
const char *station_password = "REDACTED_WIFI_PASSWORD";
const char *ap_ssid = "M5Dial-TargetSetter";
const char *ap_password = "REDACTED_AP_PASSWORD"; // Or NULL for an open network


const int offset_x = 0;
const int offset_y = 0;
const float scale_x = 1.0;
const float scale_y = 1.0;
const double MAGNETIC_DECLINATION = 1.7; // Example for your location

const float HEADING_SMOOTHING_FACTOR = 0.1;
const char* GEOCODING_USER_AGENT = "M5Dial-CompassNav/1.0 (your.email@example.com)"; // CUSTOMIZE

// Definitions for global state variables (already declared 'extern' in globals_and_includes.h)
double smoothedHeadingX = 0.0;
double smoothedHeadingY = 0.0;
bool firstHeadingReading = true;

//menu settings
int numMenuItems = 3;