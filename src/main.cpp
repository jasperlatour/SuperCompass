bool menuActive = false;
bool savedLocationsMenuActive = false;
bool gpsinfoActive = false;
bool bluetoothInfoActive = false;
bool settingsMenuActive = false;
bool compassCalibrationActive = false;

// Runtime settings defaults
bool soundEnabled = true;
bool touchEnabled = true;
int screenBrightness = 128;   // Default brightness (0-255)
int soundLevel = 128;         // Default sound level (0-255)

// ---- Includes ----
#include "globals_and_includes.h" // Includes config.h
#include "sensor_processing.h"
#include "drawing.h"
#include "calculations.h"
#include "menu.h" 
#include "gpsinfo.h"
#include "bluetooth.h"
#include "page/bluetoothinfo.h"
#include "page/settings.h"
#include "page/calibration.h"
#include <esp_sleep.h> // For deep sleep wake-up management
#include <esp_system.h>

static const char* wakeupCauseToString(esp_sleep_wakeup_cause_t cause) {
    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
        case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCHPAD";
        case ESP_SLEEP_WAKEUP_ULP: return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
        case ESP_SLEEP_WAKEUP_UART: return "UART";
        case ESP_SLEEP_WAKEUP_WIFI: return "WIFI";
        case ESP_SLEEP_WAKEUP_COCPU: return "COCPU";
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG: return "COCPU_TRAP_TRIG";
        case ESP_SLEEP_WAKEUP_BT: return "BT";
        case ESP_SLEEP_WAKEUP_UNDEFINED: return "UNDEFINED";
        default: return "UNKNOWN";
    }
}

static const char* resetReasonToString(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN: return "UNKNOWN";
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "OTHER";
    }
}

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
    // Release global deep-sleep hold state first.
    gpio_deep_sleep_hold_dis();

    // CRITICAL: Release GPIO hold immediately after wake-up  
    // This allows us to control GPIO 46 again
    gpio_hold_dis(GPIO_NUM_46);
    
    Serial.begin(115200);
    delay(100); // Give serial time to initialize
    
    // Set power hold pin HIGH immediately to ensure device stays powered
    // This is critical after wake-up from deep sleep
    pinMode(GPIO_NUM_46, OUTPUT);
    digitalWrite(GPIO_NUM_46, HIGH);
    delay(10); // Ensure pin state is stable
    
    M5.begin();
    
    while (!Serial && millis() < 2000);
    
    // Check reset and wake-up reason
    esp_reset_reason_t reset_reason = esp_reset_reason();
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.print(F("Reset reason: "));
    Serial.print((int)reset_reason);
    Serial.print(F(" ("));
    Serial.print(resetReasonToString(reset_reason));
    Serial.println(F(")"));

    Serial.print(F("Wake-up cause: "));
    Serial.print((int)wakeup_reason);
    Serial.print(F(" ("));
    Serial.print(wakeupCauseToString(wakeup_reason));
    Serial.println(F(")"));

    bool wokeFromSleepByInput = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0)
                             || (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1)
                             || (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO);

    if (wokeFromSleepByInput) {
        Serial.println(F("\n--- M5Dial Waking Up From Button Press ---"));
        // Disable the wakeup sources so they don't interfere with normal operation
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    } else {
        Serial.println(F("\n--- M5Dial Navigator Starting Up ---"));
    }
    
    setupBLE();

    // Initialize M5Dial hardware, display, canvas, GPS, QMC compass, and display geometry
    initializeHardwareAndSensors(); 
    Serial.println(F("Hardware and Sensors Initialized."));
    
    // After wake-up, wait for button to be released to prevent unwanted actions
    if (wokeFromSleepByInput) {
        delay(200); // Give time for button state to stabilize
        while(M5.BtnA.isPressed()) {
            M5.update();
            delay(10);
        }
        Serial.println(F("Button released after wake-up."));
    }

    // Display initial status on M5Dial (centerX, centerY zijn nu gezet)
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(1);
    
    Serial.println("Mounting FileSystem...");
    if (!SPIFFS.begin(true)) {                       
        Serial.println("FATAL: FileSystem Mount Failed. Halting.");
        while (1) { delay(1000); } 
    }
    Serial.println("FileSystem mounted successfully.");
    loadSettings(); // load persisted sound/touch settings
    M5Dial.Display.wakeup();
    M5Dial.Display.setBrightness(screenBrightness);

    initMenu(); // Initialiseer het menu
    loadSavedLocations();

    // One-time boot animation: spin the dial through two easing-out rotations.
    // Skipped when waking from standby so wake-ups stay instant, not just cold boots.
    if (!wokeFromSleepByInput) {
        const int steps = 45;
        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / steps;
            float eased = 1.0f - powf(1.0f - t, 3.0f); // ease-out cubic
            double animHeadingRad = (720.0 * eased) * M_PI / 180.0; // two spins, easing to a stop
            canvas.fillSprite(TFT_BLACK);
            drawCompassBackgroundToCanvas(canvas, centerX, centerY, R, animHeadingRad);
            drawFixedHeadingMarker(canvas, centerX, centerY, R);
            canvas.pushSprite(0, 0);
            delay(12);
        }
    }

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
    } else if (settingsMenuActive) {
        handleSettingsInput();
        drawSettingsMenu(canvas, centerX, centerY);
        drawPopupIfActive(canvas);
        canvas.pushSprite(0,0);
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
    } else if (compassCalibrationActive) {
        handleCompassCalibrationInput();
        drawCompassCalibration(canvas, centerX, centerY, R);
        drawPopupIfActive(canvas);
        canvas.pushSprite(0, 0);
    } else if (M5.BtnA.wasPressed()) { // ADDED: Handle button A press
        Serial.println("Button A pressed");
        menuActive = true; // Set menuActive to true to show the menu
        initMenu(); // Reset menu state
        // (No direct display clear here - drawAppMenu() clears the off-screen
        // canvas before the next pushSprite, avoiding a visible black flash.)
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

        double distanceToTargetMeters = 0.0;
        if (locationIsValid && targetIsSet) {
            distanceToTargetMeters = calculateDistanceMeters(currentLat, currentLon, TARGET_LAT, TARGET_LON);
        }

        canvas.fillSprite(TFT_BLACK); // Begin met een schone canvas
        drawCompassBackgroundToCanvas(canvas, centerX, centerY, R, currentHeadingRadians);
        drawFixedHeadingMarker(canvas, centerX, centerY, R);
        drawCompassLabels(canvas, currentHeadingRadians, centerX, centerY, R);

        if (targetIsSet && locationIsValid) {
            drawTargetArrow(canvas, arrowAngleOnCompassDegrees, centerX, centerY, R);
        }

        // Heading readout is drawn on top (opaque background) so it stays crisp
        // even where the target arrow's shaft passes behind it near center.
        drawHeadingValue(canvas, currentHeadingDegrees, centerX, centerY);
        drawGpsInfo(canvas, gps, centerX, centerY);
        drawTargetInfoBanner(canvas, centerX, centerY, R, targetIsSet, locationIsValid, Setaddress, distanceToTargetMeters);

    drawPopupIfActive(canvas);
    canvas.pushSprite(0, 0);

        if (M5.BtnA.wasHold()) { 
            Serial.println("Returning to menu...");
            menuActive = true;
            initMenu();
        }
    }
    
}