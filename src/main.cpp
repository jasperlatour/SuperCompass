bool menuActive = false;
bool savedLocationsMenuActive = false;

// ---- Includes ----
#include "globals_and_includes.h" // Includes config.h
#include "web_handlers.h"
#include "wifi_manager.h"
#include "sensor_processing.h"
#include "drawing.h"
#include "calculations.h"
#include "menu.h" 


// ---- Global Object Definitions (reeds 'extern' verklaard in globals_and_includes.h) ----
M5Canvas canvas(&M5Dial.Display);
MechaQMC5883 qmc;
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);
WebServer server(80);

double TARGET_LAT = 0.0;
double TARGET_LON = 0.0;
String currentNetworkIP = "N/A";
String Setaddress = "";
bool targetIsSet = false;



// Compass Display Geometry (geïnitialiseerd in initializeHardwareAndSensors)
int centerX, centerY, R;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    Serial.println(F("\n--- M5Dial Navigator Starting Up ---"));

    // Initialize M5Dial hardware, display, canvas, GPS, QMC compass, and display geometry
    initializeHardwareAndSensors(); // Zorg dat M5.begin() hierin zit of ervoor
                                   // M5.begin() initialiseert ook de Encoder.
    Serial.println(F("Hardware and Sensors Initialized."));

    // Display initial status on M5Dial (centerX, centerY zijn nu gezet)
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Sensors OK", centerX, M5Dial.Display.height() / 2 - 55);

    // Connect to WiFi (STA mode) or fallback to AP mode
    bool networkReady = connectWiFiWithFallback();
    Serial.println(networkReady ? F("Network is ready.") : F("Network setup failed or is unavailable."));

    if (networkReady) {
        setupServerRoutes();
        M5Dial.Display.drawString("Web Server Active", centerX, M5Dial.Display.height() / 2 + 35);
        Serial.println(F("Web server routes configured and server started."));
    } else {
        M5Dial.Display.drawString("Network Inactive", centerX, M5Dial.Display.height() / 2 + 35);
        Serial.println(F("Web server not started due to network unavailability."));
    }

    Serial.println(F("Displaying IP info for a few seconds..."));
    delay(2000); // Iets kortere delay

    initMenu(); // Initialiseer het menu

    // Bereid display voor main loop (kan overschreven worden door menu of andere schermen)
    M5Dial.Display.fillScreen(TFT_BLACK);
    M5Dial.Display.setTextDatum(TL_DATUM);
    M5Dial.Display.setTextSize(1);
    Serial.println(F("Setup complete. Entering main loop."));
}

// ---- MAIN LOOP: Runs repeatedly ----
void loop() {
    M5.update();          // Essentieel voor knoppen en encoder updates
    server.handleClient(); // Handel web server requests af

    if (menuActive) {
        handleMenuInput(); // Verwerk input voor het menu
        // Teken het menu op de globale canvas
        // De straal R/2 is een gok, pas aan voor de gewenste grootte van het cirkelmenu
        drawAppMenu(canvas, centerX, centerY, R / 2, 32);
        canvas.pushSprite(0, 0); // Push de getekende canvas naar het display
    }else if (savedLocationsMenuActive) { // ADDED: Handle saved locations menu
        handleSavedLocationsInput();
        drawSavedLocationsMenu(canvas, centerX, centerY);
        canvas.pushSprite(0, 0); 
    } else {
        // --- Jouw bestaande applicatielogica (als het menu niet actief is) ---
        // Dit is de "Navigate" modus of een andere modus geselecteerd vanuit het menu
        processGpsData();

        double currentHeadingDegrees = getSmoothedHeadingDegrees();
        double currentHeadingRadians = currentHeadingDegrees * M_PI / 180.0;

        double targetBearingDegrees = 0.0;
        double arrowAngleOnCompassDegrees = 0.0;

        bool locationIsValid = gps.location.isValid() && gps.location.age() < 3000;
        bool targetIsSet = (TARGET_LAT != 0.0 || TARGET_LON != 0.0);

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

        // Manier om terug te keren naar het menu
        // Bijvoorbeeld: lang indrukken van de knop
        if (M5.BtnA.wasHold()) { // Of M5.BtnA.pressedFor(long_press_duration)
            Serial.println("Returning to menu...");
            menuActive = true;
            initMenu(); // Reset menu state (optioneel, of zet alleen selectedMenuItemIndex)
            M5Dial.Display.fillScreen(TFT_BLACK); // Wis scherm voordat menu getekend wordt
        }
    }

}