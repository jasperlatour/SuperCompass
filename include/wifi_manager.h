#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "globals_and_includes.h" // For WiFi, M5Dial objects, config.h (for credentials)

// Attempts to connect to WiFi in Station mode, falls back to AP mode on failure.
// Returns true if a network interface (STA or AP) is active, indicating the server can start.
bool connectWiFiWithFallback();

#endif // WIFI_MANAGER_H