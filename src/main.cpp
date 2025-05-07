// ---- Includes ----
#include "globals_and_includes.h" // Includes config.h
#include "web_handlers.h"
#include "wifi_manager.h"
#include "sensor_processing.h"
#include "drawing.h"
#include "calculations.h"

// ---- Definitions for 'extern const' variables from config.h ----
const char *station_ssid = "Jappiepixel";
const char *station_password = "Latour69";
const char *ap_ssid = "M5Dial-TargetSetter";
const char *ap_password = "Lisvoort11"; // Or NULL for an open network

const int offset_x = 0;
const int offset_y = 0;
const float scale_x = 1.0;
const float scale_y = 1.0;
const double MAGNETIC_DECLINATION = 1.7; // Example for your location

const float HEADING_SMOOTHING_FACTOR = 0.1;
const char* GEOCODING_USER_AGENT = "M5Dial-CompassNav/1.0 (your.email@example.com)"; // CUSTOMIZE

// ---- Global Object Definitions (already declared 'extern' in globals_and_includes.h) ----
M5Canvas canvas(&M5Dial.Display);
MechaQMC5883 qmc;
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);
WebServer server(80);

double TARGET_LAT = 0.0;
double TARGET_LON = 0.0;
String currentNetworkIP = "N/A";
String Setaddress = "";

// Definitions for global state variables (already declared 'extern' in globals_and_includes.h)
double smoothedHeadingX = 0.0;
double smoothedHeadingY = 0.0;
bool firstHeadingReading = true;


// Compass Display Geometry (initialized in initializeHardwareAndSensors)
int centerX, centerY, R;

void setup() {
    Serial.begin(115200); 
    while (!Serial && millis() < 2000); 
    Serial.println(F("\n--- M5Dial Navigator Starting Up ---"));

    // Initialize M5Dial hardware, display, canvas, GPS, QMC compass, and display geometry
    initializeHardwareAndSensors();
    Serial.println(F("Hardware and Sensors Initialized."));

    // Display initial status on M5Dial (centerX, centerY are now set)
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(1); // Use smaller text for status messages
    M5Dial.Display.drawString("Sensors OK", centerX, M5Dial.Display.height() / 2 - 55);

    // Connect to WiFi (STA mode) or fallback to AP mode
    bool networkReady = connectWiFiWithFallback();
    Serial.println(networkReady ? F("Network is ready.") : F("Network setup failed or is unavailable."));

    if (networkReady) {
        setupServerRoutes(); 
        M5Dial.Display.drawString("Web Server Active", centerX, M5Dial.Display.height() / 2 + 35); 
        Serial.println(F("Web server routes configured and server started."));
    } else {
        M5Dial.Display.drawString("Network Inactive", centerX, M5Dial.Display.height() / 2 + 35); 
        Serial.println(F("Web server not started due to network unavailability."));
    }

    // Short delay to display initialization messages and IP address
    Serial.println(F("Displaying IP info for a few seconds..."));
    delay(4000);

    // Prepare display for main loop
    M5Dial.Display.fillScreen(TFT_BLACK);  
    M5Dial.Display.setTextDatum(TL_DATUM); 
    M5Dial.Display.setTextSize(1);         
    Serial.println(F("Setup complete. Entering main loop."));
}

// ---- MAIN LOOP: Runs repeatedly ----
void loop() {
    M5.update();           
    server.handleClient();  
    processGpsData();       

    // Get the current smoothed heading from the compass
    double currentHeadingDegrees = getSmoothedHeadingDegrees();
    double currentHeadingRadians = currentHeadingDegrees * M_PI / 180.0;

    // Variables for target navigation
    double targetBearingDegrees = 0.0;
    double arrowAngleOnCompassDegrees = 0.0; // This is target_bearing - current_heading

    // Check GPS status and if a target has been set
    bool locationIsValid = gps.location.isValid() && gps.location.age() < 3000; // GPS fix is recent enough
    bool targetIsSet = (TARGET_LAT != 0.0 || TARGET_LON != 0.0); 

    if (locationIsValid && targetIsSet) {

        targetBearingDegrees = calculateTargetBearing(
            gps.location.lat(), gps.location.lng(),
            TARGET_LAT, TARGET_LON
        );

        // Calculate the angle the target arrow should point on the compass display
        // This is relative to the current heading of the device
        arrowAngleOnCompassDegrees = targetBearingDegrees - currentHeadingDegrees;
        arrowAngleOnCompassDegrees = fmod(arrowAngleOnCompassDegrees + 360.0, 360.0); // Normalize to 0-360
    }

    // ---- Drawing Logic on M5Canvas ----
    // 1. Draw the static compass background and rotating labels
    drawCompassBackgroundToCanvas(canvas, centerX, centerY, R);
    drawCompassLabels(canvas, currentHeadingRadians, centerX, centerY, R); // Pass radians for N/E/S/W label rotation

    // 2. Draw GPS information
    drawGpsInfo(canvas, gps, centerX, centerY); // Pass the whole gps object

    // 3. Draw status messages or target arrow
    if (!locationIsValid) {
        String statusMsg = "Acquiring GPS...";
        if (Setaddress != "" && !targetIsSet) { 
            statusMsg = Setaddress.substring(0, min((int)Setaddress.length(), 25)); // Show part of address
             if(Setaddress.length() > 25) statusMsg += "...";
        } else if (Setaddress != "" && targetIsSet) { // Address was geocoded, now waiting for current GPS
             statusMsg = "Target: ";
             statusMsg += Setaddress.substring(0, min((int)Setaddress.length(), 20)); 
             if(Setaddress.length() > 20) statusMsg += "...";
        }
        drawStatusMessage(canvas, statusMsg.c_str(), centerX, centerY + R - 40, 1, TFT_ORANGE); 
    } else if (!targetIsSet) {
        // Display WiFi/AP info for setting the target
        drawStatusMessage(canvas, "Set Target via WiFi:", centerX, centerY + R - 70, 1, TFT_CYAN);
        if (currentNetworkIP != "N/A") {
             drawStatusMessage(canvas, currentNetworkIP.c_str(), centerX, centerY + R - 50, 1, TFT_CYAN);
        } else {
             drawStatusMessage(canvas, "No Network IP", centerX, centerY + R - 50, 1, TFT_RED);
        }
    } else { // Location is valid AND target is set
        drawTargetArrow(canvas, arrowAngleOnCompassDegrees, centerX, centerY, R);
    }

    canvas.pushSprite(0, 0);
    delay(50);
}