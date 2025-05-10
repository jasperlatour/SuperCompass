#include "saved_locations.h"

#include "menu.h"

// ---  Saved Locations Data ---
const int MAX_SAVED_LOCATIONS = 5; // Example: Max 5 saved locations
int numActualSavedLocations = 5; 
SavedLocation savedLocations[MAX_SAVED_LOCATIONS] = {
    {"Eindhoven", 51.4392648, 5.478633},
    {"Helmond", 51.4790956, 5.6557686},
    {"Parijs", 48.8534951, 2.3483915},
    {"Amsterdam", 52.3676, 4.9041}, // Example: Amsterdam
    {"Berlin", 52.5200, 13.4050} // Example: Berlin
};

void initSavedLocationsMenu() {
    selectedLocationIndex = 0;
    encoder_click_accumulator = 0; // Reset accumulator for this menu
    Serial.println("Saved Locations Menu Initialized");
}

void drawSavedLocationsMenu(M5Canvas &canvas, int centerX, int centerY) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextDatum(MC_DATUM); // Center datum for all text
    canvas.setTextColor(TFT_WHITE);

    // Draw Title
    canvas.setTextSize(2);
    canvas.drawString("Saved Locations", centerX, 50);

    // Define Y positions for the items
    int selectedY = centerY; // Center for the selected item
    int prevY = centerY - 50; // Position for the previous item
    int nextY = centerY + 50; // Position for the next item

    // Display Previous Item (with wrap around)
    if (numActualSavedLocations > 1) { // Only show if there's more than one item
        int prevIndex = (selectedLocationIndex - 1 + numActualSavedLocations) % numActualSavedLocations;
        canvas.setTextSize(1); // Smaller font size
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString(String(savedLocations[prevIndex].name), centerX, prevY);
    }

    // Display Selected Item
    if (numActualSavedLocations > 0) { // Ensure there's at least one item to display
        canvas.setTextSize(3); // Larger font size
        canvas.setTextColor(TFT_YELLOW); // Highlight selected item
        canvas.drawString(String(savedLocations[selectedLocationIndex].name), centerX, selectedY);
    }


    // Display Next Item (with wrap around)
    if (numActualSavedLocations > 1) { // Only show if there's more than one item
        int nextIndex = (selectedLocationIndex + 1) % numActualSavedLocations;
        canvas.setTextSize(1); // Smaller font size
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString(String(savedLocations[nextIndex].name), centerX, nextY);
    }

    // Footer instructions
    canvas.setTextSize(1); // Reset text size for footer
    canvas.setTextColor(TFT_CYAN);
    canvas.drawString("Press to Select", centerX, canvas.height() - 25);
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
}
