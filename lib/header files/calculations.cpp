#include "calculations.h"

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

double calculateTrueHeading(MechaQMC5883 &sensor, float offsetX, float offsetY, float scaleX, float scaleY, float declination) {
    int x, y, z; // Variables to store raw sensor readings

    // Read the sensor
    sensor.read(&x, &y, &z);

    // Apply calibration offsets and scaling
    float x_corrected = (float)x - offsetX; // Cast x to float for calculation
    float y_corrected = (float)y - offsetY; // Cast y to float for calculation
    float x_calibrated = x_corrected * scaleX;
    float y_calibrated = y_corrected * scaleY;

    // Calculate magnetic heading using atan2
    // atan2 gives the angle in radians between the positive x-axis and the point (x, y)
    // Result is typically in the range [-pi, pi]
    double magneticHeading = atan2(y_calibrated, x_calibrated) * (180.0 / M_PI);

    // Normalize magnetic heading to the range [0, 360) degrees
    magneticHeading = fmod(magneticHeading + 360.0, 360.0);

    // Calculate true heading by adding declination
    double trueHeading = magneticHeading + declination;

    // Normalize true heading to the range [0, 360) degrees
    trueHeading = fmod(trueHeading + 360.0, 360.0);

    return trueHeading;
}