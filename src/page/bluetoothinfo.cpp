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
        canvas.drawString("Status: Connected", canvas.width()/2, yPos);
    } else {
        canvas.setTextColor(TFT_YELLOW);
        canvas.drawString("Status: Ready for Connection", canvas.width()/2, yPos);
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
    
    // Add instruction text for controls
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Press button: Return to menu", canvas.width()/2, yPos + 15);
    canvas.drawString("Hold button 3s: Reset Bluetooth", canvas.width()/2, yPos + 35);
    
    // Add a visual indicator instead of a touchable button
    int indicatorY = yPos + 60;
    canvas.drawRoundRect((canvas.width() - 120) / 2, indicatorY - 15, 120, 30, 5, TFT_BLUE);
    canvas.setTextColor(TFT_CYAN);
    canvas.drawString("BLE " + String(SERVICE_UUID_SHORT), canvas.width()/2, indicatorY);
    
    canvas.pushSprite(0, 0);
}

void handleBluetoothInfoInput() {
    M5.update(); // Update button state before checking
    
    // Short press - return to menu
    if (M5.BtnA.wasPressed()) {
        bluetoothInfoActive = false;
        menuActive = true; // Go back to the menu
        Serial.println("Returning to menu from Bluetooth info");
    }
    
    // Long press (3 seconds) - reset Bluetooth
    if (M5.BtnA.pressedFor(3000)) {
        resetBluetooth();
    }
    
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
    
    delay(500); // Small delay for stability
    
    // Briefly show success message
    canvas.fillSprite(TFT_BLACK);
    canvas.drawString("Reset Complete", canvas.width()/2, canvas.height()/2);
    canvas.pushSprite(0, 0);
    delay(700);
    
    // Go back to info page
    showBluetoothInfoPage();
}
