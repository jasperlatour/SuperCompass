#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include "globals_and_includes.h"
#include "ui/drawing.h"

void setupBLE();
void notifySavedLocationsChange();
void checkBLEStatus(); // Call this from the main loop
bool isBlePositionValid(); // Returns true if we have a valid BLE position
void getBlePosition(double &lat, double &lon); // Get the current BLE position
void disconnectBluetooth(); // Force disconnect current BLE client

// New helper functions for improved reliability
// Publish the current target value to the target characteristic (read + notify)
void publishTargetCharacteristic();
// Publish READY state (true when device finished initializing data)
void publishReady(bool ready);


#endif // BLUETOOTH_H