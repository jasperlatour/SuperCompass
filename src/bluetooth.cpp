#include "bluetooth.h"
#include "page/saved_locations.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#define BluetoothName "SuperCompass"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID           "e393c3ca-4e9f-4d5c-bba0-37e53272f8b3"
#define TARGET_CHAR_UUID       "78afdeb8-a315-4030-8337-629d4e021306"
#define LOCATIONS_LIST_CHAR_UUID "cdefa4dc-b73e-4865-b35f-fafa76914afb"
#define LOCATIONS_MODIFY_CHAR_UUID "c660ca7d-b7ea-4c13-84fe-74dd8a11814d"

BLECharacteristic *pLocationsListCharacteristic;

// Global variables to track BLE connection state
bool btConnected = false;
uint32_t lastBtConnectedTime = 0;

// Flag to save locations after BLE action
bool needsLocationsSave = false;

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
        // Use a small static buffer to avoid memory issues
        static char buffer[200];
        strcpy(buffer, "[]"); // Default empty array
        
        if (savedLocations.size() > 0) {
            // Create a small JsonDocument
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
        
        if (pLocationsListCharacteristic != nullptr) {
            // Use standard string API which is known to be safe
            pLocationsListCharacteristic->setValue(buffer);
            pLocationsListCharacteristic->notify();
        }
    } catch (...) {
        Serial.println("Exception in notifySavedLocationsChange");
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
                        // Log old values
                        Serial.print("Updating target from previous lat: ");
                        Serial.print(TARGET_LAT, 6);
                        Serial.print(", lon: ");
                        Serial.print(TARGET_LON, 6);
                        Serial.print(" to new lat: ");
                        Serial.print(lat, 6);
                        Serial.print(", lon: ");
                        Serial.println(lon, 6);
                        
                        // Update global variables with atomic operations
                        noInterrupts(); // Disable interrupts for critical section
                        TARGET_LAT = lat;
                        TARGET_LON = lon;
                        targetIsSet = true;
                        Setaddress = "BLE Target";
                        interrupts(); // Re-enable interrupts
                        
                        // Force a delay to ensure all values are written
                        delay(10);
                        
                        Serial.println("Target set via BLE.");
                        
                        // Verify values were set correctly
                        Serial.print("Verification - TARGET_LAT: ");
                        Serial.print(TARGET_LAT, 6);
                        Serial.print(", TARGET_LON: ");
                        Serial.println(TARGET_LON, 6);
                        Serial.print("targetIsSet: ");
                        Serial.println(targetIsSet ? "true" : "false");
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
            if (value.length() == 0 || value.length() > 200) { // Limit size
                return;
            }
            
            // Keep things very simple
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, value);
            
            if (error) {
                return;
            }
            
            const char* action = doc["action"].as<const char*>();
            if (!action) return;
            
            if (strcmp(action, "add") == 0) {
                JsonObject data = doc["data"];
                if (data && data.containsKey("name") && data.containsKey("lat") && data.containsKey("lon")) {
                    const char* name = data["name"];
                    if (name) {
                        char* name_copy = new char[strlen(name) + 1];
                        strcpy(name_copy, name);
                        savedLocations.push_back({name_copy, data["lat"], data["lon"]});
                        needsLocationsSave = true;
                    }
                }
            }
            else if (strcmp(action, "edit") == 0) {
                int index = doc["index"];
                if (index >= 0 && index < savedLocations.size()) {
                    JsonObject data = doc["data"];
                    if (data && data.containsKey("name")) {
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
                        
                        needsLocationsSave = true;
                    }
                }
            }
            else if (strcmp(action, "delete") == 0) {
                int index = doc["index"];
                if (index >= 0 && index < savedLocations.size()) {
                    if (savedLocations[index].name) {
                        delete[] savedLocations[index].name;
                    }
                    savedLocations.erase(savedLocations.begin() + index);
                    needsLocationsSave = true;
                }
            }
        } catch (...) {
            Serial.println("Exception in LocationsModifyCallbacks");
        }
    }
};

class LocationsListCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) override {
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
            
            // Use standard string API which is known to be safe
            pCharacteristic->setValue(buffer);
        } catch (...) {
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

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false); // Advertise service UUID in the main packet
    pAdvertising->setMinPreferred(0x06);  // For values that are multiples of 1.25ms
    pAdvertising->setMaxPreferred(0x0C);
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising started.");
}

// Print current target status for debugging
void logTargetStatus() {
    Serial.println("=== TARGET STATUS ===");
    Serial.print("TARGET_LAT: ");
    Serial.print(TARGET_LAT, 6);
    Serial.print(", TARGET_LON: ");
    Serial.println(TARGET_LON, 6);
    Serial.print("targetIsSet: ");
    Serial.println(targetIsSet ? "true" : "false");
    Serial.print("Setaddress: ");
    Serial.println(Setaddress);
    Serial.println("===================");
}

// Call this from your main loop
void checkBLEStatus() {
    static uint32_t lastStatusTime = 0;
    static uint32_t lastVerifyTime = 0;
    
    // Log target status every 5 seconds if BLE is connected
    if (btConnected && millis() - lastStatusTime > 5000) {
        lastStatusTime = millis();
        logTargetStatus();
    }
    
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
    }
    
    if (needsLocationsSave) {
        needsLocationsSave = false;
        Serial.println("Saving locations from main loop due to BLE update");
        saveSavedLocations();
        notifySavedLocationsChange();
    }
}
