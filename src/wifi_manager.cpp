#include "wifi_manager.h"
// Assumes globals_and_includes.h is included via wifi_manager.h
// Access to global 'currentNetworkIP', 'centerX', 'centerY'
// Access to constants from 'config.h' like 'station_ssid', 'ap_ssid', etc.

bool connectWiFiWithFallback() {
    // M5Dial display should be initialized before calling this
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Connecting to WiFi:", centerX, M5Dial.Display.height() / 2 - 45);
    M5Dial.Display.drawString(station_ssid, centerX, M5Dial.Display.height() / 2 - 30);

    WiFi.mode(WIFI_STA); // Set to Station mode
    WiFi.begin(station_ssid, station_password);

    int connect_timeout = 20; // 10 seconds timeout (20 * 500ms)
    String dots = "";
    while (WiFi.status() != WL_CONNECTED && connect_timeout > 0) {
        delay(500);
        Serial.print(".");
        dots += ".";
        // Clear previous dots area before redrawing
        M5Dial.Display.fillRect(centerX - (dots.length() * 3), M5Dial.Display.height() / 2 - 15, dots.length() * 6, 10, TFT_BLACK);
        M5Dial.Display.drawString(dots, centerX, M5Dial.Display.height() / 2 - 10);
        connect_timeout--;
        if (dots.length() > 10) { // Reset dots to prevent excessive width
            M5Dial.Display.fillRect(centerX - (dots.length() * 3), M5Dial.Display.height() / 2 - 15, dots.length() * 6, 10, TFT_BLACK);
            dots = "";
        }
    }
    // Clear progress area
    M5Dial.Display.fillRect(0, M5Dial.Display.height() / 2 - 20, M5Dial.Display.width(), 20, TFT_BLACK); 

    bool serverShouldStart = false;
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("\nConnected to WiFi (STA mode)!"));
        currentNetworkIP = WiFi.localIP().toString();
        Serial.print(F("STA IP Address: ")); Serial.println(currentNetworkIP);
        M5Dial.Display.drawString("WiFi Connected!", centerX, M5Dial.Display.height() / 2 - 15);
        M5Dial.Display.drawString("IP: " + currentNetworkIP, centerX, M5Dial.Display.height() / 2);
        serverShouldStart = true;
    } else {
        Serial.println(F("\nFailed to connect to WiFi network. Falling back to AP mode."));
        M5Dial.Display.drawString("STA WiFi Failed.", centerX, M5Dial.Display.height() / 2 - 15);
        M5Dial.Display.drawString("Starting AP...", centerX, M5Dial.Display.height() / 2);
        
        WiFi.disconnect(true); // Disconnect STA mode explicitly
        WiFi.mode(WIFI_AP);    // Switch to AP mode
        
        // Start AP. If ap_password is NULL or empty, it creates an open network.
        if (ap_password && strlen(ap_password) > 0) {
            WiFi.softAP(ap_ssid, ap_password);
        } else {
            WiFi.softAP(ap_ssid); // Open network
        }
        delay(500); // Allow AP to initialize
        
        currentNetworkIP = WiFi.softAPIP().toString();
        Serial.print(F("AP Mode Enabled. SSID: ")); Serial.println(ap_ssid);
        Serial.print(F("AP IP Address: ")); Serial.println(currentNetworkIP);
        
        // Clear "Starting AP..." and display AP info
        M5Dial.Display.fillRect(0, M5Dial.Display.height() / 2 - 5, M5Dial.Display.width(), 20, TFT_BLACK); 
        M5Dial.Display.drawString("AP Mode Active", centerX, M5Dial.Display.height() / 2 - 5);
        M5Dial.Display.drawString("SSID: " + String(ap_ssid), centerX, M5Dial.Display.height() / 2 + 10);
        M5Dial.Display.drawString("IP: " + currentNetworkIP, centerX, M5Dial.Display.height() / 2 + 25); // Adjusted Y for SSID
        serverShouldStart = true; // Server can start on AP IP
    }
    return serverShouldStart;
}