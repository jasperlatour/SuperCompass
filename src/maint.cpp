#include "M5Dial.h"
#include <math.h>          // For cos(), sin(), atan2(), fmod(), M_PI
#include <MechaQMC5883.h>  // For the QMC5883 compass sensor
#include <TinyGPS++.h>
#include <HardwareSerial.h>

#include "drawing.h"       // <<< Include the new drawing header

// Define PI if not already defined (it usually is in math.h)
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
double a;
int x, y, z;

// Compass Display Geometry (calculated in setup/loop)
int centerX, centerY, R;

// --- User Configuration ---
const double TARGET_LAT = 48.8584;   // Target Latitude (e.g., Eiffel Tower)
const double TARGET_LON = 2.2945;    // Target Longitude (e.g., Eiffel Tower)
const float MAGNETIC_DECLINATION = -1.5; // Example: 1.5° West declination
const float offset_x = 345.0;
const float offset_y = -196.0;
const float scale_x = 0.996644295;
const float scale_y = 1.003378378;
// --- End User Configuration ---


// Function to calculate the bearing (keep this logic here or move to another utility file if desired)
double calculateTargetBearing(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg) {
    // Convert degrees to radians
    double lat1Rad = lat1Deg * M_PI / 180.0;
    double lon1Rad = lon1Deg * M_PI / 180.0;
    double lat2Rad = lat2Deg * M_PI / 180.0;
    double lon2Rad = lon2Deg * M_PI / 180.0;
    double dLonRad = lon2Rad - lon1Rad;
    double y = sin(dLonRad) * cos(lat2Rad);
    double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(dLonRad);
    double bearingRad = atan2(y, x);
    double bearingDeg = bearingRad * 180.0 / M_PI;
    bearingDeg = fmod(bearingDeg + 360.0, 360.0); // Normalize to 0-360
    return bearingDeg;
}


void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, true); // LCD, Serial enabled
    M5Dial.Display.setBrightness(24);

    // Initialize GPS Serial
    GPS_Serial.begin(9600, SERIAL_8N1, 1, 2); // RX=GPIO2, TX=GPIO1 for PortA

    Serial.println("M5Dial Initialized.");
    Serial.println("Initializing GPS...");

    // Create the canvas (off-screen buffer)
    canvas.createSprite(M5Dial.Display.width(), M5Dial.Display.height());

    // Initialize the compass sensor
    Wire.begin(); // SDA=G32, SCL=G33 for M5Dial internal I2C
    qmc.init();

    // Calculate compass geometry once
    centerX = M5Dial.Display.width() / 2;
    centerY = M5Dial.Display.height() / 2;
    R = M5Dial.Display.height() / 2; // Assuming a square or using height for radius

    Serial.println("Setup Complete. Waiting for GPS fix...");
}

void loop() {
    M5.update(); // Handle M5Dial events

    // --- GPS Processing ---
    bool gpsDataUpdated = false;
    while (GPS_Serial.available() > 0) {
        if (gps.encode(GPS_Serial.read())) {
            gpsDataUpdated = true; // A complete sentence was processed
        }
    }

    // --- Heading Calculation ---
    qmc.read(&x, &y, &z);
    float x_corrected = x - offset_x;
    float y_corrected = y - offset_y;
    float x_calibrated = x_corrected * scale_x;
    float y_calibrated = y_corrected * scale_y;
    double magneticHeading = atan2(y_calibrated, x_calibrated) * (180.0 / M_PI);
    magneticHeading = fmod(magneticHeading + 360.0, 360.0); // Normalize 0-360
    double trueHeading = magneticHeading + MAGNETIC_DECLINATION;
    trueHeading = fmod(trueHeading + 360.0, 360.0);       // Normalize 0-360
    double trueHeading_rad = trueHeading * M_PI / 180.0; // To radians for drawing labels

    // --- Target Calculation ---
    double targetBearing = 0.0;
    double arrowAngle = 0.0;
    bool locationIsValid = gps.location.isValid() && gps.location.age() < 2000; // Valid & recent fix

    if (locationIsValid) {
        double currentLat = gps.location.lat();
        double currentLon = gps.location.lng();
        targetBearing = calculateTargetBearing(currentLat, currentLon, TARGET_LAT, TARGET_LON);
        arrowAngle = targetBearing - trueHeading;
        arrowAngle = fmod(arrowAngle + 360.0, 360.0); // Normalize arrow angle 0-360
    }

    // --- Drawing ---
    // (All drawing happens on the 'canvas' sprite first)

    // 1. Draw the static background
    drawCompassBackgroundToCanvas(canvas, centerX, centerY, R);

    // 2. Draw the rotating compass labels
    drawCompassLabels(canvas, trueHeading_rad, centerX, centerY, R);

    // 3. Draw the target arrow or status message
    if (locationIsValid) {
        drawTargetArrow(canvas, arrowAngle, centerX, centerY, R);
    } else {
        // Use the new status message function
        drawStatusMessage(canvas, "Need GPS for Arrow", centerX, M5Dial.Display.height() - 5, 2, TFT_WHITE);
    }

    // 4. Draw GPS Info (Coordinates or "No GPS Fix")
    drawGpsInfo(canvas, gps, centerX, centerY);

    // --- Push Sprite to Screen ---
    // Push the completed canvas to the display in one go to prevent flicker
    canvas.pushSprite(0, 0);

    // Optional delay
    delay(50); // ~20 FPS
}