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
    // Handle popup without disrupting normal screen drawing
    if (popupActive) {
        // Check if the popup should be cleared
        if (millis() > popupEndTime) {
            Serial.println("Clearing popup notification");
            popupActive = false;
            // No need to clear the screen as we'll continue with normal drawing
        } else {
            // Popup is still active, render popup over whatever is currently shown
            // We'll still continue with normal screen rendering to keep the background up to date
            // But we'll overlay the popup at the end of the loop

        }
    }

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
        // Follow same pattern as other pages
        showBluetoothInfoPage();
        handleBluetoothInfoInput();
        // No need for M5.update() here as it's already called at the beginning of loop()
    } else if (M5.BtnA.wasPressed()) { // ADDED: Handle button A press
        Serial.println("Button A pressed");
        menuActive = true; // Set menuActive to true to show the menu
        initMenu(); // Reset menu state
        M5Dial.Display.fillScreen(TFT_BLACK); // Clear screen before drawing menu
    }else {
        processGpsData();
        // Check if we need to save BLE-updated locations
        checkBLEStatus();

        double currentHeadingDegrees = getSmoothedHeadingDegrees();
        double currentHeadingRadians = currentHeadingDegrees * M_PI / 180.0;

        double targetBearingDegrees = 0.0;
        double arrowAngleOnCompassDegrees = 0.0;

        // Check for valid location from either GPS or BLE
        bool gpsLocationIsValid = gps.location.isValid() && gps.location.age() < 3000;
        bool bleLocationIsValid = isBlePositionValid();
        bool locationIsValid = gpsLocationIsValid || bleLocationIsValid;

        targetIsSet = (TARGET_LAT != 0.0 || TARGET_LON != 0.0);

        double currentLat = 0.0;
        double currentLon = 0.0;

        if (locationIsValid) {
            if (gpsLocationIsValid) {
                currentLat = gps.location.lat();
                currentLon = gps.location.lng();
            } else { // bleLocationIsValid must be true
                getBlePosition(currentLat, currentLon);
            }
        }

        if (locationIsValid && targetIsSet) {
            targetBearingDegrees = calculateTargetBearing(
                currentLat, currentLon,
                TARGET_LAT, TARGET_LON
            );
            arrowAngleOnCompassDegrees = targetBearingDegrees - currentHeadingDegrees;
            arrowAngleOnCompassDegrees = fmod(arrowAngleOnCompassDegrees + 360.0, 360.0);
        }

        canvas.fillSprite(TFT_BLACK); // Begin met een schone canvas
        drawCompassBackgroundToCanvas(canvas, centerX, centerY, R);
        drawCompassLabels(canvas, currentHeadingRadians, centerX, centerY, R);
        drawGpsInfo(canvas, gps, centerX, centerY);

        if (!targetIsSet) {
            drawStatusMessage(canvas, "No Target", centerX, centerY + 50, TFT_RED, TFT_WHITE);
        } else {
            if (locationIsValid) {
                drawTargetArrow(canvas, arrowAngleOnCompassDegrees, centerX, centerY, R);
            }
            //draw target name
            drawStatusMessage(canvas, ("Target: " + Setaddress).c_str(), centerX, centerY + 50, TFT_BLUE, TFT_WHITE);
        }
        canvas.pushSprite(0, 0);

        if (M5.BtnA.wasHold()) { 
            Serial.println("Returning to menu...");
            menuActive = true;
            initMenu(); 
            M5Dial.Display.fillScreen(TFT_BLACK); 
        }
    }
    
    // If popup is active, redraw it over whatever was just drawn
    if (popupActive) {
        // Save current canvas properties
        int oldTextSize = canvas.getTextSizeX();
        uint8_t oldDatum = canvas.getTextDatum();
       
        
        // Calculate the popup size and position
        canvas.setTextSize(2);
        int popupWidth = canvas.textWidth(popupMessage.c_str()) + 40;
        int popupHeight = 50;
        int popupX = (canvas.width() - popupWidth) / 2;
        int popupY = (canvas.height() - popupHeight) / 2;
        
        // Draw the popup with semi-transparent effect
        canvas.fillRoundRect(popupX, popupY, popupWidth, popupHeight, 15, popupBgColor);
        
        // Add a white border (2 pixels)
        canvas.drawRoundRect(popupX, popupY, popupWidth, popupHeight, 15, TFT_WHITE);
        canvas.drawRoundRect(popupX+1, popupY+1, popupWidth-2, popupHeight-2, 14, TFT_WHITE);
        
        // Draw text
        canvas.setTextColor(popupTextColor);
        canvas.setTextDatum(MC_DATUM);
        canvas.drawString(popupMessage, canvas.width() / 2, canvas.height() / 2);
        
        // Restore original canvas properties
        canvas.setTextSize(oldTextSize);
        canvas.setTextDatum(oldDatum);
        
        // Push changes to display
        canvas.pushSprite(0, 0);
    }
}