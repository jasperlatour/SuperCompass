#include "saved_locations.h"
#include "menu.h"
#include "bluetooth.h" // For publishTargetCharacteristic / publishReady

#define FileSystem SPIFFS
#define SAVED_LOCATIONS_FILE "/saved_locations.json"

std::vector<SavedLocation> savedLocations;


// Example initial locations if file doesn't exist (optional)
void addDefaultLocations() {
    if (savedLocations.empty()) { // Only add if the list is empty after trying to load
        savedLocations.push_back({"Eindhoven", 51.4392648, 5.478633});
        savedLocations.push_back({"Helmond", 51.4790956, 5.6557686});
        savedLocations.push_back({"Parijs", 48.8534951, 2.3483915});

        saveSavedLocations(); // Save them if added
    }
}

void loadSavedLocations() {
    // Example using SPIFFS and ArduinoJson
    // Ensure FileSystem.begin() has been called in setup()
    if (FileSystem.exists(SAVED_LOCATIONS_FILE)) {
        File file = FileSystem.open(SAVED_LOCATIONS_FILE, "r");
        if (file) {
            DynamicJsonDocument doc(2048); // Adjust size as needed
            DeserializationError error = deserializeJson(doc, file);
            if (error) {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.c_str());
            } else {
                JsonArray array = doc.as<JsonArray>();
                savedLocations.clear();

                for (JsonObject obj : array) {
                    const char* name_from_json = obj["name"];
                    double lat_from_json = obj["lat"]; //.as<double>(); // Be explicit if needed
                    double lon_from_json = obj["lon"]; //.as<double>();

                    if (name_from_json) { // Check if name exists in JSON
                        savedLocations.push_back({String(name_from_json), lat_from_json, lon_from_json});
                    } else {
                        Serial.println(F("Warning: Location in JSON missing name. Skipping."));
                    }
                }
            }
            file.close();
        } else {
            Serial.println(F("Failed to open saved_locations.json for reading"));
        }
    } else {
        Serial.println(F("saved_locations.json not found. Loading defaults."));
        addDefaultLocations(); // Load defaults if file doesn't exist
    }
    Serial.print("Loaded "); Serial.print(savedLocations.size()); Serial.println(" locations.");
}

void saveSavedLocations() {
    // Example using SPIFFS and ArduinoJson
    File file = FileSystem.open(SAVED_LOCATIONS_FILE, "w");
    if (file) {
        StaticJsonDocument<2048> doc; // Adjust size as needed
        JsonArray array = doc.to<JsonArray>();
        for (const auto& loc : savedLocations) {
            JsonObject obj = array.createNestedObject();
            obj["name"] = loc.name;
            obj["lat"] = loc.lat;
            obj["lon"] = loc.lon;
        }
        if (serializeJson(doc, file) == 0) {
            Serial.println(F("Failed to write to file"));
        }
        file.close();
        Serial.println(F("Saved locations to file."));
    } else {
        Serial.println(F("Failed to open saved_locations.json for writing"));
    }
}

void initSavedLocationsMenu() {
    selectedLocationIndex = 0;
    encoder_click_accumulator = 0; // Reset accumulator for this menu
    Serial.println("Saved Locations Menu Initialized");
}

void drawSavedLocationsMenu(M5Canvas &canvas, int centerX, int centerY) {
    canvas.fillSprite(THEME_BG);
    canvas.setTextDatum(MC_DATUM); // Center datum for all text
    canvas.setTextColor(THEME_ACCENT_PRIMARY);

    // Draw Title (short enough to fit the round panel's narrower band at this height)
    canvas.setTextSize(2);
    canvas.drawString("Locations", centerX, 40);

    // Define Y positions for the items
    int selectedY = centerY; // Center for the selected item
    int prevY = centerY - 50; // Position for the previous item
    int nextY = centerY + 50; // Position for the next item

    // Display Previous Item (with wrap around)
    if (savedLocations.size() > 1) { // Only show if there's more than one item
        int prevIndex = (selectedLocationIndex - 1 + savedLocations.size()) % savedLocations.size();
        canvas.setTextSize(1); // Smaller font size
        canvas.setTextColor(THEME_TEXT_DIM);
        canvas.drawString(savedLocations[prevIndex].name, centerX, prevY);
    }

    // Display Selected Item
    if (savedLocations.size() > 0) { // Ensure there's at least one item to display
        canvas.setTextSize(3); // Larger font size
        canvas.setTextColor(THEME_ACCENT_PRIMARY); // Highlight selected item
        canvas.drawString(savedLocations[selectedLocationIndex].name, centerX, selectedY);
    }


    // Display Next Item (with wrap around)
    if (savedLocations.size() > 1) { // Only show if there's more than one item
        int nextIndex = (selectedLocationIndex + 1) % savedLocations.size();
        canvas.setTextSize(1); // Smaller font size
        canvas.setTextColor(THEME_TEXT_DIM);
        canvas.drawString(savedLocations[nextIndex].name, centerX, nextY);
    }

    // Footer instructions
    canvas.setTextSize(1); // Reset text size for footer
    canvas.setTextColor(THEME_TEXT_DIM);
    canvas.drawString("Press to Select", centerX, canvas.height() - 25);
}


void handleSavedLocationsInput() {
    if (savedLocations.empty()) return; // No input if no locations

    // Assuming ENCODER_COUNTS_PER_DETENT and encoder_click_accumulator are available
    // Ensure encoder_click_accumulator is declared/managed appropriately (e.g., global or passed)
    extern int encoder_click_accumulator; // If it's a global from menu.cpp or main.cpp
    extern const int ENCODER_COUNTS_PER_DETENT; // If it's a global const

    int raw_encoder_change = M5Dial.Encoder.read();
    if (raw_encoder_change != 0) {
        M5Dial.Encoder.write(0);
    }
    encoder_click_accumulator += raw_encoder_change;

    if (ENCODER_COUNTS_PER_DETENT > 0 && !savedLocations.empty()) {
        if (encoder_click_accumulator >= ENCODER_COUNTS_PER_DETENT) {
            int num_detents = encoder_click_accumulator / ENCODER_COUNTS_PER_DETENT;
            for (int i = 0; i < num_detents; ++i) {
                selectedLocationIndex++;
                if (selectedLocationIndex >= savedLocations.size()) {
                    selectedLocationIndex = 0;
                }
            }
            encoder_click_accumulator %= ENCODER_COUNTS_PER_DETENT;
        } else if (encoder_click_accumulator <= -ENCODER_COUNTS_PER_DETENT) {
            int num_detents = -encoder_click_accumulator / ENCODER_COUNTS_PER_DETENT;
            for (int i = 0; i < num_detents; ++i) {
                selectedLocationIndex--;
                if (selectedLocationIndex < 0) {
                    selectedLocationIndex = savedLocations.size() - 1;
                }
            }
            encoder_click_accumulator = -(-encoder_click_accumulator % ENCODER_COUNTS_PER_DETENT);
        }
    }

    if (M5.BtnA.wasPressed()) {
        if (!savedLocations.empty()) {
            TARGET_LAT = savedLocations[selectedLocationIndex].lat;
            TARGET_LON = savedLocations[selectedLocationIndex].lon;
            Setaddress = savedLocations[selectedLocationIndex].name;
            targetIsSet = true; 
            
            Serial.print("Target set from saved: "); Serial.println(Setaddress);

            // Immediately publish updated target to BLE so the connected app sees change
            publishTargetCharacteristic();
            // Update ready characteristic (hasTarget field)
            publishReady(true);
            
            savedLocationsMenuActive = false; 
            menuActive = false;             
        }
    }
}
