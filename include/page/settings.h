#ifndef SETTINGS_H
#define SETTINGS_H

#include "globals_and_includes.h"

void initSettingsMenu();
void drawSettingsMenu(M5Canvas &canvas, int centerX, int centerY);
void handleSettingsInput();
// Optional external access
void loadSettings();

// Updates the magnetometer calibration (offset_x/offset_y/scale_x/scale_y),
// applies it immediately, and persists it to settings.json.
void saveCalibrationValues(int offsetX, int offsetY, float scaleX, float scaleY);

#endif // SETTINGS_H
