bool menuActive = false;
bool savedLocationsMenuActive = false;
bool gpsinfoActive = false;
bool bluetoothInfoActive = false;

// ---- Includes ----
#include "globals_and_includes.h" // Includes config.h
#include "sensor_processing.h"
#include "drawing.h"
#include "calculations.h"
#include "menu.h" 
#include "gpsinfo.h"
#include "bluetooth.h"
#include "page/bluetoothinfo.h"

// ---- Global Object Definitions (reeds 'extern' verklaard in globals_and_includes.h) ----
M5Canvas canvas(&M5Dial.Display);
MechaQMC5883 qmc;
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);

double TARGET_LAT = 0.0;
double TARGET_LON = 0.0;
String currentNetworkIP = "N/A";
String Setaddress = "";
bool targetIsSet = false;

int centerX, centerY, R;

void setup() {
    Serial.begin(115200);
    pinMode(GPIO_NUM_46, OUTPUT);
    digitalWrite(GPIO_NUM_46,HIGH);
    while (!Serial && millis() < 2000);
    Serial.println(F("\n--- M5Dial Navigator Starting Up ---"));
    setupBLE();

    // Initialize M5Dial hardware, display, canvas, GPS, QMC compass, and display geometry
    initializeHardwareAndSensors(); 
    Serial.println(F("Hardware and Sensors Initialized."));

    // Display initial status on M5Dial (centerX, centerY zijn nu gezet)
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(1);
    
    Serial.println("Mounting FileSystem...");
    if (!SPIFFS.begin(true)) {                       
        Serial.println("FATAL: FileSystem Mount Failed. Halting.");
        while (1) { delay(1000); } 
    }
    Serial.println("FileSystem mounted successfully.");

    initMenu(); // Initialiseer het menu
    loadSavedLocations();

    // Bereid display voor main loop (kan overschreven worden door menu of andere schermen)
    M5Dial.Display.fillScreen(TFT_BLACK);
    M5Dial.Display.setTextDatum(TL_DATUM);
    M5Dial.Display.setTextSize(1);
    Serial.println(F("Setup complete. Entering main loop."));
}

// ---- MAIN LOOP: Runs repeatedly ----
void loop() {
    M5.update();          // Essentieel voor knoppen en encoder updates

    if (menuActive) {
        handleMenuInput(); 
        drawAppMenu(canvas, centerX, centerY, R / 2, 32);
        canvas.pushSprite(0, 0); 
    } else if (savedLocationsMenuActive) {
        handleSavedLocationsInput();
        drawSavedLocationsMenu(canvas, centerX, centerY);
        canvas.pushSprite(0, 0); 
    } else if (gpsinfoActive) { // ADDED: Handle GPS info page
        drawGpsInfoPage(canvas, centerX, centerY);
        handleGpsInfoInput();
        canvas.pushSprite(0, 0); 
    } else if (bluetoothInfoActive) {
        // Make sure M5Dial state is updated before checking input
        M5.update();
        showBluetoothInfoPage();
        handleBluetoothInfoInput();
    } else if (M5.BtnA.wasPressed()) { // ADDED: Handle button A press
        Serial.println("Button A pressed");
        menuActive = true; // Set menuActive to true to show the menu
        initMenu(); // Reset menu state
        M5Dial.Display.fillScreen(TFT_BLACK); // Clear screen before drawing menu
    } else {
        processGpsData();
        
        // Check if we need to save BLE-updated locations
        checkBLEStatus();

        double currentHeadingDegrees = getSmoothedHeadingDegrees();
        double currentHeadingRadians = currentHeadingDegrees * M_PI / 180.0;

        double targetBearingDegrees = 0.0;
        double arrowAngleOnCompassDegrees = 0.0;

        bool locationIsValid = gps.location.isValid() && gps.location.age() < 3000;
        // Don't redefine the global targetIsSet variable, just use it directly
        // Also check if the coordinates are valid
        targetIsSet = (TARGET_LAT != 0.0 || TARGET_LON != 0.0);

        if (locationIsValid && targetIsSet) {
            targetBearingDegrees = calculateTargetBearing(
                gps.location.lat(), gps.location.lng(),
                TARGET_LAT, TARGET_LON
            );
            arrowAngleOnCompassDegrees = targetBearingDegrees - currentHeadingDegrees;
            arrowAngleOnCompassDegrees = fmod(arrowAngleOnCompassDegrees + 360.0, 360.0);
        }

        canvas.fillSprite(TFT_BLACK); // Begin met een schone canvas
        drawCompassBackgroundToCanvas(canvas, centerX, centerY, R);
        drawCompassLabels(canvas, currentHeadingRadians, centerX, centerY, R);
        drawGpsInfo(canvas, gps, centerX, centerY);

        if (!locationIsValid) {
            String statusMsg = "Acquiring GPS...";
            if (Setaddress != "" && !targetIsSet) {
                statusMsg = Setaddress.substring(0, min((int)Setaddress.length(), 25));
                if (Setaddress.length() > 25) statusMsg += "...";
            } else if (Setaddress != "" && targetIsSet) {
                statusMsg = "Target: ";
                statusMsg += Setaddress.substring(0, min((int)Setaddress.length(), 20));
                if (Setaddress.length() > 20) statusMsg += "...";
            }
            drawStatusMessage(canvas, statusMsg.c_str(), centerX, centerY + R - 40, 1, TFT_ORANGE);
        } else if (!targetIsSet) {
            drawStatusMessage(canvas, "Set Target via WiFi:", centerX, centerY + R - 70, 1, TFT_CYAN);
            if (currentNetworkIP != "N/A") {
                drawStatusMessage(canvas, currentNetworkIP.c_str(), centerX, centerY + R - 50, 1, TFT_CYAN);
            } else {
                drawStatusMessage(canvas, "No Network IP", centerX, centerY + R - 50, 1, TFT_RED);
            }
        } else { // Location is valid AND target is set
            drawTargetArrow(canvas, arrowAngleOnCompassDegrees, centerX, centerY, R);
        }
        canvas.pushSprite(0, 0);

        
        if (M5.BtnA.wasHold()) { 
            Serial.println("Returning to menu...");
            menuActive = true;
            initMenu(); 
            M5Dial.Display.fillScreen(TFT_BLACK); 
        }
    }

}