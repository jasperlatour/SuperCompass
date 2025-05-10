#include "saved_locations.h"

#include "menu.h"

// ---  Saved Locations Data ---
const int MAX_SAVED_LOCATIONS = 5; // Example: Max 5 saved locations
SavedLocation savedLocations[MAX_SAVED_LOCATIONS] = {
    {"Eindhoven", 51.4392648, 5.478633},      
    {"Work", 51.4790956, 5.6557686},      
    {"Parijs", 48.8534951, 2.3483915}       
};


void initSavedLocationsMenu() {
    selectedLocationIndex = 0;
    encoder_click_accumulator = 0; // Reset accumulator for this menu
    Serial.println("Saved Locations Menu Initialized");
}


void drawSavedLocationsMenu(M5Canvas &canvas, int centerX, int centerY) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextSize(2);
    canvas.drawString("Saved Locations", centerX, 20);

    canvas.setTextSize(1);
    canvas.setTextDatum(ML_DATUM); // Middle Left for list items
    int startY = 50;
    int lineHeight = 20;

    for (int i = 0; i < numActualSavedLocations; ++i) {
        if (i == selectedLocationIndex) {
            canvas.setTextColor(TFT_YELLOW); // Highlight selected item
            canvas.drawString("> " + String(savedLocations[i].name), 20, startY + i * lineHeight);
        } else {
            canvas.setTextColor(TFT_WHITE);
            canvas.drawString(String(savedLocations[i].name), 30, startY + i * lineHeight);
        }
    }
    // Add a "Back" option drawn at the bottom if desired, or rely on long press
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextColor(TFT_CYAN);
    canvas.drawString("Press to Select", centerX, canvas.height() - 30);
    canvas.drawString("Hold Btn for Main Menu", centerX, canvas.height() - 15);

}


void handleSavedLocationsInput() {
    int raw_encoder_change = M5Dial.Encoder.read();
    if (raw_encoder_change != 0) {
        M5Dial.Encoder.write(0);
    }
    encoder_click_accumulator += raw_encoder_change;

    if (ENCODER_COUNTS_PER_DETENT > 0) {
        if (encoder_click_accumulator >= ENCODER_COUNTS_PER_DETENT) {
            int num_detents = encoder_click_accumulator / ENCODER_COUNTS_PER_DETENT;
            for (int i = 0; i < num_detents; ++i) {
                selectedLocationIndex++;
                if (selectedLocationIndex >= numActualSavedLocations) {
                    selectedLocationIndex = 0;
                }
            }
            encoder_click_accumulator %= ENCODER_COUNTS_PER_DETENT;
        } else if (encoder_click_accumulator <= -ENCODER_COUNTS_PER_DETENT) {
            int num_detents = -encoder_click_accumulator / ENCODER_COUNTS_PER_DETENT;
            for (int i = 0; i < num_detents; ++i) {
                selectedLocationIndex--;
                if (selectedLocationIndex < 0) {
                    selectedLocationIndex = numActualSavedLocations - 1;
                }
            }
            encoder_click_accumulator = -(-encoder_click_accumulator % ENCODER_COUNTS_PER_DETENT);
        }
    }

    if (M5.BtnA.wasPressed()) {
        TARGET_LAT = savedLocations[selectedLocationIndex].lat;
        TARGET_LON = savedLocations[selectedLocationIndex].lon;
        Setaddress = savedLocations[selectedLocationIndex].name;
        targetIsSet = true; // Set the flag
        
        Serial.print("Target set from saved: "); Serial.println(Setaddress);
        
        savedLocationsMenuActive = false; // Exit saved locations menu
        menuActive = false;             // Ensure main menu is also off, go to navigation
        // Screen will be redrawn by main loop's navigation logic
    }

    // Optional: Handle long press to go back to main menu
    if (M5.BtnA.wasHold()) {
        Serial.println("Returning to main menu from saved locations...");
        savedLocationsMenuActive = false;
        menuActive = true;
        initMenu(); // Re-initialize main menu
    }
}
