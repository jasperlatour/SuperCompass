#ifndef SETTINGS_H
#define SETTINGS_H

#include "globals_and_includes.h"

void initSettingsMenu();
void drawSettingsMenu(M5Canvas &canvas, int centerX, int centerY);
void handleSettingsInput();
// Optional external access
void loadSettings();

#endif // SETTINGS_H
