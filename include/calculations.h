#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <MechaQMC5883.h> // Needed for the MechaQMC5883 type

// calculate get bearings
// This function calculates the bearing from one geographical point to another using the Haversine formula.
/**
 * * @brief Calculate the bearing from one geographical point to another.
 * * @param lat1Deg Latitude of the first point in degrees.
 * * @param lon1Deg Longitude of the first point in degrees.
 * * @param lat2Deg Latitude of the second point in degrees.
 * * @param lon2Deg Longitude of the second point in degrees.
 * * @return The bearing in degrees from the first point to the second point.
 */
double calculateTargetBearing(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg);

/**
 * @brief Calculates the true heading based on sensor readings, calibration, and declination.
 *
 * @param sensor A reference to the initialized MechaQMC5883 sensor object.
 * @param offsetX The calibration offset for the X-axis.
 * @param offsetY The calibration offset for the Y-axis.
 * @param scaleX The calibration scale factor for the X-axis.
 * @param scaleY The calibration scale factor for the Y-axis.
 * @param declination The magnetic declination for the current location in degrees.
 * @return The calculated true heading in degrees (0-360).
 */
double calculateTrueHeading(MechaQMC5883 &sensor, float offsetX, float offsetY, float scaleX, float scaleY, float declination);

#endif 