#include "M5Dial.h"
#include <math.h>          // For cos(), sin(), atan2(), fmod(), M_PI
#include <MechaQMC5883.h>  // For the QMC5883 compass sensor
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// Define PI if not already defined (it usually is in math.h)
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

// Use M5Dial's display object for the canvas
M5Canvas canvas(&M5Dial.Display);

MechaQMC5883 qmc;
double a;
int x, y, z;

int centerX, centerY, R;

// --- User Configuration ---
// Target Coordinates (Example: Replace with your desired location)
const double TARGET_LAT = 48.8584;  // Target Latitude in decimal degrees (e.g., Eiffel Tower)
const double TARGET_LON = 2.2945;   // Target Longitude in decimal degrees (e.g., Eiffel Tower)

// Magnetic Declination for your location (degrees)
// Find yours here: https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml
// West declination is negative, East is positive.
// Example: -1.5 degrees for a location with 1.5° West declination
const float MAGNETIC_DECLINATION = 2;

// Magnetometer Calibration Parameters (from your original code)
const float offset_x = 345.0;
const float offset_y = -196.0;
const float scale_x = 0.996644295;
const float scale_y = 1.003378378;
// --- End User Configuration ---

// Initialize TinyGPS++ for parsing NMEA sentences
TinyGPSPlus gps;

// Initialize HardwareSerial for UART communication with GPS module
// Using UART1 on M5Dial (GPIO1 for TX, GPIO2 for RX - check your specific wiring)
HardwareSerial GPS_Serial(1);

// Function to draw the static compass background onto a given canvas
void drawCompassBackgroundToCanvas(M5Canvas* c) {
    int textsize = M5Dial.Display.height() / 60; // Use M5Dial.Display dimensions
    if (textsize == 0) {
        textsize = 1;
    }
    c->setTextSize(textsize);

    // Define center and radius for the compass using M5Dial.Display dimensions
    centerX = M5Dial.Display.width() / 2;
    centerY = M5Dial.Display.height() / 2;
    R = M5Dial.Display.height() / 2;

    // Draw the compass background
    c->fillScreen(TFT_BLACK); // Clear the canvas first
    c->fillCircle(centerX, centerY, R, TFT_BLUE);
    c->fillCircle(centerX, centerY, R - 15, TFT_WHITE);

    // Draw static compass points (triangles) - These are fixed relative to the display
    // North (Top)
    c->fillTriangle(centerX, 0, centerX - 5, 10, centerX + 5, 10, TFT_BLACK);
    // South (Bottom)
    c->fillTriangle(centerX, M5Dial.Display.height(), centerX - 5, M5Dial.Display.height() - 10, centerX + 5, M5Dial.Display.height() - 10, TFT_BLACK);
    // West (Left)
    c->fillTriangle(0, centerY, 10, centerY - 5, 10, centerY + 5, TFT_BLACK);
    // East (Right)
    c->fillTriangle(M5Dial.Display.width(), centerY, M5Dial.Display.width() - 10, centerY - 5, M5Dial.Display.width() - 10, centerY + 5, TFT_BLACK);

    // Draw center circles
    c->fillCircle(centerX, centerY, 5, TFT_BLACK);
    c->fillCircle(centerX, centerY, 2, TFT_WHITE);
}

// Function to draw a single letter at a specific position on the main canvas
void drawLetter(int x, int y, const char* letter) {
    canvas.drawString(letter, x, y);
}

// Function to draw the dynamic compass labels (N, E, S, W) rotating with heading
// Draws directly onto the main 'canvas'
void drawCompassLabels(double heading_rad) {
    // Set text size based on display height
    int textsize = M5Dial.Display.height() / 60;
    if (textsize == 0) {
        textsize = 1;
    }
    canvas.setTextSize(textsize);
    canvas.setTextDatum(MC_DATUM);  // Middle center

    int R_letters = R - 25;  // Radius for letters, slightly inside the white circle

    // Calculate positions for N, E, S, W based on the current heading
    // Angles are relative to North (0 rad), adjusted by the current heading
    double angle_N = (0 * M_PI / 180.0) - heading_rad;
    double angle_E = (90 * M_PI / 180.0) - heading_rad;
    double angle_S = (180 * M_PI / 180.0) - heading_rad;
    double angle_W = (270 * M_PI / 180.0) - heading_rad;

    // Calculate positions using sin for x and -cos for y (screen coordinates)
    int x_N = centerX + R_letters * sin(angle_N);
    int y_N = centerY - R_letters * cos(angle_N);

    int x_E = centerX + R_letters * sin(angle_E);
    int y_E = centerY - R_letters * cos(angle_E);

    int x_S = centerX + R_letters * sin(angle_S);
    int y_S = centerY - R_letters * cos(angle_S);

    int x_W = centerX + R_letters * sin(angle_W);
    int y_W = centerY - R_letters * cos(angle_W);

    // Set text color (drawn on top of the white circle)
    canvas.setTextColor(TFT_BLACK, TFT_WHITE); // Black text on white background

    // Draw letters
    drawLetter(x_N, y_N, "N");
    drawLetter(x_E, y_E, "E");
    drawLetter(x_S, y_S, "S");
    drawLetter(x_W, y_W, "W");
}

// Function to calculate the bearing from current location to target location
// Uses spherical law of cosines / atan2 formula
// Returns bearing in degrees (0-360, True North)
double calculateTargetBearing(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg) {
    // Convert degrees to radians
    double lat1Rad = lat1Deg * M_PI / 180.0;
    double lon1Rad = lon1Deg * M_PI / 180.0;
    double lat2Rad = lat2Deg * M_PI / 180.0;
    double lon2Rad = lon2Deg * M_PI / 180.0;

    // Calculate difference in longitudes
    double dLonRad = lon2Rad - lon1Rad;

    // Calculate components for atan2(y, x)
    double y = sin(dLonRad) * cos(lat2Rad);
    double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(dLonRad);

    // Calculate bearing in radians
    double bearingRad = atan2(y, x);

    // Convert bearing to degrees
    double bearingDeg = bearingRad * 180.0 / M_PI;

    // Normalize bearing to 0-360 degrees
    bearingDeg = fmod(bearingDeg + 360.0, 360.0);

    return bearingDeg;
}

// Function to draw the arrow pointing towards the target
// Draws directly onto the main 'canvas'
void drawTargetArrow(double arrowAngleDeg) {
    // Convert arrow angle from degrees to radians
    // Adjust by -90 degrees because 0 degrees in trig is East, but we want 0 degrees to be North (top)
    double arrowAngleRad = (arrowAngleDeg - 90.0) * M_PI / 180.0;

    // Define arrow properties
    int arrowLength = R - 5; // Length from center towards edge
    int arrowBaseWidth = 10; // Width of the arrow base triangle

    // Calculate endpoint of the arrow line
    int endX = centerX + arrowLength * cos(arrowAngleRad);
    int endY = centerY + arrowLength * sin(arrowAngleRad);

    // Calculate points for the arrowhead triangle
    // Point 1: Tip of the arrow
    int tipX = endX;
    int tipY = endY;

    // Point 2: Base corner 1 (rotate 150 degrees from arrow direction)
    double angle2 = arrowAngleRad + (150.0 * M_PI / 180.0);
    int baseX1 = tipX + arrowBaseWidth * cos(angle2);
    int baseY1 = tipY + arrowBaseWidth * sin(angle2);

    // Point 3: Base corner 2 (rotate -150 degrees from arrow direction)
    double angle3 = arrowAngleRad - (150.0 * M_PI / 180.0);
    int baseX2 = tipX + arrowBaseWidth * cos(angle3);
    int baseY2 = tipY + arrowBaseWidth * sin(angle3);

    // Draw the arrow (e.g., a red triangle)
    canvas.fillTriangle(tipX, tipY, baseX1, baseY1, baseX2, baseY2, TFT_RED);
}


void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    // Initialize M5Dial components (including display)
    // Pass true, true to enable LCD and Serial
    M5Dial.begin(cfg, true, true);
    M5Dial.Display.setBrightness(24); // Use M5Dial.Display

    // Initialize UART communication for GPS
    // M5Dial PortA: TX=G1, RX=G2 (Default UART1)
    GPS_Serial.begin(9600, SERIAL_8N1, 1, 2); // RX=GPIO2, TX=GPIO1 for PortA

    Serial.println("M5Dial Initialized.");
    Serial.println("Initializing GPS...");

    // Create the main canvas using M5Dial's display dimensions
    // This allocates memory for the off-screen buffer
    canvas.createSprite(M5Dial.Display.width(), M5Dial.Display.height());

    // Initialize the compass sensor
    Wire.begin(); // SDA=G32, SCL=G33 for M5Dial internal I2C
    qmc.init();

    Serial.println("Setup Complete. Waiting for GPS fix...");
}

void loop() {
    M5.update(); // Handle button/encoder events if needed

    // Read data from GPS module
    bool gpsDataUpdated = false;
    while (GPS_Serial.available() > 0) {
        if (gps.encode(GPS_Serial.read())) {
            gpsDataUpdated = true; // A complete sentence was processed
        }
    }

    // --- Heading Calculation ---
    qmc.read(&x, &y, &z);

    // Apply hard-iron offsets
    float x_corrected = x - offset_x;
    float y_corrected = y - offset_y;

    // Apply soft-iron scaling
    float x_calibrated = x_corrected * scale_x;
    float y_calibrated = y_corrected * scale_y;

    // Calculate the magnetic heading angle in degrees using calibrated values
    double magneticHeading = atan2(y_calibrated, x_calibrated) * (180.0 / M_PI);

    // Normalize magnetic heading to 0-360 degrees
    magneticHeading = fmod(magneticHeading + 360.0, 360.0);

    // Calculate True Heading by adding declination
    double trueHeading = magneticHeading + MAGNETIC_DECLINATION;

    // Normalize true heading to 0-360 degrees
    trueHeading = fmod(trueHeading + 360.0, 360.0);

    // Convert true heading to radians for drawing compass labels
    double trueHeading_rad = trueHeading * M_PI / 180.0;

    // --- Drawing (Double Buffering) ---

    // 1. Draw the complete next frame onto the off-screen canvas

    // Start by drawing the static background onto the canvas
    drawCompassBackgroundToCanvas(&canvas);

    // Draw the rotating compass labels (N, E, S, W) based on true heading
    drawCompassLabels(trueHeading_rad);

    // --- Target Arrow Logic ---
    double currentLat = 0.0;
    double currentLon = 0.0;
    bool locationIsValid = gps.location.isValid() && gps.location.age() < 2000; // Check validity and age (e.g., < 2 seconds)

    if (locationIsValid) {
        currentLat = gps.location.lat();
        currentLon = gps.location.lng();

        // Calculate bearing to target
        double targetBearing = calculateTargetBearing(currentLat, currentLon, TARGET_LAT, TARGET_LON);

        // Calculate the angle for the arrow relative to the device's heading
        double arrowAngle = targetBearing - trueHeading;

        // Normalize arrow angle to 0-360 degrees
        arrowAngle = fmod(arrowAngle + 360.0, 360.0);

        // Draw the target arrow onto the canvas
        drawTargetArrow(arrowAngle);

    } else {
        // Optional: Indicate that GPS is needed for the arrow (draw on canvas)
        canvas.setTextColor(TFT_RED, TFT_WHITE);
        canvas.setTextSize(1);
        canvas.setTextDatum(BC_DATUM); // Bottom Center
        canvas.drawString("Need GPS for Arrow", centerX, M5Dial.Display.height() - 5);
    }

    // --- Display GPS Info (Optional) ---
    // Draw GPS location inside the compass (on canvas)
    if (gps.location.isValid()) { // Show coordinates even if slightly old
        String lat_str = "Lat: " + String(gps.location.lat(), 4);
        String lng_str = "Lng: " + String(gps.location.lng(), 4);

        canvas.setTextColor(TFT_BLACK, TFT_WHITE);
        canvas.setTextSize(1);
        canvas.setTextDatum(MC_DATUM);

        canvas.drawString(lat_str, centerX, centerY - 10);
        canvas.drawString(lng_str, centerX, centerY + 10);
    } else {
        // Show "No GPS" if not valid (on canvas)
        canvas.setTextColor(TFT_RED, TFT_WHITE);
        canvas.setTextSize(1);
        canvas.setTextDatum(MC_DATUM);
        canvas.drawString("No GPS Fix", centerX, centerY);
    }

    // 2. Push the completed canvas to the display in one go
    // This should be fast and prevent flicker.
    canvas.pushSprite(0, 0);

    // Optional: Add a delay to control update rate
    delay(50); // ~20 FPS. Increase if flicker persists to see if it's rate-related.
}