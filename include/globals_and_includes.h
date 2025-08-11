#ifndef GLOBALS_AND_INCLUDES_H
#define GLOBALS_AND_INCLUDES_H

// ---- Standard & Core Libraries ----
#include "M5Dial.h"
#include <math.h>         // For cos(), sin(), atan2(), fmod(), M_PI

// ---- Sensor Libraries ----
#include <MechaQMC5883.h> // For the QMC5883 compass sensor
#include <TinyGPS++.h>
#include <HardwareSerial.h> // For GPS_Serial

// ---- Network & Web Libraries ----
#include <ArduinoJson.h>  // For parsing API responses
#include <FS.h>
#include <SPIFFS.h>

// ---- Custom Project Headers ----
#include "config.h" 
#include "icons.h"        
// Forward declare functions from drawing.h and calculations.h or include them if no circular dependencies
// #include "drawing.h"
// #include "calculations.h"


// M5Dial Display Canvas
extern M5Canvas canvas;

// Sensor Objects
extern MechaQMC5883 qmc;
extern TinyGPSPlus gps;
extern HardwareSerial GPS_Serial;

// Compass Display Geometry
extern int centerX, centerY, R;


// Target Coordinates
extern double TARGET_LAT;
extern double TARGET_LON;

extern String Setaddress; // Geocoded address string

// Heading Smoothing Variables
extern double smoothedHeadingX;
extern double smoothedHeadingY;
extern bool firstHeadingReading;

extern bool menuActive;
extern bool savedLocationsMenuActive;
extern bool settingsMenuActive; // Settings page active

// For popup notifications
extern bool popupActive;
extern uint32_t popupEndTime;
extern String popupMessage;
extern uint16_t popupTextColor;
extern uint16_t popupBgColor;
extern bool gpsinfoActive;

// Encoder Variables
static int encoder_click_accumulator = 0;
const int ENCODER_COUNTS_PER_DETENT = 4;

extern bool targetIsSet;

// User-configurable runtime settings
extern bool soundEnabled;   // When false, suppress UI beeps
extern bool touchEnabled;   // When false, ignore touch input
extern int screenBrightness; // Screen brightness level (0-255)
extern int soundLevel;      // Sound volume level (0-255)


// BLE position variables
extern bool blePositionSet;        // Whether we have a valid position from BLE
extern uint32_t blePositionTime;   // Time when the BLE position was last updated
extern double BLE_LAT;             // Latitude from BLE
extern double BLE_LON;             // Longitude from BLE

#endif // GLOBALS_AND_INCLUDES_H