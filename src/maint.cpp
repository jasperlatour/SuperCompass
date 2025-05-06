#include "M5Dial.h"
#include <math.h>         // For cos(), sin(), atan2(), fmod(), M_PI
#include <MechaQMC5883.h> // For the QMC5883 compass sensor
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// ---- WiFi, Web Server, HTTP Client, and JSON Includes ----
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>   // For making API requests
#include <ArduinoJson.h>  // For parsing API responses

#include "drawing.h"
#include "calculations.h" // For calculateTargetBearing
#include "config.h"       // Make sure this includes your calibration constants etc.

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Use M5Dial's display object for the canvas
M5Canvas canvas(&M5Dial.Display);

// Sensor Objects
MechaQMC5883 qmc;
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1); // UART1 (RX=G2, TX=G1 for PortA)

// Compass Display Geometry
int centerX, centerY, R;

// ---- Web Server and Target Variables ----
WebServer server(80); // Web server on port 80

double TARGET_LAT = 0.0;
double TARGET_LON = 0.0;

// ---- WiFi Credentials ----
// IMPORTANT: Replace with your actual WiFi credentials for internet access
const char *station_ssid = "Jappiepixel";         // Your network SSID (name)
const char *station_password = "Latour69"; // Your network password

// Variables for AP mode (fallback)
const char *ap_ssid = "M5Dial-TargetSetter";
const char *ap_password = "Lisvoort11"; // Or NULL for an open network

String currentNetworkIP = "N/A"; // Variable to store the active IP address

// ---- HTML Page Content and Handlers (Identical to previous version) ----
String HTML_CONTENT = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>M5Dial Target Setter</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; color: #333; }
        .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); max-width: 500px; margin: auto; }
        h1 { color: #007bff; text-align: center; }
        h2 { margin-top: 30px; border-bottom: 1px solid #eee; padding-bottom: 5px;}
        label { display: block; margin-top: 15px; margin-bottom: 5px; font-weight: bold; }
        input[type="text"], input[type="number"] {
            width: calc(100% - 22px); padding: 10px; margin-bottom: 10px; border: 1px solid #ddd;
            border-radius: 4px; box-sizing: border-box;
        }
        input[type="submit"] {
            background-color: #007bff; color: white; padding: 12px 20px; border: none;
            border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%; margin-top: 10px;
        }
        input[type="submit"]:hover { background-color: #0056b3; }
        .current-target { margin-top:20px; margin-bottom:20px; padding:10px; background-color:#e9ecef; border-radius:4px; }
        hr { margin-top: 30px; margin-bottom: 20px; border: 0; border-top: 1px solid #eee; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Set Navigation Target</h1>
        <div class="current-target">
            <strong>Current Target:</strong><br>
            Lat: <span id="current_lat_display">%.6f</span><br>
            Lon: <span id="current_lon_display">%.6f</span>
        </div>

        <h2>Option 1: Set by Coordinates</h2>
        <form action="/settarget" method="POST">
            <label for="lat">Target Latitude:</label>
            <input type="number" step="any" id="lat" name="latitude" placeholder="e.g., 40.7128" required>
            <label for="lon">Target Longitude:</label>
            <input type="number" step="any" id="lon" name="longitude" placeholder="e.g., -74.0060" required>
            <input type="submit" value="Set Target by Lat/Lon">
        </form>

        <hr>

        <h2>Option 2: Set by Address</h2>
        <form action="/settargetbyaddress" method="POST">
            <label for="address">Target Address:</label>
            <input type="text" id="address" name="address" placeholder="e.g., Eiffeltoren, Parijs" required>
            <input type="submit" value="Search Address & Set Target">
        </form>
        <div id="message-area"></div> </div>
</body>
</html>
)rawliteral";

void sendResponsePage(const String& title, const String& statusType, const String& headerMsg, const String& bodyMsg, int refreshDelay = 3) {
    String responseHtml = R"rawliteral(
    <!DOCTYPE html><html><head><title>%s</title>
    <meta http-equiv="refresh" content="%d;url=/" />
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }
        .message { padding: 15px; border-radius: 5px; margin: 20px auto; max-width: 500px; word-wrap: break-word; }
        .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        a { color: #007bff; text-decoration: none; }
    </style></head><body>
        <div class="message %s">
            <h1>%s</h1>
            <p>%s</p>
            <p>Redirecting back in %d seconds... or <a href="/">click here</a>.</p>
        </div>
    </body></html>
    )rawliteral";
    size_t bufferSize = responseHtml.length() + title.length() + statusType.length() + headerMsg.length() + bodyMsg.length() + 20;
    char* buffer = new char[bufferSize];
    if (!buffer) {
        Serial.println("Failed to allocate memory for response buffer!");
        server.send(500, "text/plain", "Internal Server Error: Memory allocation failed.");
        return;
    }
    sprintf(buffer, responseHtml.c_str(), title.c_str(), refreshDelay, statusType.c_str(), headerMsg.c_str(), bodyMsg.c_str(), refreshDelay);
    server.send(200, "text/html", buffer);
    delete[] buffer;
}

void handleRoot() {
    char buffer[HTML_CONTENT.length() + 200];
    sprintf(buffer, HTML_CONTENT.c_str(), TARGET_LAT, TARGET_LON);
    server.send(200, "text/html", buffer);
}

void handleSetTarget() {
    String messageBody = "";
    bool success = false;
    if (server.hasArg("latitude") && server.hasArg("longitude")) {
        String latStr = server.arg("latitude");
        String lonStr = server.arg("longitude");
        double tempLat = latStr.toDouble();
        double tempLon = lonStr.toDouble();
        if (abs(tempLat) <= 90.0 && abs(tempLon) <= 180.0) {
            TARGET_LAT = tempLat;
            TARGET_LON = tempLon;
            Serial.print("New Target (Lat/Lon): "); Serial.print(TARGET_LAT, 6); Serial.print(", "); Serial.println(TARGET_LON, 6);
            messageBody = "Target updated successfully!<br>Lat: " + String(TARGET_LAT, 6) + "<br>Lon: " + String(TARGET_LON, 6);
            success = true;
        } else {
            messageBody = "Error: Invalid latitude or longitude values.";
            Serial.println(messageBody);
        }
    } else {
        messageBody = "Error: Missing latitude or longitude parameters.";
        Serial.println(messageBody);
    }
    sendResponsePage("Target Status", success ? "success" : "error", success ? "Success!" : "Error!", messageBody);
}

void handleSetTargetByAddress() {
    String address = "";
    String messageBody = "";
    bool geocodeOverallSuccess = false;

    if (!server.hasArg("address")) {
        messageBody = "Error: Address parameter missing.";
        Serial.println(messageBody);
        sendResponsePage("Geocoding Error", "error", "Error!", messageBody);
        return;
    }
    address = server.arg("address");
    Serial.print("Address received for geocoding: "); Serial.println(address);

    if (WiFi.status() != WL_CONNECTED) { // Crucially checks if we have internet via STA mode
        messageBody = "Error: M5Dial not connected to Wi-Fi with Internet access. Cannot geocode address.";
        Serial.println(messageBody);
        sendResponsePage("Geocoding Error", "error", "Network Error!", messageBody, 5);
        return;
    }

    HTTPClient http;
    String encodedAddress = "";
    for (char c : address) {
        if (isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.') {
            encodedAddress += (c == ' ') ? "%20" : String(c);
        } else {
            char temp[4];
            sprintf(temp, "%%%02X", (unsigned char)c);
            encodedAddress += temp;
        }
    }
    String url = "https://nominatim.openstreetmap.org/search?q=" + encodedAddress + "&format=json&limit=1&addressdetails=0&extratags=0&namedetails=0";
    Serial.print("Geocoding URL: "); Serial.println(url);
    http.begin(url);
    http.setUserAgent("M5Dial-CompassNav/1.0 (m5dial.user@example.com)"); // CUSTOMIZE THIS
    int httpCode = http.GET();
    double foundLat = 0.0, foundLon = 0.0;
    bool geocodeApiSuccess = false;

    if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.println("Payload received:\n" + payload);
            StaticJsonDocument<1024> doc; // Adjust size (1024) based on expected JSON payload
            DeserializationError error = deserializeJson(doc, payload);
            if (error) {
                Serial.print(F("deserializeJson() failed: ")); Serial.println(error.f_str());
                messageBody = "Failed to parse geocoding API response. Error: " + String(error.f_str());
            } else {
                if (doc.is<JsonArray>() && doc.as<JsonArray>().size() > 0) {
                    JsonObject firstResult = doc.as<JsonArray>()[0];
                    if (firstResult.containsKey("lat") && firstResult.containsKey("lon")) {
                        foundLat = firstResult["lat"].as<String>().toDouble();
                        foundLon = firstResult["lon"].as<String>().toDouble();
                        geocodeApiSuccess = true;
                    } else { messageBody = "Latitude or Longitude not found in API response."; Serial.println(messageBody); }
                } else { messageBody = "No results found for the address, or unexpected API response format."; Serial.println(messageBody); }
            }
        } else { messageBody = "Geocoding API request failed. HTTP Error: " + String(httpCode) + " " + http.errorToString(httpCode); Serial.println(messageBody); }
    } else { messageBody = "Geocoding API request failed. Error: " + http.errorToString(httpCode); Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str()); }
    http.end();

    if (geocodeApiSuccess) {
        if (abs(foundLat) <= 90.0 && abs(foundLon) <= 180.0) {
            TARGET_LAT = foundLat; TARGET_LON = foundLon;
            Serial.print("New Target (Address: '"); Serial.print(address); Serial.print("'): "); Serial.print(TARGET_LAT, 6); Serial.print(", "); Serial.println(TARGET_LON, 6);
            messageBody = "Target updated from address: <b>" + address + "</b><br>Lat: " + String(TARGET_LAT, 6) + "<br>Lon: " + String(TARGET_LON, 6);
            geocodeOverallSuccess = true;
        } else { messageBody = "Error: Invalid latitude/longitude values received from geocoding API."; Serial.println(messageBody); }
    } else { if (messageBody.isEmpty()) messageBody = "Could not geocode address. Unknown error."; }
    sendResponsePage("Geocoding Status", geocodeOverallSuccess ? "success" : "error", geocodeOverallSuccess ? "Success!" : "Geocoding Error!", messageBody, geocodeOverallSuccess ? 3 : 7);
}

void handleNotFound() {
    server.send(404, "text/plain", "404: Not found");
}


void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, true);
    M5Dial.Display.setBrightness(50);

    M5Dial.Display.fillScreen(TFT_BLACK);
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.drawString("Initializing...", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 70);

    GPS_Serial.begin(9600, SERIAL_8N1, 1, 2);
    canvas.createSprite(M5Dial.Display.width(), M5Dial.Display.height());
    Wire.begin();
    qmc.init();

    centerX = M5Dial.Display.width() / 2;
    centerY = M5Dial.Display.height() / 2;
    R = M5Dial.Display.height() / 2 - 5;

    // ---- MODIFIED Wi-Fi Setup ----
    M5Dial.Display.setTextSize(1); // Smaller text for IP display
    M5Dial.Display.drawString("Connecting to WiFi:", centerX, M5Dial.Display.height() / 2 - 45);
    M5Dial.Display.drawString(station_ssid, centerX, M5Dial.Display.height() / 2 - 30);

    WiFi.mode(WIFI_STA); // Explicitly set to Station mode
    WiFi.begin(station_ssid, station_password);

    int connect_timeout = 20; // 10 seconds timeout (20 * 500ms)
    String dots = "";
    while (WiFi.status() != WL_CONNECTED && connect_timeout > 0) {
        delay(500);
        Serial.print(".");
        dots += ".";
        M5Dial.Display.fillRect(centerX - 20, M5Dial.Display.height() / 2 - 15, 40, 10, TFT_BLACK); // Clear previous dots
        M5Dial.Display.drawString(dots, centerX, M5Dial.Display.height() / 2 - 10);
        connect_timeout--;
        if (dots.length() > 10) dots = ""; // Reset dots string
    }
    M5Dial.Display.fillRect(0, M5Dial.Display.height() / 2 - 20, M5Dial.Display.width(), 20, TFT_BLACK); // Clear progress area

    bool serverShouldStart = false;
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi (STA mode)!");
        currentNetworkIP = WiFi.localIP().toString();
        Serial.print("STA IP Address: "); Serial.println(currentNetworkIP);
        M5Dial.Display.drawString("WiFi Connected!", centerX, M5Dial.Display.height() / 2 - 15);
        M5Dial.Display.drawString("IP: " + currentNetworkIP, centerX, M5Dial.Display.height() / 2);
        serverShouldStart = true;
    } else {
        Serial.println("\nFailed to connect to WiFi network. Falling back to AP mode.");
        M5Dial.Display.drawString("STA WiFi Failed.", centerX, M5Dial.Display.height() / 2 - 15);
        M5Dial.Display.drawString("Starting AP...", centerX, M5Dial.Display.height() / 2);
        
        WiFi.mode(WIFI_AP); // Switch to AP mode
        WiFi.softAP(ap_ssid, ap_password);
        delay(100); // Allow AP to initialize
        
        currentNetworkIP = WiFi.softAPIP().toString();
        Serial.print("AP Mode Enabled. IP Address: "); Serial.println(currentNetworkIP);
        M5Dial.Display.fillRect(0, M5Dial.Display.height() / 2 -5, M5Dial.Display.width(), 20, TFT_BLACK); // Clear "Starting AP..."
        M5Dial.Display.drawString("AP Mode Active", centerX, M5Dial.Display.height() / 2 -5);
        M5Dial.Display.drawString("IP: " + currentNetworkIP, centerX, M5Dial.Display.height() / 2 + 10);
        serverShouldStart = true; // Start server on AP IP
    }

    // ---- Web Server Setup ----
    if (serverShouldStart) {
        server.on("/", HTTP_GET, handleRoot);
        server.on("/settarget", HTTP_POST, handleSetTarget);
        server.on("/settargetbyaddress", HTTP_POST, handleSetTargetByAddress);
        server.onNotFound(handleNotFound);
        server.begin();
        Serial.println("HTTP server started on IP: " + currentNetworkIP);
        M5Dial.Display.drawString("Server ON", centerX, M5Dial.Display.height() / 2 + 25);
    } else {
        Serial.println("Web server NOT started (No network connection possible).");
        M5Dial.Display.drawString("No Network!", centerX, M5Dial.Display.height() / 2 + 25);
        // currentNetworkIP remains "N/A" or the last attempted IP.
    }

    delay(4000); // Display IP info for a few seconds
    M5Dial.Display.setTextDatum(TL_DATUM);
    M5Dial.Display.setTextSize(1);

    Serial.println("Setup Complete. Waiting for GPS fix...");
}

void loop() {
    M5.update();
    server.handleClient();

    bool gpsDataUpdated = false;
    while (GPS_Serial.available() > 0) {
        if (gps.encode(GPS_Serial.read())) {
            gpsDataUpdated = true;
        }
    }

    // (Compass and Target Calculation Logic - identical to previous version)
    // Ensure these are defined in config.h: offset_x, offset_y, scale_x, scale_y, MAGNETIC_DECLINATION
    // Also HEADING_SMOOTHING_FACTOR, firstHeadingReading, smoothedHeadingX, smoothedHeadingY
    double rawTrueHeading = calculateTrueHeading(qmc, offset_x, offset_y, scale_x, scale_y, MAGNETIC_DECLINATION);
    double rawTrueHeading_rad = rawTrueHeading * M_PI / 180.0;

    double currentX = cos(rawTrueHeading_rad);
    double currentY = sin(rawTrueHeading_rad);

    if (firstHeadingReading) {
        smoothedHeadingX = currentX;
        smoothedHeadingY = currentY;
        firstHeadingReading = false;
    } else {
        smoothedHeadingX = HEADING_SMOOTHING_FACTOR * currentX + (1.0 - HEADING_SMOOTHING_FACTOR) * smoothedHeadingX;
        smoothedHeadingY = HEADING_SMOOTHING_FACTOR * currentY + (1.0 - HEADING_SMOOTHING_FACTOR) * smoothedHeadingY;
    }

    double smoothedHeading_rad = atan2(smoothedHeadingY, smoothedHeadingX);
    double smoothedHeading_deg = fmod(smoothedHeading_rad * 180.0 / M_PI + 360.0, 360.0);

    double trueHeading_rad = smoothedHeading_rad;
    double trueHeading = smoothedHeading_deg;

    double targetBearing = 0.0;
    double arrowAngle = 0.0;
    bool locationIsValid = gps.location.isValid() && gps.location.age() < 3000;
    bool targetIsSet = (TARGET_LAT != 0.0 || TARGET_LON != 0.0);

    if (locationIsValid && targetIsSet) {
        double currentLat = gps.location.lat();
        double currentLon = gps.location.lng();
        targetBearing = calculateTargetBearing(currentLat, currentLon, TARGET_LAT, TARGET_LON);
        arrowAngle = targetBearing - trueHeading;
        arrowAngle = fmod(arrowAngle + 360.0, 360.0);
    }

    // (Drawing Logic - update for IP display if needed)
    drawCompassBackgroundToCanvas(canvas, centerX, centerY, R);
    drawCompassLabels(canvas, trueHeading_rad, centerX, centerY, R);
    drawGpsInfo(canvas, gps, centerX, centerY);

    if (!locationIsValid) {
        drawStatusMessage(canvas, "Need GPS Fix", centerX, centerY + R - 60, 2, TFT_ORANGE);
    } else if (!targetIsSet) {
        // currentNetworkIP holds the IP address from setup (either STA or AP fallback)
        drawStatusMessage(canvas, "Set Target via WiFi", centerX, centerY + R - 70, 1, TFT_CYAN);
        if (currentNetworkIP != "N/A") {
             drawStatusMessage(canvas, currentNetworkIP.c_str(), centerX, centerY + R - 50, 1, TFT_CYAN);
        } else {
             drawStatusMessage(canvas, "No Network IP", centerX, centerY + R - 50, 1, TFT_RED);
        }
    } else {
        drawTargetArrow(canvas, arrowAngle, centerX, centerY, R);
    }

    canvas.pushSprite(0, 0);
    delay(50);
}

// Ensure you have your drawing.h, calculations.h, and config.h files correctly set up.