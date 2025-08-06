#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include "globals_and_includes.h"

void setupBLE();
void notifySavedLocationsChange();
void checkBLEStatus(); // Call this from the main loop
bool isBlePositionValid(); // Returns true if we have a valid BLE position
void getBlePosition(double &lat, double &lon); // Get the current BLE position

#endif // BLUETOOTH_H