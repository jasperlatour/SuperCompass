#include "M5Dial.h"
#include <math.h>         // For cos(), sin(), atan2(), fmod(), M_PI
#include <MechaQMC5883.h> // For the QMC5883 compass sensor
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// ---- NEW: WiFi and Web Server Includes ----
#include <WiFi.h>
#include <WebServer.h>
// -----------------------------------------

#include "drawing.h"
#include "calculations.h" // For calculateTargetBearing
#include "config.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Use M5Dial's display object for the canvas
M5Canvas canvas(&M5Dial.Display);

// Sensor Objects
MechaQMC5883 qmc;
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1); // UART1 (RX=G2, TX=G1 for PortA)

// Sensor Readings
double a; // This seems unused, consider removing if not needed elsewhere
int x, y, z; // Also seem unused in the provided loop, ensure they are for qmc or remove

// Compass Display Geometry (calculated in setup/loop)
int centerX, centerY, R;

// ---- NEW: Web Server and Target Variables ----
WebServer server(80); // Create a web server object on port 80

// These will now be updated by the web interface, so not const
double TARGET_LAT = 0.0; // Default or last known, can be updated
double TARGET_LON = 0.0; // Default or last known, can be updated

// Variables for AP mode
const char *ap_ssid = "M5Dial-TargetSetter";
const char *ap_password = "Lisvoort11"; // Change this! Or set to NULL for an open network


// ---- NEW: HTML Page Content and Handlers ----
String HTML_CONTENT = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>M5Dial Target Setter</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; color: #333; }
        .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        h1 { color: #007bff; text-align: center; }
        label { display: block; margin-top: 15px; margin-bottom: 5px; font-weight: bold; }
        input[type="text"], input[type="number"] {
            width: calc(100% - 22px); padding: 10px; margin-bottom: 10px; border: 1px solid #ddd;
            border-radius: 4px; box-sizing: border-box;
        }
        input[type="submit"] {
            background-color: #007bff; color: white; padding: 12px 20px; border: none;
            border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%;
        }
        input[type="submit"]:hover { background-color: #0056b3; }
        .current-target { margin-top:20px; padding:10px; background-color:#e9ecef; border-radius:4px; }
        .status-message { margin-top: 15px; padding: 10px; border-radius: 4px; text-align: center; }
        .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
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
        <form action="/settarget" method="POST">
            <label for="lat">Target Latitude:</label>
            <input type="number" step="any" id="lat" name="latitude" placeholder="e.g., 40.7128" required>
            <label for="lon">Target Longitude:</label>
            <input type="number" step="any" id="lon" name="longitude" placeholder="e.g., -74.0060" required>
            <input type="submit" value="Set Target">
        </form>
        <div id="message-area"></div>
    </div>
    <script>
        // Optional: Update display if form submission happens via AJAX or to reflect current state
        // For this simple POST, the page will reload.
        // If we wanted to make it more dynamic without full reload:
        // document.getElementById('targetForm').addEventListener('submit', function(event) {
        //     event.preventDefault();
        //     // ... fetch logic ...
        // });
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    char buffer[HTML_CONTENT.length() + 200]; // Adjusted buffer size
    sprintf(buffer, HTML_CONTENT.c_str(), TARGET_LAT, TARGET_LON);
    server.send(200, "text/html", buffer);
}

void handleSetTarget() {
    String message = "";
    bool success = false;
    if (server.hasArg("latitude") && server.hasArg("longitude")) {
        String latStr = server.arg("latitude");
        String lonStr = server.arg("longitude");

        double tempLat = latStr.toDouble();
        double tempLon = lonStr.toDouble();

        // Basic validation
        if (abs(tempLat) <= 90.0 && abs(tempLon) <= 180.0) {
            TARGET_LAT = tempLat;
            TARGET_LON = lonStr.toDouble(); // Re-parse to be sure, or use tempLon
            Serial.print("New Target Latitude: "); Serial.println(TARGET_LAT, 6);
            Serial.print("New Target Longitude: "); Serial.println(TARGET_LON, 6);
            message = "Target updated successfully!<br>Lat: " + String(TARGET_LAT, 6) + "<br>Lon: " + String(TARGET_LON, 6);
            success = true;
        } else {
            message = "Error: Invalid latitude or longitude values.";
            Serial.println(message);
        }
    } else {
        message = "Error: Missing latitude or longitude parameters.";
        Serial.println(message);
    }

    // Send a response page
    String responseHtml = R"rawliteral(
    <!DOCTYPE html><html><head><title>Target Status</title>
    <meta http-equiv="refresh" content="3;url=/" />
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }
        .message { padding: 15px; border-radius: 5px; margin: 20px auto; max-width: 400px; }
        .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        a { color: #007bff; text-decoration: none; }
    </style></head><body>
        <div class="message %s">
            <h1>%s</h1>
            <p>%s</p>
            <p>Redirecting back in 3 seconds... or <a href="/">click here</a>.</p>
        </div>
    </body></html>
    )rawliteral";

    char buffer[responseHtml.length() + message.length() + 100];
    sprintf(buffer, responseHtml.c_str(),
            success ? "success" : "error",
            success ? "Success!" : "Error!",
            message.c_str());
    server.send(200, "text/html", buffer);
}

void handleNotFound() {
    server.send(404, "text/plain", "404: Not found");
}
// -----------------------------------------------


void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, true);
    M5Dial.Display.setBrightness(30); // Slightly brighter for IP visibility

    // ---- Show "Starting AP" message ----
    M5Dial.Display.fillScreen(TFT_BLACK);
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.drawString("Starting AP...", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 20);
    // ------------------------------------

    // Initialize GPS Serial
    GPS_Serial.begin(9600, SERIAL_8N1, 1, 2); // PortA default: TX=G1, RX=G2 on M5Dial

    // Create the canvas (off-screen buffer)
    canvas.createSprite(M5Dial.Display.width(), M5Dial.Display.height());

    // Initialize the compass sensor
    Wire.begin(); // SDA, SCL for M5Dial are typically G5, G6 if using PortB, or internal pins
                  // If QMC5883 is on I2C Port A (Grove), pins are G2 (SDA), G1 (SCL)
                  // Your GPS_Serial uses G1, G2. So QMC5883 cannot be on Port A's I2C if GPS is on Port A's UART.
                  // The M5Dial has an internal I2C bus (G32/G33 usually) for built-in sensors.
                  // Assuming QMC5883 is connected to the main I2C bus.
    qmc.init();

    // Calculate compass geometry once
    centerX = M5Dial.Display.width() / 2;
    centerY = M5Dial.Display.height() / 2;
    R = M5Dial.Display.height() / 2;


    // ---- Wi-Fi Access Point Setup ----
    Serial.print("Setting up AP: "); Serial.println(ap_ssid);
    M5Dial.Display.drawString(ap_ssid, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 + 10);

    WiFi.softAP(ap_ssid, ap_password); // Password can be NULL for an open network

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: "); Serial.println(myIP);

    M5Dial.Display.setTextSize(2); // Reset for IP
    M5Dial.Display.drawString("IP:", M5Dial.Display.width() / 2 - 30, M5Dial.Display.height() / 2 + 40);
    M5Dial.Display.drawString(myIP.toString(), M5Dial.Display.width() / 2 + 40 , M5Dial.Display.height() / 2 + 40); // Adjust x offset
    // -------------------------------

    // ---- Web Server Setup ----
    server.on("/", HTTP_GET, handleRoot);
    server.on("/settarget", HTTP_POST, handleSetTarget); // Handle POST requests for setting target
    server.onNotFound(handleNotFound);
    server.begin(); // Start the server
    Serial.println("HTTP server started");
    M5Dial.Display.drawString("Server ON", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 + 70);
    // --------------------------

    delay(3000); // Display AP info for a few seconds
    M5Dial.Display.setTextDatum(TL_DATUM); // Reset datum for normal drawing
    M5Dial.Display.setTextSize(1); // Reset text size

    Serial.println("Setup Complete. Waiting for GPS fix...");
}

void loop() {
    M5.update();         // Handle M5Dial events
    server.handleClient(); // ---- NEW: Handle web server client requests ----

    // --- GPS Processing ---
    bool gpsDataUpdated = false;
    while (GPS_Serial.available() > 0) {
        if (gps.encode(GPS_Serial.read())) {
            gpsDataUpdated = true; // A complete sentence was processed
        }
    }

    // --- Heading Calculation ---
    // (Ensure variables like offset_x, scale_y, MAGNETIC_DECLINATION, etc. are correctly defined in config.h or globally)
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


    // --- Target Calculation ---
    double targetBearing = 0.0;
    double arrowAngle = 0.0; // This is angle relative to North to point the arrow on screen
    bool locationIsValid = gps.location.isValid() && gps.location.age() < 2000; // Valid & recent fix
    bool targetIsSet = (TARGET_LAT != 0.0 || TARGET_LON != 0.0); // Consider a target set if not default 0,0

    if (locationIsValid && targetIsSet) {
        double currentLat = gps.location.lat();
        double currentLon = gps.location.lng();
        targetBearing = calculateTargetBearing(currentLat, currentLon, TARGET_LAT, TARGET_LON); // Bearing from current to target
        
        // Arrow angle on compass display needs to be relative to the current heading of the device
        arrowAngle = targetBearing - trueHeading;
        arrowAngle = fmod(arrowAngle + 360.0, 360.0); // Normalize arrow angle 0-360
    }

    // --- Drawing ---
    drawCompassBackgroundToCanvas(canvas, centerX, centerY, R);
    drawCompassLabels(canvas, trueHeading_rad, centerX, centerY, R); // Pass smoothed heading in radians
    drawGpsInfo(canvas, gps, centerX, centerY); // Pass the whole gps object

    if (!locationIsValid) {
        drawStatusMessage(canvas, "Need GPS Fix", centerX, centerY + R - 60, 2, TFT_ORANGE);
    } else if (!targetIsSet) {
        drawStatusMessage(canvas, "Set Target via WiFi", centerX, centerY + R - 70, 1, TFT_CYAN);
        drawStatusMessage(canvas, WiFi.softAPIP().toString().c_str(), centerX, centerY + R - 50, 1, TFT_CYAN);
    } else { // Both GPS is valid and Target is set
        drawTargetArrow(canvas, arrowAngle, centerX, centerY, R); // Pass the final arrowAngle
    }

    // --- Push Sprite to Screen ---
    canvas.pushSprite(0, 0);

    delay(50); // Aim for ~20 FPS. Web server handling might add slight, variable delays.
}
