#ifndef CONFIG_H
#define CONFIG_H

// ---- WiFi Credentials ----
// Declared here, defined in your main .ino or a config.cpp
extern const char *station_ssid;
extern const char *station_password;

// ---- AP Mode Credentials (Fallback) ----
extern const char *ap_ssid;
extern const char *ap_password;

// ---- Sensor Calibration Constants ----
extern const int offset_x;
extern const int offset_y;
extern const float scale_x;
extern const float scale_y;
extern const double MAGNETIC_DECLINATION;

// ---- Navigation Logic Constants ----
extern const float HEADING_SMOOTHING_FACTOR;

// ---- Geocoding API ----
extern const char* GEOCODING_USER_AGENT;

// ---- Mathematical Constants ----
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// IMPORTANT:
// The following variables (firstHeadingReading, smoothedHeadingX, smoothedHeadingY)
// should NOT be in this file.
// They are global state variables.
// Define them in your main .ino file and declare them 'extern' in globals_and_includes.h.
// Your error messages indicate they are currently in your config.h - REMOVE THEM FROM HERE.

#endif // CONFIG_H