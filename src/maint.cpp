#include "M5Dial.h"
#include <math.h>          // For cos(), sin(), atan2(), fmod(), M_PI
#include <MechaQMC5883.h>  // For the QMC5883 compass sensor
#include <TinyGPS++.h>
#include <HardwareSerial.h>

#include "drawing.h"    
#include "calculations.h" // For calculateTargetBearing   


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
const float MAGNETIC_DECLINATION = 2; // Example: 1.5° West declination
const float offset_x = 345.0;
const float offset_y = -196.0;
const float scale_x = 0.996644295;
const float scale_y = 1.003378378;
// --- End User Configuration ---


void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, true); 
    M5Dial.Display.setBrightness(24);

    // Initialize GPS Serial
    GPS_Serial.begin(9600, SERIAL_8N1, 1, 2); 

    // Create the canvas (off-screen buffer)
    canvas.createSprite(M5Dial.Display.width(), M5Dial.Display.height());

    // Initialize the compass sensor
    Wire.begin();
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
    double trueHeading = calculateTrueHeading(qmc, offset_x, offset_y, scale_x, scale_y, MAGNETIC_DECLINATION);
    double trueHeading_rad = trueHeading * M_PI / 180.0;

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
    drawCompassBackgroundToCanvas(canvas, centerX, centerY, R);
    drawCompassLabels(canvas, trueHeading_rad, centerX, centerY, R);
    drawGpsInfo(canvas, gps, centerX, centerY);
    if (locationIsValid) {
        drawTargetArrow(canvas, arrowAngle, centerX, centerY, R);
    } else {
        // Use the new status message function
        drawStatusMessage(canvas, "Need GPS for Arrow", centerX, centerY + 30, 2, TFT_WHITE);
    }

    // --- Push Sprite to Screen ---
    canvas.pushSprite(0, 0);

    delay(50); // ~20 FPS
}