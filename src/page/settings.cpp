#include "page/settings.h"
#include "menu.h"

// Local state
static int settingsSelectedIndex = 0;
static const int SETTINGS_ITEMS = 6; // Sound, Sound Level, Touch, Brightness, Power Off, Back
static int encoderAccum = 0; // for slower scroll
static bool adjustingValue = false; // Track if we're adjusting a value

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
    serializeJson(doc, f);
    f.close();
    Serial.println("Settings saved");
}

void loadSettings(){
    if(!SPIFFS.exists(SETTINGS_FILE)){ Serial.println("Settings file not found, using defaults"); return; }
    File f = SPIFFS.open(SETTINGS_FILE, "r");
    if(!f){ Serial.println("Failed to open settings file for read"); return; }
    StaticJsonDocument<256> doc;
    DeserializationError e = deserializeJson(doc, f);
    if(e){ Serial.println("Failed to parse settings file"); f.close(); return; }
    if(doc.containsKey("sound")) soundEnabled = doc["sound"].as<bool>();
    if(doc.containsKey("touch")) touchEnabled = doc["touch"].as<bool>();
    if(doc.containsKey("brightness")) {
        screenBrightness = doc["brightness"].as<int>();
        M5Dial.Display.setBrightness(screenBrightness); // Apply brightness setting
    }
    if(doc.containsKey("soundlevel")) {
        soundLevel = doc["soundlevel"].as<int>();
        M5Dial.Speaker.setVolume(soundLevel); // Apply sound level setting
    }
    f.close();
    Serial.println("Settings loaded");
}

void initSettingsMenu(){
    settingsSelectedIndex = 0;
    adjustingValue = false;
    loadSettings();
}

static void drawSettingLine(M5Canvas &canvas,int y,const char* label,const char* value,bool sel, bool adjusting = false){
    canvas.setTextDatum(MC_DATUM);
    if(sel){
        if(adjusting) {
            // Different style when in adjustment mode
            canvas.setTextColor(TFT_BLACK,TFT_YELLOW);
            canvas.fillRect(0,y-18,canvas.width(),36,TFT_YELLOW);
        } else {
            canvas.setTextColor(TFT_YELLOW,TFT_DARKGREY);
            canvas.fillRect(0,y-18,canvas.width(),36,TFT_DARKGREY);
        }
    } else {
        canvas.setTextColor(TFT_WHITE,TFT_BLACK);
    }
    String line = String(label) + ": " + value;
    canvas.drawString(line,canvas.width()/2,y);
}

void drawSettingsMenu(M5Canvas &canvas,int centerX,int centerY){
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextColor(TFT_CYAN);
    canvas.setTextSize(2);
    canvas.drawString("Settings",canvas.width()/2,40);
    canvas.setTextSize(1);

    // Format the brightness and sound level as percentages
    char brightnessStr[10];
    char soundLevelStr[10];
    sprintf(brightnessStr, "%d%%", (screenBrightness * 100) / 255);
    sprintf(soundLevelStr, "%d%%", (soundLevel * 100) / 255);

    drawSettingLine(canvas, centerY-80, "Sound", soundEnabled?"On":"Off", settingsSelectedIndex==0, adjustingValue && settingsSelectedIndex==0);
    drawSettingLine(canvas, centerY-40, "Sound Level", soundLevelStr, settingsSelectedIndex==1, adjustingValue && settingsSelectedIndex==1);
    drawSettingLine(canvas, centerY, "Touch", touchEnabled?"On":"Off", settingsSelectedIndex==2, adjustingValue && settingsSelectedIndex==2);
    drawSettingLine(canvas, centerY+40, "Brightness", brightnessStr, settingsSelectedIndex==3, adjustingValue && settingsSelectedIndex==3);
    drawSettingLine(canvas, centerY+80, "Power Off", "--", settingsSelectedIndex==4, adjustingValue && settingsSelectedIndex==4);
    drawSettingLine(canvas, centerY+120, "Back", "", settingsSelectedIndex==5, adjustingValue && settingsSelectedIndex==5);

    canvas.setTextDatum(BC_DATUM);
    canvas.setTextColor(TFT_LIGHTGREY);
    
    // Update the help text based on the current state
    if (adjustingValue) {
        canvas.drawString("Rotate: adjust value  Press: confirm", canvas.width()/2, canvas.height()-4);
    } else {
        canvas.drawString("Rotate: navigate  Press: select", canvas.width()/2, canvas.height()-4);
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
                    
                // No adjustment for Power Off or Back options
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
                    
                case 4: // Power Off - immediate action (using deep sleep for wake capability)
                    if(soundEnabled) M5Dial.Speaker.tone(200, 200);
                    
                    // Show a popup message
                    popupActive = true;
                    popupMessage = "Going to sleep, press button to wake";
                    popupTextColor = TFT_WHITE;
                    popupBgColor = TFT_NAVY;
                    popupEndTime = millis() + 2000; // Display for 2 seconds
                    
                    // Process the popup message before sleeping
                    delay(2000); 
                    
                    // Use deepSleep instead of powerOff
                    // This allows the button to wake the device
                    M5Dial.Power.deepSleep(0, true); // No time limit, enable button wakeup
                    break;
                    
                case 5: // Back - immediate action
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
