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
    // popup lifetime handled later in drawPopupIfActive()

    if (menuActive) {
        handleMenuInput(); 
        drawAppMenu(canvas, centerX, centerY, R / 2, 32);
        drawPopupIfActive(canvas); // compose popup before single push
        canvas.pushSprite(0, 0); 
    } else if (savedLocationsMenuActive) {
        handleSavedLocationsInput();
        drawSavedLocationsMenu(canvas, centerX, centerY);
        drawPopupIfActive(canvas);
        canvas.pushSprite(0, 0); 
    } else if (gpsinfoActive) { // ADDED: Handle GPS info page
        drawGpsInfoPage(canvas, centerX, centerY);
        handleGpsInfoInput();
        drawPopupIfActive(canvas);
        canvas.pushSprite(0, 0); 
    } else if (bluetoothInfoActive) {
        // Follow same pattern as other pages
        showBluetoothInfoPage();
        handleBluetoothInfoInput();
        // No need for M5.update() here as it's already called at the beginning of loop()
    drawPopupIfActive(canvas);
    canvas.pushSprite(0,0);
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
    drawCompassBackgroundToCanvas(canvas, centerX, centerY, R, currentHeadingRadians);
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
    drawPopupIfActive(canvas);
    canvas.pushSprite(0, 0);

        if (M5.BtnA.wasHold()) { 
            Serial.println("Returning to menu...");
            menuActive = true;
            initMenu(); 
            M5Dial.Display.fillScreen(TFT_BLACK); 
        }
    }
    
}