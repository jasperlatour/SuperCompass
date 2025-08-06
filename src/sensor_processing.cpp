#include "sensor_processing.h"
#include "gpsinfo.h"
#include "bluetooth.h"
// Assumes globals_and_includes.h is included via sensor_processing.h
// Access to global objects 'M5Dial', 'canvas', 'GPS_Serial', 'qmc'
// Access to global variables 'centerX', 'centerY', 'R', 'firstHeadingReading', 'smoothedHeadingX/Y'
// Access to constants from 'config.h' like 'offset_x', 'MAGNETIC_DECLINATION', 'HEADING_SMOOTHING_FACTOR'

void initializeHardwareAndSensors() {
    auto cfg = M5.config(); // Get M5Dial default configuration
    // Consider enabling power for PortA if GPS is connected there and needs it.
    // cfg.external_power = true; // If PortA needs to supply power via M5Dial control
    M5Dial.begin(cfg, true, true); // Initialize M5Dial, with I2C and Display by default
    M5Dial.Encoder.begin(); // Initialize the encoder
    
    GPS_Serial.begin(9600, SERIAL_8N1, 1, 2); // RX=GPIO1, TX=GPIO2 (as per your original code)
    Serial.println(F("GPS Serial (UART1) configured on RX=1, TX=2 at 9600 baud."));

    canvas.createSprite(M5Dial.Display.width(), M5Dial.Display.height());
    if (canvas.width() == 0 || canvas.height() == 0) {
        Serial.println(F("Canvas creation failed!"));
        M5Dial.Display.drawString("Canvas Fail!", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
        while(1) delay(1000); // Halt
    } else {
        Serial.println(F("Canvas created successfully."));
    }


    Wire.begin(); // Initialize I2C for QMC5883L
    qmc.init();   // Initialize QMC5883L compass sensor
    // Optional: Set compass mode, output data rate, range, oversampling
    // qmc.setMode(Mode_Continuous, ODR_200Hz, RNG_8G, OSR_512); 
    Serial.println(F("QMC5883L Compass Initialized."));

    // Compass Display Geometry
    centerX = M5Dial.Display.width() / 2;
    centerY = M5Dial.Display.height() / 2;
    R = (M5Dial.Display.height() / 2) - 10; // Radius for compass rose, with a small margin

    firstHeadingReading = true; // Reset smoothing
    smoothedHeadingX = 0.0;
    smoothedHeadingY = 0.0;
}

void processGpsData() {
    bool gpsUpdated = false;

    // Process any available GPS data
    while (GPS_Serial.available() > 0) {
        gps.encode(GPS_Serial.read());
        if (gps.location.isUpdated()) {
            gpsUpdated = true;
            // Update GPS location
            setLatitude(gps.location.lat());
            setLongitude(gps.location.lng());
            setAltitude(gps.altitude.meters());
            setSpeed(gps.speed.kmph());
            setSatellitesInView(gps.satellites.value());
            setFixQuality(gps.location.FixQuality());
        }
    }

    // If we didn't get a GPS update and GPS location is not valid or is too old
    if (!gpsUpdated && (!gps.location.isValid() || gps.location.age() > 10000)) {
        // Check if we have a valid BLE position
        if (isBlePositionValid()) {
            double lat, lon;
            getBlePosition(lat, lon);
            
            // Use the BLE position
            setLatitude(lat);
            setLongitude(lon);
            
            // Set fix quality to indicate BLE source (using value 9 to distinguish from GPS quality)
            setFixQuality(9);
            
            Serial.println("Using BLE position instead of GPS");
        }
    }
}

double calculateRawTrueHeading() {
    int raw_x, raw_y, raw_z;
    float heading_deg;

    qmc.read(&raw_x, &raw_y, &raw_z); // Read raw compass values

    // Apply calibration offsets and scales from config.h
    // It's crucial these are correctly determined for your specific sensor.
    double calibrated_x = (double)(raw_x - offset_x) * scale_x;
    double calibrated_y = (double)(raw_y - offset_y) * scale_y;

    // Calculate heading from calibrated values
    heading_deg = atan2(calibrated_y, calibrated_x) * (180.0 / M_PI);

    // Apply magnetic declination (from config.h)
    heading_deg += MAGNETIC_DECLINATION;

    // Normalize heading to 0-360 degrees
    if (heading_deg < 0) {
        heading_deg += 360.0;
    }
    if (heading_deg >= 360.0) {
        heading_deg -= 360.0;
    }
    
    return heading_deg;
}

double getSmoothedHeadingDegrees() {
    double rawTrueHeading_deg = calculateRawTrueHeading();
    double rawTrueHeading_rad = rawTrueHeading_deg * M_PI / 180.0;

    // Convert heading to Cartesian coordinates for smoothing
    double currentX_component = cos(rawTrueHeading_rad);
    double currentY_component = sin(rawTrueHeading_rad);

    if (firstHeadingReading) {
        smoothedHeadingX = currentX_component;
        smoothedHeadingY = currentY_component;
        firstHeadingReading = false;
    } else {
        // Exponential moving average
        smoothedHeadingX = (HEADING_SMOOTHING_FACTOR * currentX_component) + ((1.0 - HEADING_SMOOTHING_FACTOR) * smoothedHeadingX);
        smoothedHeadingY = (HEADING_SMOOTHING_FACTOR * currentY_component) + ((1.0 - HEADING_SMOOTHING_FACTOR) * smoothedHeadingY);
    }

    // Convert smoothed Cartesian coordinates back to polar (angle)
    double smoothedHeading_rad = atan2(smoothedHeadingY, smoothedHeadingX);
    double smoothedHeading_deg = fmod((smoothedHeading_rad * 180.0 / M_PI) + 360.0, 360.0); // Normalize to 0-360

    return smoothedHeading_deg;
}