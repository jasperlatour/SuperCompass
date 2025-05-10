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
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>   // For making API requests
#include <ArduinoJson.h>  // For parsing API responses

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

// Web Server
extern WebServer server;

// Target Coordinates
extern double TARGET_LAT;
extern double TARGET_LON;

// Network Information
extern String currentNetworkIP;
extern String Setaddress; // Geocoded address string

// Heading Smoothing Variables
extern double smoothedHeadingX;
extern double smoothedHeadingY;
extern bool firstHeadingReading;

extern bool menuActive;
extern bool savedLocationsMenuActive;

// Encoder Variables
static int encoder_click_accumulator = 0;
const int ENCODER_COUNTS_PER_DETENT = 4;

extern bool targetIsSet;

#endif // GLOBALS_AND_INCLUDES_H