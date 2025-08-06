#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include "globals_and_includes.h"

void setupBLE();
void notifySavedLocationsChange();
void checkBLEStatus(); // Call this from the main loop

#endif // BLUETOOTH_H