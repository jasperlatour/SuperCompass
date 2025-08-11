#include "page/bluetoothinfo.h"
#include "bluetooth.h"
#include <BLEDevice.h>

extern bool bluetoothInfoActive;
extern bool menuActive;
extern bool btConnected;
extern uint32_t lastBtConnectedTime;

// Define the Bluetooth constants locally for UI display
#define BluetoothName "SuperCompass"
#define SERVICE_UUID_SHORT "e393c3ca" // First part of the full UUID for display

void showBluetoothInfoPage() {
    // Create a fresh canvas to avoid interference with other drawing
        // Prepare canvas (don't push yet; main loop will handle popup + push)
        canvas.fillSprite(TFT_BLACK);
    
    // Set text properties
    canvas.setTextDatum(MC_DATUM); // Middle-Center alignment
    canvas.setTextSize(1.5);  // Reduced text size from 2 to 1.5
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Title
    canvas.drawString("Bluetooth Info", canvas.width()/2, 30);
    
    int yPos = 70;
    int lineHeight = 25;  // Reduced line height from 30 to 25
    
    // Connection Status - using the global variable instead of checking advertising
    // This avoids calling BLEDevice::getAdvertising() which might be causing issues
    
    if (btConnected) {
        canvas.setTextColor(TFT_GREEN);
        canvas.drawString("Connected", canvas.width()/2, yPos);
    } else {
        canvas.setTextColor(TFT_YELLOW);
        canvas.drawString("Ready for Connection", canvas.width()/2, yPos);
    }
    yPos += lineHeight;
    
    // Device information
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(String("Name: ") + String(BluetoothName), canvas.width()/2, yPos);
    yPos += lineHeight;
    
    // Service UUID info
    canvas.drawString("Service Available", canvas.width()/2, yPos);
    yPos += lineHeight;
    
    // Connection duration if connected
    if (btConnected && lastBtConnectedTime > 0) {
        uint32_t connectedDuration = (millis() - lastBtConnectedTime) / 1000; // in seconds
        String durationStr = "Connected: ";
        
        if (connectedDuration < 60) {
            // Less than a minute
            durationStr += String(connectedDuration) + " sec";
        } else if (connectedDuration < 3600) {
            // Less than an hour
            durationStr += String(connectedDuration / 60) + "m " + String(connectedDuration % 60) + "s";
        } else {
            // Hours and minutes
            durationStr += String(connectedDuration / 3600) + "h " + String((connectedDuration % 3600) / 60) + "m";
        }
        canvas.drawString(durationStr, canvas.width()/2, yPos);
        yPos += lineHeight;
    }

    
    // Add a visual indicator instead of a touchable button
    // Draw disconnect button only if connected
    if (btConnected) {
        int indicatorY = yPos + 35; // Adjusted position for disconnect button
        int btnW = 140; int btnH = 40; int btnX = (canvas.width()-btnW)/2; int btnY = indicatorY - btnH/2;
        canvas.fillRoundRect(btnX, btnY, btnW, btnH, 8, TFT_RED);
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString("Disconnect", canvas.width()/2, indicatorY);
        // store bounds (unused but kept for potential future logic)
        static int lastBtnX=0,lastBtnY=0,lastBtnW=0,lastBtnH=0; lastBtnX=btnX; lastBtnY=btnY; lastBtnW=btnW; lastBtnH=btnH;
    }
    
        // Don't push here; main loop will call drawPopupIfActive + pushSprite
}

void handleBluetoothInfoInput() {
    // Don't call M5.update() here as it's already called at the beginning of the main loop
    
    // Short press - return to menu
    if (M5.BtnA.wasPressed()) {
        bluetoothInfoActive = false;
        menuActive = true; // Go back to the menu
        Serial.println("Returning to menu from Bluetooth info");
    }
    
    // Touch button detection (disconnect only if connected)
    if(touchEnabled){
        static int btnX,btnY,btnW,btnH; // will be set by drawing code via static storage; safe fallback
        auto t = M5Dial.Touch.getDetail();
        if(t.wasClicked()){
            if(btConnected){
                // Recompute button bounds (match drawing)
                int w = M5Dial.Display.width();
                int indicatorY = 70 + 25*4 + 70; // yPos logic mirrored
                int calcBtnW=140; int calcBtnH=40; int calcBtnX=(w-calcBtnW)/2; int calcBtnY=indicatorY - calcBtnH/2; 
                if(t.x>=calcBtnX && t.x<calcBtnX+calcBtnW && t.y>=calcBtnY && t.y<calcBtnY+calcBtnH){
                    disconnectBluetooth();
                    showPopupNotification("Disconnecting",1500,TFT_WHITE,TFT_RED);
                }
            }
            if(soundEnabled) M5Dial.Speaker.tone(650,25);
        }
    }
    // Removed long-press actions per new UI spec
    
    // Check for encoder activity - can be used for additional interaction
    int encoderValue = M5Dial.Encoder.read();
    if (encoderValue != 0) {
        M5Dial.Encoder.write(0); // Reset encoder value after reading
        // Could add additional functionality here in the future
    }
}

void resetBluetooth() {
    // Simple visual feedback - avoid complex operations during reset
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextSize(1.5);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Resetting Bluetooth...", canvas.width()/2, canvas.height()/2);
    canvas.pushSprite(0, 0);
    
    Serial.println("Bluetooth reset requested");
    
    // Call setupBLE() directly without trying to deinit
    // The setupBLE() function should handle initialization properly
    setupBLE();
    
    delay(300); // Small delay for stability
    
    // Go back to info page
        // Caller (reset flow) will handle popup compose+push
}
