#include "page/settings.h"
#include "menu.h"
#include "drawing.h" // For drawPopupIfActive
#include "page/calibration.h"
#include <esp_sleep.h>
#include <driver/gpio.h>

static const char* sleepWakeCauseToString(esp_sleep_wakeup_cause_t cause) {
    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
        case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCH";
        case ESP_SLEEP_WAKEUP_ULP: return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
        case ESP_SLEEP_WAKEUP_UART: return "UART";
        case ESP_SLEEP_WAKEUP_WIFI: return "WIFI";
        case ESP_SLEEP_WAKEUP_BT: return "BT";
        case ESP_SLEEP_WAKEUP_UNDEFINED: return "UNDEFINED";
        default: return "UNKNOWN";
    }
}

// Local state
static int settingsSelectedIndex = 0;
static const int SETTINGS_ITEMS = 7; // Sound, Sound Level, Touch, Brightness, Calibrate Compass, Power Off, Back
static int encoderAccum = 0; // for slower scroll
static bool adjustingValue = false; // Track if we're adjusting a value
static float settingsScrollAnim = 0.0f; // smoothed version of settingsSelectedIndex, for the scroll animation

// Persistence file
static const char* SETTINGS_FILE = "/settings.json";

static void saveSettings(){
    File f = SPIFFS.open(SETTINGS_FILE, "w");
    if(!f){ Serial.println("Failed to open settings file for write"); return; }
    StaticJsonDocument<256> doc;
    doc["sound"] = soundEnabled;
    doc["touch"] = touchEnabled;
    doc["brightness"] = screenBrightness;
    doc["soundlevel"] = soundLevel;
    doc["offset_x"] = offset_x;
    doc["offset_y"] = offset_y;
    doc["scale_x"] = scale_x;
    doc["scale_y"] = scale_y;
    serializeJson(doc, f);
    f.close();
    Serial.println("Settings saved");
}

void saveCalibrationValues(int offsetX, int offsetY, float scaleX, float scaleY) {
    offset_x = offsetX;
    offset_y = offsetY;
    scale_x = scaleX;
    scale_y = scaleY;
    saveSettings();
}

void loadSettings(){
    if(!SPIFFS.exists(SETTINGS_FILE)){
        Serial.println("Settings file not found, using defaults");
        M5Dial.Display.setBrightness(screenBrightness);
        M5Dial.Speaker.setVolume(soundLevel);
        return;
    }

    File f = SPIFFS.open(SETTINGS_FILE, "r");
    if(!f){
        Serial.println("Failed to open settings file for read");
        M5Dial.Display.setBrightness(screenBrightness);
        M5Dial.Speaker.setVolume(soundLevel);
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError e = deserializeJson(doc, f);
    if(e){
        Serial.println("Failed to parse settings file");
        f.close();
        M5Dial.Display.setBrightness(screenBrightness);
        M5Dial.Speaker.setVolume(soundLevel);
        return;
    }

    if(doc["sound"].is<bool>()) soundEnabled = doc["sound"].as<bool>();
    if(doc["touch"].is<bool>()) touchEnabled = doc["touch"].as<bool>();
    if(doc["brightness"].is<int>()) screenBrightness = doc["brightness"].as<int>();
    if(doc["soundlevel"].is<int>()) soundLevel = doc["soundlevel"].as<int>();
    if(doc["offset_x"].is<int>()) offset_x = doc["offset_x"].as<int>();
    if(doc["offset_y"].is<int>()) offset_y = doc["offset_y"].as<int>();
    if(doc["scale_x"].is<float>()) scale_x = doc["scale_x"].as<float>();
    if(doc["scale_y"].is<float>()) scale_y = doc["scale_y"].as<float>();

    f.close();

    M5Dial.Display.setBrightness(screenBrightness);
    M5Dial.Speaker.setVolume(soundLevel);
    Serial.println("Settings loaded");
}

void initSettingsMenu(){
    settingsSelectedIndex = 0;
    adjustingValue = false;
    settingsScrollAnim = 0.0f;
    loadSettings();
}

// Draws one row of the carousel. isSelected rows get the full-size highlighted
// treatment; everything else is a small, dim neighbor peeking in from above/below.
static void drawSettingRow(M5Canvas &canvas, int centerX, int y, const char* label, const char* value, bool isSelected, bool isAdjusting) {
    canvas.setTextDatum(MC_DATUM);
    String line = (value[0] != '\0') ? (String(label) + ": " + value) : String(label);

    if (isSelected) {
        uint16_t bg = isAdjusting ? THEME_ACCENT_TARGET : THEME_PANEL;
        uint16_t fg = isAdjusting ? THEME_BG : THEME_ACCENT_PRIMARY;

        // Drop to a smaller font if the line is too wide for the round panel
        // at size 2, rather than letting text spill past the highlight pill.
        canvas.setTextSize(2);
        int fontSize = 2;
        int textW = canvas.textWidth(line.c_str());
        if (textW > 176) {
            canvas.setTextSize(1);
            fontSize = 1;
            textW = canvas.textWidth(line.c_str());
        }

        int barWidth = min(textW + 28, 200);
        int barHeight = (fontSize == 2) ? 34 : 24;
        canvas.fillSmoothRoundRect(centerX - barWidth / 2, y - barHeight / 2, barWidth, barHeight, 10, bg);
        canvas.setTextColor(fg, bg);
        canvas.drawString(line, centerX, y);
    } else {
        canvas.setTextSize(1);
        canvas.setTextColor(THEME_TEXT_DIM, THEME_BG);
        canvas.drawString(line, centerX, y);
    }
}

void drawSettingsMenu(M5Canvas &canvas,int centerX,int centerY){
    canvas.fillSprite(THEME_BG);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextColor(THEME_ACCENT_PRIMARY);
    canvas.setTextSize(2);
    canvas.drawString("Settings",canvas.width()/2,30);

    // Format the brightness and sound level as percentages
    char brightnessStr[10];
    char soundLevelStr[10];
    sprintf(brightnessStr, "%d%%", (screenBrightness * 100) / 255);
    sprintf(soundLevelStr, "%d%%", (soundLevel * 100) / 255);

    const char* labels[SETTINGS_ITEMS] = {"Sound", "Sound Level", "Touch", "Brightness", "Calibrate", "Power Off", "Back"};
    const char* values[SETTINGS_ITEMS] = {
        soundEnabled ? "On" : "Off",
        soundLevelStr,
        touchEnabled ? "On" : "Off",
        brightnessStr,
        "",
        "",
        ""
    };

    // Ease the scroll position toward the selected index - a carousel that
    // slides between rows instead of the list just snapping into place.
    settingsScrollAnim += (settingsSelectedIndex - settingsScrollAnim) * 0.3f;
    if (fabs(settingsSelectedIndex - settingsScrollAnim) < 0.02f) settingsScrollAnim = (float)settingsSelectedIndex;

    // Only render items within +/-1.5 rows of center - keeps every row inside
    // the round panel's wide middle band instead of overflowing top/bottom.
    const int rowSpacing = 42;
    for (int i = 0; i < SETTINGS_ITEMS; ++i) {
        float relative = i - settingsScrollAnim;
        if (fabs(relative) > 1.5f) continue;
        int y = centerY + (int)roundf(relative * rowSpacing);
        bool isSelected = (i == settingsSelectedIndex);
        drawSettingRow(canvas, centerX, y, labels[i], values[i], isSelected, adjustingValue && isSelected);
    }

    canvas.setTextSize(1);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextColor(THEME_TEXT_DIM);

    // Two short lines instead of one long one - a single-line hint this wide
    // doesn't fit inside the round panel's bottom band.
    if (adjustingValue) {
        canvas.drawString("Turn: adjust", centerX, canvas.height() - 34);
        canvas.drawString("Press: confirm", centerX, canvas.height() - 20);
    } else {
        canvas.drawString("Turn: move", centerX, canvas.height() - 34);
        canvas.drawString("Press: select", centerX, canvas.height() - 20);
    }
}

void handleSettingsInput(){
    int delta = M5Dial.Encoder.read();
    if(delta!=0){
        M5Dial.Encoder.write(0);
        encoderAccum += delta;
        const int STEP = 4; // require four raw ticks per move (slower)
        const int ADJUST_STEP = 4; // step size for value adjustments
        
        if(adjustingValue) {
            // We're in adjustment mode - adjust the selected setting
            switch(settingsSelectedIndex) {
                case 0: // Sound On/Off - toggle with encoder motion
                    if(encoderAccum >= STEP || encoderAccum <= -STEP) {
                        encoderAccum = 0;
                        soundEnabled = !soundEnabled;
                        if(soundEnabled) { 
                            M5Dial.Speaker.tone(1000, 40);
                        } else { 
                            M5Dial.Speaker.tone(400, 40);
                        }
                        saveSettings();
                    }
                    break;
                
                case 1: // Sound Level
                    if(encoderAccum >= ADJUST_STEP) {
                        encoderAccum -= ADJUST_STEP;
                        soundLevel = min(255, soundLevel + 10); // Increment by ~4%
                        M5Dial.Speaker.setVolume(soundLevel);
                        if(soundEnabled) {
                            // Play a sound to demonstrate the current volume level
                            M5Dial.Speaker.tone(800, 50);
                        }
                        saveSettings();
                    } else if(encoderAccum <= -ADJUST_STEP) {
                        encoderAccum += ADJUST_STEP;
                        soundLevel = max(10, soundLevel - 10); // Decrement by ~4%, keep minimum audible level
                        M5Dial.Speaker.setVolume(soundLevel);
                        if(soundEnabled) {
                            // Play a sound to demonstrate the current volume level
                            M5Dial.Speaker.tone(800, 50);
                        }
                        saveSettings();
                    }
                    break;
                    
                case 2: // Touch On/Off - toggle with encoder motion
                    if(encoderAccum >= STEP || encoderAccum <= -STEP) {
                        encoderAccum = 0;
                        touchEnabled = !touchEnabled;
                        if(soundEnabled) M5Dial.Speaker.tone(800, 30);
                        saveSettings();
                    }
                    break;
                    
                case 3: // Brightness
                    if(encoderAccum >= ADJUST_STEP) {
                        encoderAccum -= ADJUST_STEP;
                        screenBrightness = min(255, screenBrightness + 10); // Increment by ~4%
                        M5Dial.Display.setBrightness(screenBrightness);
                        if(soundEnabled) M5Dial.Speaker.tone(600, 15);
                        saveSettings();
                    } else if(encoderAccum <= -ADJUST_STEP) {
                        encoderAccum += ADJUST_STEP;
                        screenBrightness = max(10, screenBrightness - 10); // Decrement by ~4%, keep minimum visible
                        M5Dial.Display.setBrightness(screenBrightness);
                        if(soundEnabled) M5Dial.Speaker.tone(600, 15);
                        saveSettings();
                    }
                    break;
                    
            }
        } else {
            // Normal navigation mode
            while(encoderAccum >= STEP){ 
                encoderAccum -= STEP; 
                settingsSelectedIndex++; 
                if(soundEnabled) M5Dial.Speaker.tone(600, 15); 
            }
            while(encoderAccum <= -STEP){ 
                encoderAccum += STEP; 
                settingsSelectedIndex--; 
                if(soundEnabled) M5Dial.Speaker.tone(600, 15); 
            }
            if(settingsSelectedIndex < 0) settingsSelectedIndex = SETTINGS_ITEMS - 1;
            if(settingsSelectedIndex >= SETTINGS_ITEMS) settingsSelectedIndex = 0;
        }
    }

    if(M5.BtnA.wasPressed()){ // Button press action
        if(adjustingValue) {
            // Exit adjustment mode
            adjustingValue = false;
            if(soundEnabled) M5Dial.Speaker.tone(900, 30);
        } else {
            // Handle selecting an option
            switch(settingsSelectedIndex){
                case 0: // Sound - enter adjustment mode
                    adjustingValue = true;
                    if(soundEnabled) M5Dial.Speaker.tone(800, 30);
                    break;
                    
                case 1: // Sound Level - enter adjustment mode
                    adjustingValue = true;
                    if(soundEnabled) {
                        // Play a sound to demonstrate the current volume level
                        M5Dial.Speaker.tone(800, 100);
                    }
                    break;
                    
                case 2: // Touch - enter adjustment mode
                    adjustingValue = true;
                    if(soundEnabled) M5Dial.Speaker.tone(800, 30);
                    break;
                    
                case 3: // Brightness - enter adjustment mode
                    adjustingValue = true;
                    if(soundEnabled) M5Dial.Speaker.tone(800, 30);
                    break;
                    
                case 4: // Calibrate Compass - launch the calibration routine
                    if(soundEnabled) M5Dial.Speaker.tone(800, 30);
                    settingsMenuActive = false;
                    compassCalibrationActive = true;
                    initCompassCalibration();
                    break;

                case 5: { // Sleep - immediate action (wake on BtnA)
                    if(soundEnabled) M5Dial.Speaker.tone(200, 200);
                    
                    // Show a popup message
                    showPopupNotification("Entering standby, press button to wake", 2000, TFT_WHITE, TFT_NAVY);

                    // Draw and display the popup before sleeping
                    drawSettingsMenu(canvas, centerX, centerY);
                    drawPopupIfActive(canvas);
                    canvas.pushSprite(0, 0);
                    delay(2000); 
                    
                    // Wait for button to be released to prevent immediate wake-up
                    while(M5.BtnA.isPressed()) {
                        M5.update();
                        delay(10);
                    }
                    delay(100); // Extra delay to ensure button is fully released

                    Serial.println("Entering standby mode (display off, wake on BtnA)...");

                    // On M5Dial, hardware wake from true sleep via the center button is
                    // unreliable on battery power. Use a software standby instead:
                    // display off, LED off, and idle loop until BtnA is pressed.
                    M5Dial.Display.setBrightness(0);
                    M5Dial.Display.sleep();
                    M5Dial.Display.waitDisplay();
                    M5Dial.Power.setLed(0);
                    Serial.flush();

                    delay(50);
                    while (true) {
                        M5.update();
                        if (M5.BtnA.wasPressed() || M5.BtnA.isPressed()) {
                            break;
                        }
                        delay(20);
                    }

                    // Restore UI after wake-up.
                    M5Dial.Display.wakeup();
                    M5Dial.Display.setBrightness(screenBrightness);
                    M5Dial.Power.setLed(255);

                    showPopupNotification("Woke from standby", 2000, TFT_WHITE, TFT_DARKGREEN);

                    // Wait release to avoid immediate re-trigger/actions.
                    delay(50);
                    M5.update();
                    while(M5.BtnA.isPressed()) {
                        M5.update();
                        delay(10);
                    }

                    // Clear transient UI/input state and return to compass screen.
                    popupActive = false;
                    adjustingValue = false;
                    encoderAccum = 0;
                    settingsMenuActive = false;
                    menuActive = false;
                    savedLocationsMenuActive = false;
                    gpsinfoActive = false;
                    return;
                    break;
                }
                    
                case 6: // Back - immediate action
                    settingsMenuActive = false;
                    menuActive = true;
                    initMenu();
                    break;
            }
        }
    }

    // Optional long press to exit to menu from anywhere in settings
    if(M5.BtnA.pressedFor(2000)){
        adjustingValue = false; // Reset adjustment state
        settingsMenuActive = false;
        menuActive = true;
        initMenu();
    }
}
