#ifndef SENSOR_PROCESSING_H
#define SENSOR_PROCESSING_H

#include "globals_and_includes.h" // For sensor objects, M5Dial, config constants

// Initializes M5Dial core, display, canvas, GPS serial, compass, and display geometry
void initializeHardwareAndSensors();

// Reads available data from GPS_Serial and updates the global 'gps' object
void processGpsData();

// Calculates raw heading from compass, applies calibration and declination
double calculateRawTrueHeading();

// Applies smoothing to the raw heading and returns the smoothed heading in degrees
double getSmoothedHeadingDegrees();

#endif // SENSOR_PROCESSING_H