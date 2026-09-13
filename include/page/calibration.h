#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "globals_and_includes.h"

// In-menu magnetometer calibration: have the user rotate the device through a
// full circle while sampling raw X/Y extents, then derive offset/scale
// (hard/soft-iron correction) and persist them via saveCalibrationValues().

void initCompassCalibration();
void drawCompassCalibration(M5Canvas &canvas, int centerX, int centerY, int R);
void handleCompassCalibrationInput();

#endif // CALIBRATION_H
