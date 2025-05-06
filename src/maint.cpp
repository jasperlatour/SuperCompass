#include "M5Dial.h"
#include <math.h>          // For cos(), sin(), atan2(), fmod(), M_PI
#include <MechaQMC5883.h>  // For the QMC5883 compass sensor
#include <TinyGPS++.h>
#include <HardwareSerial.h>

#include "drawing.h"    
#include "calculations.h" // For calculateTargetBearing   
#include "config.h"

//webserver
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h> // Optional: If serving HTML from filesystem


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

AsyncWebServer server(80);

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

    // --- Wi-Fi Connection ---
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status()!= WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        // Optional: Add timeout logic
    }
    Serial.println("\nConnected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

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
       // 1. Get the RAW heading from the sensor
       double rawTrueHeading = calculateTrueHeading(qmc, offset_x, offset_y, scale_x, scale_y, MAGNETIC_DECLINATION);
       double rawTrueHeading_rad = rawTrueHeading * M_PI / 180.0;
   
       // 2. Calculate current heading components
       double currentX = cos(rawTrueHeading_rad);
       double currentY = sin(rawTrueHeading_rad);
   
       // 3. Apply Exponential Moving Average (EMA) to components
       if (firstHeadingReading) {
           // Initialize with the first reading
           smoothedHeadingX = currentX;
           smoothedHeadingY = currentY;
           firstHeadingReading = false;
       } else {
           // Apply EMA formula
           smoothedHeadingX = HEADING_SMOOTHING_FACTOR * currentX + (1.0 - HEADING_SMOOTHING_FACTOR) * smoothedHeadingX;
           smoothedHeadingY = HEADING_SMOOTHING_FACTOR * currentY + (1.0 - HEADING_SMOOTHING_FACTOR) * smoothedHeadingY;
       }
   
       // 4. Calculate the smoothed heading angle from the averaged components
       double smoothedHeading_rad = atan2(smoothedHeadingY, smoothedHeadingX);
   
       // 5. Convert smoothed heading back to degrees (optional, if needed elsewhere)
       //    Ensure it's normalized between 0 and 360
       double smoothedHeading_deg = fmod(smoothedHeading_rad * 180.0 / M_PI + 360.0, 360.0);
   
       // --- Use the SMOOTHED heading for further calculations and drawing ---
       double trueHeading_rad = smoothedHeading_rad; // Use the smoothed radians
       double trueHeading = smoothedHeading_deg;     // Use the smoothed degrees (if needed)
   
   
       // --- Target Calculation ---
       double targetBearing = 0.0;
       double arrowAngle = 0.0;
       bool locationIsValid = gps.location.isValid() && gps.location.age() < 2000; // Valid & recent fix
   
       if (locationIsValid) {
           double currentLat = gps.location.lat();
           double currentLon = gps.location.lng();
           targetBearing = calculateTargetBearing(currentLat, currentLon, TARGET_LAT, TARGET_LON);
           // Calculate arrow angle based on SMOOTHED heading
           arrowAngle = targetBearing - trueHeading; // Using smoothed degrees here
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