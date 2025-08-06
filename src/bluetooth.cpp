#include "bluetooth.h"
#include "page/saved_locations.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#define BluetoothName "SuperCompass"

#define SERVICE_UUID           "e393c3ca-4e9f-4d5c-bba0-37e53272f8b3"
#define TARGET_CHAR_UUID       "78afdeb8-a315-4030-8337-629d4e021306"
#define LOCATIONS_LIST_CHAR_UUID "cdefa4dc-b73e-4865-b35f-fafa76914afb"
#define LOCATIONS_MODIFY_CHAR_UUID "c660ca7d-b7ea-4c13-84fe-74dd8a11814d"
#define CURRENT_POSITION_CHAR_UUID "b5439cfa-7d1b-4e82-8a81-f5d84a276dc2"

BLECharacteristic *pLocationsListCharacteristic;

// Global variables to track BLE connection state
bool btConnected = false;
uint32_t lastBtConnectedTime = 0;

// Flag to save locations after BLE action
bool needsLocationsSave = false;

// BLE position variables
bool blePositionSet = false;        // Whether we have a valid position from BLE
uint32_t blePositionTime = 0;       // Time when the BLE position was last updated
double BLE_LAT = 0.0;               // Latitude from BLE
double BLE_LON = 0.0;               // Longitude from BLE

class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("BLE Client Connected");
      btConnected = true;
      lastBtConnectedTime = millis();
    }

    void onDisconnect(BLEServer* pServer) {
      Serial.println("BLE Client Disconnected");
      btConnected = false;
      BLEDevice::startAdvertising(); // Restart advertising on disconnect
    }
};

void notifySavedLocationsChange() {
    try {
        Serial.println("BLE: Preparing to notify saved locations change");
        
        // Use a small static buffer to avoid memory issues
        static char buffer[200];
        strcpy(buffer, "[]"); // Default empty array
        
        Serial.print("BLE: Total saved locations: ");
        Serial.println(savedLocations.size());
        
        if (savedLocations.size() > 0) {
            // Create a small JsonDocument
            JsonDocument doc;
            JsonArray array = doc.to<JsonArray>();
            
            // Only send a few locations to avoid memory issues
            const size_t maxLocations = 3; // Send very few at a time
            Serial.print("BLE: Sending up to ");
            Serial.print(maxLocations);
            Serial.println(" locations");
            
            for (size_t i = 0; i < savedLocations.size() && i < maxLocations; i++) {
                JsonObject obj = array.add<JsonObject>();
                
                // Safely handle the name
                if (savedLocations[i].name != nullptr) {
                    obj["name"] = savedLocations[i].name;
                } else {
                    obj["name"] = "Unnamed";
                }
                
                obj["lat"] = savedLocations[i].lat;
                obj["lon"] = savedLocations[i].lon;
            }
            
            // Serialize to our buffer with size limit
            serializeJson(doc, buffer, sizeof(buffer));
        }
        
        // Log the JSON that's going to be sent
        Serial.print("BLE: Sending locations JSON: ");
        Serial.println(buffer);
        
        if (pLocationsListCharacteristic != nullptr) {
            // Use standard string API which is known to be safe
            pLocationsListCharacteristic->setValue(buffer);
            pLocationsListCharacteristic->notify();
            Serial.println("BLE: Locations notification sent successfully");
        } else {
            Serial.println("BLE: ERROR - Locations characteristic not initialized");
        }
    } catch (const std::exception& e) {
        Serial.print("Exception in notifySavedLocationsChange: ");
        Serial.println(e.what());
    } catch (...) {
        Serial.println("Unknown exception in notifySavedLocationsChange");
    }
}

class TargetCharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        Serial.print("BLE Target received data, length: ");
        Serial.println(value.length());
        
        if (value.length() > 0 && value.length() < 100) { // Limit size
            // Print raw value for debugging
            Serial.print("Raw BLE target data: ");
            Serial.println(value.c_str());
            
            try {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, value);
                
                if (error) {
                    Serial.print("JSON parsing error: ");
                    Serial.println(error.c_str());
                    return;
                }
                
                // Log JSON contents
                Serial.println("JSON content:");
                serializeJsonPretty(doc, Serial);
                Serial.println();
                
                // Handle both naming conventions: "lat"/"lon" and "latitude"/"longitude"
                double lat = 0;
                double lon = 0;
                bool validCoords = false;
                
                if (doc.containsKey("lat") && doc.containsKey("lon")) {
                    lat = doc["lat"].as<double>();
                    lon = doc["lon"].as<double>();
                    validCoords = true;
                } else if (doc.containsKey("latitude") && doc.containsKey("longitude")) {
                    lat = doc["latitude"].as<double>();
                    lon = doc["longitude"].as<double>();
                    validCoords = true;
                }
                
                if (validCoords) {
                    Serial.print("Extracted lat: ");
                    Serial.print(lat, 6); // Print with 6 decimal precision
                    Serial.print(", lon: ");
                    Serial.println(lon, 6);
                    
                    if (validCoords && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {        
                        // Set target coordinates and state
                        TARGET_LAT = lat;
                        TARGET_LON = lon;
                        targetIsSet = true;
                        
                        // If name is provided in JSON, use it for Setaddress
                        if (doc.containsKey("name")) {
                            Setaddress = doc["name"].as<String>();
                        } else {
                            Setaddress = "BLE Target";
                        }
                        
                        // Verify values were set correctly
                        Serial.print("Verification - TARGET_LAT: ");
                        Serial.print(TARGET_LAT, 6);
                        Serial.print(", TARGET_LON: ");
                        Serial.println(TARGET_LON, 6);
                        Serial.print("targetIsSet: ");
                        Serial.println(targetIsSet ? "true" : "false");
                        Serial.print("Setaddress: ");
                        Serial.println(Setaddress);
                    } else {
                        Serial.println("Invalid coordinates received (out of range)");
                    }
                } else {
                    Serial.println("Missing coordinate fields in JSON. Looking for 'lat'/'lon' or 'latitude'/'longitude'");
                }
            } catch (...) {
                Serial.println("Exception during target JSON parsing");
            }
        } else {
            Serial.println("BLE target data invalid length");
        }
    }
};

class LocationsModifyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        try {
            std::string value = pCharacteristic->getValue();
            Serial.print("BLE Locations Modify received data, length: ");
            Serial.println(value.length());
            
            if (value.length() == 0 || value.length() > 200) { // Limit size
                Serial.println("BLE Locations data invalid length");
                return;
            }
            
            // Print raw value for debugging
            Serial.print("Raw BLE locations data: ");
            Serial.println(value.c_str());
            
            // Keep things very simple
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, value);
            
            if (error) {
                Serial.print("JSON parsing error in locations: ");
                Serial.println(error.c_str());
                return;
            }
            
            // Log JSON contents
            Serial.println("Locations JSON content:");
            serializeJsonPretty(doc, Serial);
            Serial.println();
            
            const char* action = doc["action"].as<const char*>();
            if (!action) {
                Serial.println("BLE Locations: Missing 'action' field in JSON");
                return;
            }
            
            Serial.print("BLE Locations: Processing action type: ");
            Serial.println(action);
            
        if (strcmp(action, "add") == 0) {
            // Check for data or location field, supporting both formats
            JsonObject data = doc.containsKey("data") ? doc["data"] : doc["location"];
            
            if (data) {
                const char* name = data["name"];
                double lat = 0.0;
                double lon = 0.0;
                
                // Handle both lat/lon and latitude/longitude field names
                if (data.containsKey("lat") && data.containsKey("lon")) {
                    lat = data["lat"];
                    lon = data["lon"];
                } else if (data.containsKey("latitude") && data.containsKey("longitude")) {
                    lat = data["latitude"];
                    lon = data["longitude"];
                } else {
                    Serial.println("BLE Locations: Missing coordinate fields in location data");
                    return;
                }
                
                if (name && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
                    Serial.print("BLE Locations: Adding new location '");
                    Serial.print(name);
                    Serial.print("' at ");
                    Serial.print(lat, 6);
                    Serial.print(", ");
                    Serial.println(lon, 6);
                    
                    char* name_copy = new char[strlen(name) + 1];
                    strcpy(name_copy, name);
                    savedLocations.push_back({name_copy, lat, lon});
                    needsLocationsSave = true;
                } else {
                    Serial.println("BLE Locations: Invalid name or coordinates in add request");
                }
            } else {
                Serial.println("BLE Locations: Add action missing 'data' or 'location' object");
            }
        }
            else if (strcmp(action, "edit") == 0) {
                int index = doc["index"];
                Serial.print("BLE Locations: Edit request for index ");
                Serial.println(index);
                
                if (index >= 0 && index < savedLocations.size()) {
                    JsonObject data = doc["data"];
                    if (data && data.containsKey("name")) {
                        const char* oldName = savedLocations[index].name ? savedLocations[index].name : "Unnamed";
                        double oldLat = savedLocations[index].lat;
                        double oldLon = savedLocations[index].lon;
                        
                        if (savedLocations[index].name) {
                            delete[] savedLocations[index].name;
                        }
                        
                        const char* name = data["name"];
                        char* name_copy = new char[strlen(name) + 1];
                        strcpy(name_copy, name);
                        savedLocations[index].name = name_copy;
                        
                        if (data.containsKey("lat")) {
                            savedLocations[index].lat = data["lat"];
                        }
                        if (data.containsKey("lon")) {
                            savedLocations[index].lon = data["lon"];
                        }
                        
                        Serial.print("BLE Locations: Edited location from '");
                        Serial.print(oldName);
                        Serial.print("' (");
                        Serial.print(oldLat, 6);
                        Serial.print(", ");
                        Serial.print(oldLon, 6);
                        Serial.print(") to '");
                        Serial.print(name);
                        Serial.print("' (");
                        Serial.print(savedLocations[index].lat, 6);
                        Serial.print(", ");
                        Serial.print(savedLocations[index].lon, 6);
                        Serial.println(")");
                        
                        needsLocationsSave = true;
                    } else {
                        Serial.println("BLE Locations: Edit action missing required fields");
                    }
                } else {
                    Serial.println("BLE Locations: Edit request with invalid index");
                }
            }
            else if (strcmp(action, "delete") == 0) {
                int index = doc["index"];
                Serial.print("BLE Locations: Delete request for index ");
                Serial.println(index);
                
                if (index >= 0 && index < savedLocations.size()) {
                    // Log what's being deleted
                    const char* name = savedLocations[index].name ? savedLocations[index].name : "Unnamed";
                    Serial.print("BLE Locations: Deleting location '");
                    Serial.print(name);
                    Serial.print("' at (");
                    Serial.print(savedLocations[index].lat, 6);
                    Serial.print(", ");
                    Serial.print(savedLocations[index].lon, 6);
                    Serial.println(")");
                    
                    if (savedLocations[index].name) {
                        delete[] savedLocations[index].name;
                    }
                    savedLocations.erase(savedLocations.begin() + index);
                    needsLocationsSave = true;
                } else {
                    Serial.println("BLE Locations: Delete request with invalid index");
                }
            } else {
                Serial.print("BLE Locations: Unknown action type: ");
                Serial.println(action);
            }
        } catch (...) {
            Serial.println("Exception in LocationsModifyCallbacks");
        }
    }
};

class CurrentPositionCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        // Log start of position update with timestamp
        unsigned long currentTime = millis();
        Serial.print("=== BLE POSITION UPDATE === Time: ");
        Serial.print(currentTime);
        Serial.println(" ms");
        
        std::string value = pCharacteristic->getValue();
        Serial.print("BLE Current Position received data, length: ");
        Serial.println(value.length());
        
        if (value.length() > 0 && value.length() < 100) { // Limit size
            // Print raw value for debugging with clear markers
            Serial.println("--------- RAW BLE POSITION DATA BEGIN ---------");
            Serial.println(value.c_str());
            Serial.println("--------- RAW BLE POSITION DATA END ---------");
            
            try {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, value);
                
                if (error) {
                    Serial.print("ERROR: JSON parsing failed: ");
                    Serial.println(error.c_str());
                    return;
                }
                
                // Log JSON contents with formatting
                Serial.println("--------- PARSED JSON DATA BEGIN ---------");
                serializeJsonPretty(doc, Serial);
                Serial.println("\n--------- PARSED JSON DATA END ---------");
                
                // Handle both naming conventions: "lat"/"lon" and "latitude"/"longitude"
                double lat = 0;
                double lon = 0;
                bool validCoords = false;
                
                // Check for different field naming conventions
                if (doc.containsKey("lat") && doc.containsKey("lon")) {
                    lat = doc["lat"].as<double>();
                    lon = doc["lon"].as<double>();
                    validCoords = true;
                    Serial.println("Using lat/lon fields from JSON");
                } else if (doc.containsKey("latitude") && doc.containsKey("longitude")) {
                    lat = doc["latitude"].as<double>();
                    lon = doc["longitude"].as<double>();
                    validCoords = true;
                    Serial.println("Using latitude/longitude fields from JSON");
                }
                
                if (validCoords) {
                    Serial.print("BLE Position Coordinates: [");
                    Serial.print(lat, 6);
                    Serial.print(", ");
                    Serial.print(lon, 6);
                    Serial.println("]");
                    
                    if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
                        // Store previous values for comparison
                        double prevLat = BLE_LAT;
                        double prevLon = BLE_LON;
                        bool wasPosSet = blePositionSet;
                        
                        // Update BLE position
                        BLE_LAT = lat;
                        BLE_LON = lon;
                        blePositionSet = true;
                        blePositionTime = currentTime;
                        
                        // Calculate distance from previous position if available
                        if (wasPosSet) {
                            // Basic distance calculation (not actual distance but good for comparison)
                            double latDiff = BLE_LAT - prevLat;
                            double lonDiff = BLE_LON - prevLon;
                            double approxDistance = sqrt(latDiff*latDiff + lonDiff*lonDiff) * 111000; // rough meters
                            Serial.print("Position changed by approximately ");
                            Serial.print(approxDistance);
                            Serial.println(" meters from previous BLE position");
                        }
                        
                        Serial.println("*** BLE Position set successfully ***");
                        Serial.print("BLE Position Age: ");
                        Serial.print(0); // Just set, so age is 0
                        Serial.println(" ms");
                    } else {
                        Serial.println("ERROR: Invalid coordinates received (out of range)");
                        Serial.print("Latitude must be between -90 and 90, got: ");
                        Serial.println(lat);
                        Serial.print("Longitude must be between -180 and 180, got: ");
                        Serial.println(lon);
                    }
                } else {
                    Serial.println("ERROR: Missing coordinate fields in JSON");
                    Serial.println("Required fields: either 'lat'+'lon' or 'latitude'+'longitude'");
                }
            } catch (const std::exception& e) {
                Serial.print("EXCEPTION in CurrentPositionCallbacks: ");
                Serial.println(e.what());
            } catch (...) {
                Serial.println("UNKNOWN EXCEPTION during current position processing");
            }
        } else {
            Serial.println("ERROR: BLE current position data invalid length");
            if (value.length() == 0) {
                Serial.println("Received empty data");
            } else {
                Serial.print("Data too long: ");
                Serial.print(value.length());
                Serial.println(" bytes (max 100)");
            }
        }
        
        // Log end of position update processing
        Serial.println("=== BLE POSITION UPDATE COMPLETE ===");
    }
};

class LocationsListCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) override {
        Serial.println("BLE: Client requested saved locations list");
        try {
            // Use a small static buffer
            static char buffer[200];
            strcpy(buffer, "[]"); // Default empty array
            
            if (savedLocations.size() > 0) {
                // Create a small document
                JsonDocument doc;
                JsonArray array = doc.to<JsonArray>();
                
                // Only send a few locations to avoid memory issues
                const size_t maxLocations = 3; // Send very few at a time
                
                for (size_t i = 0; i < savedLocations.size() && i < maxLocations; i++) {
                    JsonObject obj = array.add<JsonObject>();
                    
                    // Safely handle the name
                    if (savedLocations[i].name != nullptr) {
                        obj["name"] = savedLocations[i].name;
                    } else {
                        obj["name"] = "Unnamed";
                    }
                    
                    obj["lat"] = savedLocations[i].lat;
                    obj["lon"] = savedLocations[i].lon;
                }
                
                // Serialize to our buffer with size limit
                serializeJson(doc, buffer, sizeof(buffer));
            }
            
            // Log what's being sent
            Serial.print("BLE: Returning locations: ");
            Serial.println(buffer);
            
            // Use standard string API which is known to be safe
            pCharacteristic->setValue(buffer);
            Serial.print("BLE: Sent ");
            Serial.print(strlen(buffer));
            Serial.println(" bytes of location data");
        } catch (const std::exception& e) {
            Serial.print("Exception in LocationsListCallbacks: ");
            Serial.println(e.what());
            // Fall back to empty array
            pCharacteristic->setValue("[]");
        } catch (...) {
            Serial.println("Unknown exception in LocationsListCallbacks");
            // Fall back to empty array
            pCharacteristic->setValue("[]");
        }
    }
};

void setupBLE() {
    // Log initial target values
    Serial.println("BLE Setup - Initial target values:");
    Serial.print("TARGET_LAT: ");
    Serial.print(TARGET_LAT, 6);
    Serial.print(", TARGET_LON: ");
    Serial.println(TARGET_LON, 6);
    Serial.print("targetIsSet: ");
    Serial.println(targetIsSet ? "true" : "false");
    
    BLEDevice::init(BluetoothName);
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Target Characteristic
    BLECharacteristic *pTargetCharacteristic = pService->createCharacteristic(
        TARGET_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pTargetCharacteristic->setCallbacks(new TargetCharacteristicCallbacks());

    // Locations List Characteristic
    pLocationsListCharacteristic = pService->createCharacteristic(
        LOCATIONS_LIST_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pLocationsListCharacteristic->addDescriptor(new BLE2902());
    pLocationsListCharacteristic->setCallbacks(new LocationsListCallbacks());

    // Locations Modify Characteristic
    BLECharacteristic *pLocationsModifyCharacteristic = pService->createCharacteristic(
        LOCATIONS_MODIFY_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pLocationsModifyCharacteristic->setCallbacks(new LocationsModifyCallbacks());
    
    // Current Position Characteristic
    BLECharacteristic *pCurrentPositionCharacteristic = pService->createCharacteristic(
        CURRENT_POSITION_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCurrentPositionCharacteristic->setCallbacks(new CurrentPositionCallbacks());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false); // Advertise service UUID in the main packet
    pAdvertising->setMinPreferred(0x06);  // For values that are multiples of 1.25ms
    pAdvertising->setMaxPreferred(0x0C);
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising started.");
}


// Returns true if the BLE position is valid and recent (less than 10 seconds old)
bool isBlePositionValid() {
    static uint32_t lastLogTime = 0;
    uint32_t currentTime = millis();
    bool isValid = true;
    
    // Only log detailed status every 5 seconds to avoid spamming Serial
    bool shouldLog = (currentTime - lastLogTime > 5000);
    
    if (!blePositionSet) {
        if (shouldLog) {
            Serial.println("BLE Position Status: Not set yet");
        }
        isValid = false;
    } else {
        // Check if position is older than 10 seconds
        uint32_t positionAge = currentTime - blePositionTime;
        if (positionAge > 10000) {
            if (shouldLog) {
                Serial.print("BLE Position Status: Too old (");
                Serial.print(positionAge / 1000.0, 1);
                Serial.println(" seconds)");
            }
            isValid = false;
        } else if (shouldLog) {
            Serial.print("BLE Position Status: Valid (");
            Serial.print(positionAge / 1000.0, 1);
            Serial.println(" seconds old)");
            Serial.print("BLE Position: [");
            Serial.print(BLE_LAT, 6);
            Serial.print(", ");
            Serial.print(BLE_LON, 6);
            Serial.println("]");
        }
    }
    
    if (shouldLog) {
        lastLogTime = currentTime;
    }
    
    return isValid;
}

// Get the current BLE position
void getBlePosition(double &lat, double &lon) {
    if (isBlePositionValid()) {
        lat = BLE_LAT;
        lon = BLE_LON;
        
        // Log when this position is used
        Serial.println(">>> Using BLE position for location <<<");
        Serial.print("Coordinates: [");
        Serial.print(lat, 6);
        Serial.print(", ");
        Serial.print(lon, 6);
        Serial.println("]");
        Serial.print("Position age: ");
        Serial.print((millis() - blePositionTime) / 1000.0, 1);
        Serial.println(" seconds");
    } else {
        // Return zeros if not valid
        lat = 0.0;
        lon = 0.0;
        Serial.println(">>> BLE position requested but not valid, returning zeros <<<");
    }
}

// Call this from your main loop
void checkBLEStatus() {
    static uint32_t lastStatusTime = 0;
    static uint32_t lastVerifyTime = 0;
    
    // Verify target variables consistency every 30 seconds
    if (millis() - lastVerifyTime > 30000) {
        lastVerifyTime = millis();
        
        // Check for potential issues with the target coordinates
        if (targetIsSet && TARGET_LAT == 0.0 && TARGET_LON == 0.0) {
            Serial.println("WARNING: targetIsSet is true but coordinates are 0.0");
        }
        
        if (!targetIsSet && (TARGET_LAT != 0.0 || TARGET_LON != 0.0)) {
            Serial.println("WARNING: targetIsSet is false but coordinates are non-zero");
            Serial.print("TARGET_LAT: ");
            Serial.print(TARGET_LAT, 6);
            Serial.print(", TARGET_LON: ");
            Serial.println(TARGET_LON, 6);
        }
        
        // Log BLE position status
        if (isBlePositionValid()) {
            Serial.println("BLE position is valid and recent");
        } else if (blePositionSet) {
            Serial.println("BLE position is set but too old");
        }
    }
    
    if (needsLocationsSave) {
        needsLocationsSave = false;
        Serial.println("Saving locations from main loop due to BLE update");
        saveSavedLocations();
        notifySavedLocationsChange();
    }
    
    // Reset BLE position after 60 seconds without updates
    if (blePositionSet && millis() - blePositionTime > 60000) {
        Serial.println("BLE position expired - no updates in 60 seconds");
        blePositionSet = false;
    }
}
