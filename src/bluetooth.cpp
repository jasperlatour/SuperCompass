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
// New READY/status characteristic UUID (randomly generated)
#define READY_CHAR_UUID "8a3de9c1-5b06-4d8f-9c0b-f2d7b5b0f9aa"



BLECharacteristic *pLocationsListCharacteristic;
static BLECharacteristic *pTargetCharacteristic = nullptr;
static BLECharacteristic *pReadyCharacteristic = nullptr;
static BLEServer *g_pServer = nullptr; // store server reference for disconnect

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

// Defer target publish out of BLE callback to avoid potential stack re-entrancy issues
static volatile bool targetNeedsPublish = false;

// ---------------- BLE Reliability / Stats (Step 2) ----------------
struct BleStats {
    uint32_t hb = 0;              // heartbeat counter
    uint32_t rxParseErrors = 0;   // JSON / decode failures
    uint32_t queueOverflow = 0;   // (placeholder for future queue implementation)
    uint32_t notifyErrors = 0;    // notification send failures (future use)
    uint32_t jsonPosPackets = 0;  // JSON position packets processed
    uint32_t binaryPosPackets = 0;// binary position packets processed (future)
} g_bleStats;

static uint32_t lastHeartbeatTime = 0;      // ms timestamp of last heartbeat publish
static const uint32_t HEARTBEAT_INTERVAL = 15000; // 15s heartbeat interval

// ---------------- Inbound Message Queue (Step 3) ----------------
// We avoid heavy JSON parsing inside BLE callbacks to reduce timing pressure and risk of re-entrancy issues.
namespace BLEInbound {
    enum class Type : uint8_t { Target=0, LocationsModify=1, Position=2 };
    struct Msg { Type type; uint8_t len; char data[128]; };
    static constexpr uint8_t QSIZE = 8; // ring buffer size
    static Msg queue[QSIZE];
    static volatile uint8_t head = 0; // write position
    static volatile uint8_t tail = 0; // read position

    bool enqueue(Type t, const char* src, size_t len){
        if(len == 0 || len > 120){
            g_bleStats.rxParseErrors++; // treat oversize as parse error category
            return false;
        }
        uint8_t next = (head + 1) % QSIZE;
        if(next == tail){
            g_bleStats.queueOverflow++;
            return false; // queue full
        }
        Msg &m = queue[head];
        m.type = t;
        m.len = (uint8_t)len;
        memcpy(m.data, src, len);
        m.data[len] = '\0'; // zero terminate for safe string ops
        head = next;
        return true;
    }

    bool dequeue(Msg &out){
        if(tail == head) return false; // empty
        out = queue[tail];
        tail = (tail + 1) % QSIZE;
        return true;
    }
}

// ---------------- Outbound Notification Queue (Step 5) ----------------
namespace BLEOutbound {
    struct Msg { BLECharacteristic* ch; uint8_t len; uint8_t data[180]; }; // len <= 180 (< MTU-3 if MTU 185)
    static constexpr uint8_t QSIZE = 8;
    static Msg queue[QSIZE];
    static volatile uint8_t head = 0; // write
    static volatile uint8_t tail = 0; // read

    bool enqueue(BLECharacteristic* ch, const uint8_t* buf, size_t len){
        if(!ch || len == 0 || len > 180) return false;
        uint8_t next = (head + 1) % QSIZE;
        if(next == tail){
            g_bleStats.queueOverflow++; // reuse counter for outbound overflow
            return false;
        }
        Msg &m = queue[head];
        m.ch = ch; m.len = (uint8_t)len;
        memcpy(m.data, buf, len);
        head = next;
        return true;
    }

    bool dequeue(Msg &out){
        if(tail == head) return false;
        out = queue[tail];
        tail = (tail + 1) % QSIZE;
        return true;
    }
}

// ---------------- Locations Chunk Sending (Step 6) ----------------
static uint16_t locationsChunkNextIndex = 0; // next savedLocations index to send when chunking
static uint16_t locationsChunkTotal = 0;     // total locations snapshot at start of sending
static bool locationsChunkInProgress = false;
static const uint8_t LOCATIONS_PER_CHUNK = 3; // keep JSON small

static void scheduleNextLocationsChunk(){
    if(!pLocationsListCharacteristic) return;
    if(!locationsChunkInProgress) return;
    if(locationsChunkNextIndex >= locationsChunkTotal){
        locationsChunkInProgress = false; // done
        return;
    }
    StaticJsonDocument<256> doc; // chunk doc
    JsonArray items = doc.createNestedArray("items");
    uint8_t count = 0;
    for(; locationsChunkNextIndex < locationsChunkTotal && count < LOCATIONS_PER_CHUNK; ++locationsChunkNextIndex, ++count){
        if(locationsChunkNextIndex < savedLocations.size()){
            JsonObject o = items.createNestedObject();
            o["name"] = savedLocations[locationsChunkNextIndex].name ? savedLocations[locationsChunkNextIndex].name : "Unnamed";
            o["lat"] = savedLocations[locationsChunkNextIndex].lat;
            o["lon"] = savedLocations[locationsChunkNextIndex].lon;
        }
    }
    doc["chunk"] = (locationsChunkNextIndex / LOCATIONS_PER_CHUNK) - 1; // sequence (after loop increment)
    uint16_t totalChunks = (locationsChunkTotal + LOCATIONS_PER_CHUNK - 1) / LOCATIONS_PER_CHUNK;
    doc["total"] = totalChunks;
    doc["final"] = (locationsChunkNextIndex >= locationsChunkTotal);
    char out[256]; size_t n = serializeJson(doc, out, sizeof(out));
    pLocationsListCharacteristic->setValue((uint8_t*)out, n);
    if(btConnected){
        // enqueue notify; if fails, we'll retry next status loop (queue overflow increments)
        BLEOutbound::enqueue(pLocationsListCharacteristic, (uint8_t*)out, n);
    }
}

class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("BLE Client Connected");
      btConnected = true;
      lastBtConnectedTime = millis();
      // Show popup notification for connection
      showPopupNotification("Connected", 2000, TFT_WHITE, TFT_GREEN);
    }

    void onDisconnect(BLEServer* pServer) {
      Serial.println("BLE Client Disconnected");
      btConnected = false;
      BLEDevice::startAdvertising(); // Restart advertising on disconnect
      // Show popup notification for disconnection
      showPopupNotification("Disconnected", 2000, TFT_WHITE, TFT_RED);
    }
};

void notifySavedLocationsChange() {
    try {
        Serial.println("BLE: Preparing to notify saved locations change");
    Serial.print("BLE: Total saved locations: "); Serial.println(savedLocations.size());
    // Start chunk sending sequence
    locationsChunkTotal = savedLocations.size();
    locationsChunkNextIndex = 0;
    locationsChunkInProgress = true;
    scheduleNextLocationsChunk(); // enqueue first chunk
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
        Serial.print("BLE Target received raw len: "); Serial.println(value.length());
        if(!BLEInbound::enqueue(BLEInbound::Type::Target, value.c_str(), value.length())){
            Serial.println("BLE Target enqueue failed (size or queue)");
        }
    }
};

class LocationsModifyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        Serial.print("BLE LocationsModify raw len: "); Serial.println(value.length());
        if(!BLEInbound::enqueue(BLEInbound::Type::LocationsModify, value.c_str(), value.length())){
            Serial.println("BLE LocationsModify enqueue failed (size or queue)");
        }
    }
};

class CurrentPositionCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if(!value.empty()){
            if(!BLEInbound::enqueue(BLEInbound::Type::Position, value.c_str(), value.length())){
                Serial.println("BLE Position enqueue failed (size or queue)");
            }
        }
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

// Helper to publish target in JSON form (public API declared in header)
void publishTargetCharacteristic(){
    if(!pTargetCharacteristic) return;
    // We keep JSON small to reduce heap pressure and avoid malloc failures in callbacks.
    // Also avoid using explicit JSON nulls for numeric fields (some central apps or
    // BLE debug utilities have been observed to mishandle them). Instead include a
    // hasTarget flag and only add coordinates when present.
    StaticJsonDocument<128> doc;  // smaller static buffer
    doc["hasTarget"] = targetIsSet;
    if(targetIsSet){
        doc["name"] = Setaddress.c_str();
        doc["lat"] = TARGET_LAT;  // double
        doc["lon"] = TARGET_LON;  // double
    } else {
        doc["name"] = "";            // Empty string
        // Do NOT add lat/lon keys when not set; keeps JSON shorter & unambiguous
    }

    char out[128];
    size_t n = serializeJson(doc, out, sizeof(out));

    if(n == 0 || n >= sizeof(out)){
        // Fallback to a minimal JSON (should never happen unless truncated)
        const char *fallback = "{\"hasTarget\":false}";
        Serial.println("publishTargetCharacteristic: JSON serialization truncated, using fallback");
        pTargetCharacteristic->setValue((uint8_t*)fallback, strlen(fallback));
    } else {
        // Log (trimmed) outgoing JSON for diagnostics
        Serial.print("publishTargetCharacteristic JSON (len=");
        Serial.print(n);
        Serial.print("): ");
        Serial.println(out);
        pTargetCharacteristic->setValue((uint8_t*)out, n);
    }

    // Notify only if connected; descriptor (BLE2902) handles whether client enabled notifications
    if(btConnected){
        BLEOutbound::enqueue(pTargetCharacteristic, (uint8_t*)pTargetCharacteristic->getValue().c_str(), strlen(pTargetCharacteristic->getValue().c_str()));
    }
}

// Helper to publish ready state
void publishReady(bool ready){
    if(!pReadyCharacteristic) return;
    // Expanded READY/heartbeat JSON (keep under ~120 bytes to fit in single notification easily)
    StaticJsonDocument<128> doc;
    doc["ready"] = ready;
    doc["hasTarget"] = targetIsSet;
    doc["fw"] = "1.0.0"; // firmware version placeholder
    doc["hb"] = g_bleStats.hb; // heartbeat counter
    JsonObject err = doc.createNestedObject("err");
    err["rx"] = g_bleStats.rxParseErrors;
    err["qov"] = g_bleStats.queueOverflow;
    err["nt"] = g_bleStats.notifyErrors;

    char out[128];
    size_t n = serializeJson(doc, out, sizeof(out));
    pReadyCharacteristic->setValue((uint8_t*)out, n);
    if(btConnected){
        BLEOutbound::enqueue(pReadyCharacteristic, (uint8_t*)pReadyCharacteristic->getValue().c_str(), strlen(pReadyCharacteristic->getValue().c_str()));
    }
}

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
    // (Optional) Request higher MTU if using NimBLE (macro may differ). Original Bluedroid stack often auto-negotiates.
    // Uncomment if supported in your build:
    // BLEDevice::setMTU(185);
    g_pServer = BLEDevice::createServer();
    g_pServer->setCallbacks(new ServerCallbacks());
    BLEService *pService = g_pServer->createService(SERVICE_UUID);

    // Target Characteristic (now READ + WRITE + NOTIFY)
    pTargetCharacteristic = pService->createCharacteristic(
        TARGET_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
    );
    pTargetCharacteristic->addDescriptor(new BLE2902());
    pTargetCharacteristic->setCallbacks(new TargetCharacteristicCallbacks());

    // READY Characteristic (READ + NOTIFY)
    pReadyCharacteristic = pService->createCharacteristic(
        READY_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pReadyCharacteristic->addDescriptor(new BLE2902());

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

    // Publish initial values BEFORE advertising so central can read immediately after connect
    publishTargetCharacteristic();
    publishReady(true); // Currently no long init, so mark ready immediately

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false); // Advertise service UUID in the main packet
    // Tune the peripheral preferred connection parameters (PPCP) to target ~30-50ms interval.
    // Values are in units of 1.25ms. Common values used by ESP32 BLE stack:
    // 0x06 (7.5ms), 0x12 (25ms), 0x18 (30ms), 0x28 (50ms), 0x30 (60ms).
    // We expose a slightly wider range so the central (phone) can pick one within target band.
    pAdvertising->setMinPreferred(0x18);  // 0x18 * 1.25ms = 30ms
    pAdvertising->setMaxPreferred(0x28);  // 0x28 * 1.25ms = 50ms
    // Note: For finer control (e.g. supervision timeout / latency) we'd need lower level GAP param update
    // via esp_ble_gap_update_conn_params which requires the peer address from GATT connect event. That can
    // be added later if we migrate to NimBLE or install a custom GAP callback.
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising started (with target + ready published).");
}

void disconnectBluetooth(){
    if(btConnected && g_pServer){
        Serial.println("Forcing BLE disconnect");
        // Attempt generic disconnect. Some stacks ignore invalid IDs; try 0..3
        for(int id=0; id<4; ++id){
            g_pServer->disconnect(id);
        }
        delay(100);
    }
}


// Returns true if the BLE position is valid (i.e., has been set).
// It also logs the age of the position.
bool isBlePositionValid() {
    static uint32_t lastLogTime = 0;
    uint32_t currentTime = millis();
    
    // Only log detailed status every 5 seconds to avoid spamming Serial
    bool shouldLog = (currentTime - lastLogTime > 5000);
    
    if (!blePositionSet) {
        if (shouldLog) {
            Serial.println("BLE Position Status: Not set yet");
            lastLogTime = currentTime;
        }
        return false;
    }

    uint32_t positionAge = currentTime - blePositionTime;
    if (shouldLog) {
        if (positionAge > 30000) {
            Serial.print("BLE Position Status: Stale (");
            Serial.print(positionAge / 1000.0, 1);
            Serial.println(" seconds old)");
        } else {
            Serial.print("BLE Position Status: Valid (");
            Serial.print(positionAge / 1000.0, 1);
            Serial.println(" seconds old)");
        }
        Serial.print("BLE Position: [");
        Serial.print(BLE_LAT, 6);
        Serial.print(", ");
        Serial.print(BLE_LON, 6);
        Serial.println("]");
        lastLogTime = currentTime;
    }
    
    return true;
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
    static uint32_t lastAdvWatchdog = 0; // advertising watchdog timer
    
    // Handle deferred target publish early in loop context
    if (targetNeedsPublish) {
        // Clear first to avoid re-trigger if publish triggers another set
        targetNeedsPublish = false;
        Serial.println("checkBLEStatus: publishing deferred target characteristic");
        publishTargetCharacteristic();
    }

    // Process inbound BLE messages (JSON parsing outside ISR/stack callback context)
    BLEInbound::Msg msg; // local copy
    while(BLEInbound::dequeue(msg)){
        switch(msg.type){
            case BLEInbound::Type::Target: {
                Serial.println("Processing queued Target message");
                JsonDocument doc;
                auto err = deserializeJson(doc, msg.data, msg.len);
                if(err){
                    g_bleStats.rxParseErrors++;
                    Serial.print("Target JSON parse error: "); Serial.println(err.c_str());
                    break;
                }
                double lat=0, lon=0; bool valid=false;
                if(doc.containsKey("lat") && doc.containsKey("lon")){
                    lat = doc["lat"].as<double>(); lon = doc["lon"].as<double>(); valid=true; }
                else if(doc.containsKey("latitude") && doc.containsKey("longitude")){
                    lat = doc["latitude"].as<double>(); lon = doc["longitude"].as<double>(); valid=true; }
                if(valid && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180){
                    TARGET_LAT = lat; TARGET_LON = lon; targetIsSet = true;
                    if(doc.containsKey("name")) Setaddress = doc["name"].as<const char*>(); else Setaddress = "BLE Target";
                    const size_t MAX_ADDR_LEN = 40; if(Setaddress.length() > MAX_ADDR_LEN) Setaddress.remove(MAX_ADDR_LEN);
                    targetNeedsPublish = true; // publish updated target soon
                } else {
                    Serial.println("Target JSON invalid or out of range");
                }
            } break;
            case BLEInbound::Type::LocationsModify: {
                Serial.println("Processing queued LocationsModify message");
                JsonDocument doc;
                auto err = deserializeJson(doc, msg.data, msg.len);
                if(err){ g_bleStats.rxParseErrors++; Serial.print("Locations JSON parse error: "); Serial.println(err.c_str()); break; }
                const char* action = doc["action"].as<const char*>();
                if(!action){ g_bleStats.rxParseErrors++; Serial.println("Locations: missing action"); break; }
                if(strcmp(action,"add")==0){
                    JsonObject data = doc.containsKey("data") ? doc["data"] : doc["location"];
                    if(!data){ Serial.println("Add missing data/location"); break; }
                    const char* name = data["name"] | "Unnamed";
                    double lat=0, lon=0; bool have=false;
                    if(data.containsKey("lat") && data.containsKey("lon")){ lat=data["lat"]; lon=data["lon"]; have=true; }
                    else if(data.containsKey("latitude") && data.containsKey("longitude")){ lat=data["latitude"]; lon=data["longitude"]; have=true; }
                    if(have && name && lat>=-90 && lat<=90 && lon>=-180 && lon<=180){
                        char* name_copy = new char[strlen(name)+1]; strcpy(name_copy, name);
                        savedLocations.push_back({name_copy, lat, lon}); needsLocationsSave = true; }
                    else { Serial.println("Add invalid fields"); }
                } else if(strcmp(action,"edit")==0){
                    int index = doc["index"] | -1;
                    JsonObject data = doc["data"];
                    if(index>=0 && index < (int)savedLocations.size() && data && data.containsKey("name")){
                        if(savedLocations[index].name) delete[] savedLocations[index].name;
                        const char* name = data["name"].as<const char*>();
                        char* name_copy = new char[strlen(name)+1]; strcpy(name_copy, name);
                        savedLocations[index].name = name_copy;
                        if(data.containsKey("lat")) savedLocations[index].lat = data["lat"];
                        if(data.containsKey("lon")) savedLocations[index].lon = data["lon"];
                        needsLocationsSave = true;
                    } else { Serial.println("Edit invalid index or data"); }
                } else if(strcmp(action,"delete")==0){
                    int index = doc["index"] | -1;
                    if(index>=0 && index < (int)savedLocations.size()){
                        if(savedLocations[index].name) delete[] savedLocations[index].name;
                        savedLocations.erase(savedLocations.begin()+index); needsLocationsSave = true;
                    } else { Serial.println("Delete invalid index"); }
                } else if(strcmp(action,"resetStats")==0){
                    Serial.println("Resetting BLE stats on request");
                    uint32_t preservedHb = g_bleStats.hb; // keep heartbeat continuity
                    g_bleStats = BleStats{}; // reset
                    g_bleStats.hb = preservedHb; // restore counter
                    publishReady(true); // immediate status update
                } else {
                    Serial.print("Unknown locations action: "); Serial.println(action);
                }
            } break;
            case BLEInbound::Type::Position: {
                // Support compact binary frame OR JSON.
                // Binary format (12 bytes):
                // [0]=0xA1 magic, [1..4]=lat *1e7 (int32 LE), [5..8]=lon *1e7 (int32 LE), [9..10]=accuracy*100 (uint16 LE), [11]=flags
                if(msg.len == 12 && (uint8_t)msg.data[0] == 0xA1){
                    const uint8_t *b = reinterpret_cast<const uint8_t*>(msg.data);
                    int32_t latE7 = (int32_t)( (uint32_t)b[1] | ((uint32_t)b[2]<<8) | ((uint32_t)b[3]<<16) | ((uint32_t)b[4]<<24) );
                    int32_t lonE7 = (int32_t)( (uint32_t)b[5] | ((uint32_t)b[6]<<8) | ((uint32_t)b[7]<<16) | ((uint32_t)b[8]<<24) );
                    uint16_t acc = (uint16_t)( b[9] | (b[10]<<8) );
                    uint8_t flags = b[11];
                    double lat = latE7 / 1e7;
                    double lon = lonE7 / 1e7;
                    if(lat>=-90 && lat<=90 && lon>=-180 && lon<=180){
                        BLE_LAT = lat; BLE_LON = lon; blePositionSet = true; blePositionTime = millis(); g_bleStats.binaryPosPackets++;
                    } else {
                        g_bleStats.rxParseErrors++; Serial.println("Binary position out of range");
                    }
                    (void)acc; (void)flags; // placeholders for future use
                } else {
                    JsonDocument doc;
                    auto err = deserializeJson(doc, msg.data, msg.len);
                    if(err){ g_bleStats.rxParseErrors++; Serial.print("Position JSON parse error: "); Serial.println(err.c_str()); break; }
                    double lat=0, lon=0; bool valid=false;
                    if(doc.containsKey("lat") && doc.containsKey("lon")){ lat=doc["lat"].as<double>(); lon=doc["lon"].as<double>(); valid=true; }
                    else if(doc.containsKey("latitude") && doc.containsKey("longitude")){ lat=doc["latitude"].as<double>(); lon=doc["longitude"].as<double>(); valid=true; }
                    if(valid && lat>=-90 && lat<=90 && lon>=-180 && lon<=180){
                        BLE_LAT = lat; BLE_LON = lon; blePositionSet = true; blePositionTime = millis(); g_bleStats.jsonPosPackets++; }
                    else { Serial.println("Position invalid or out of range"); }
                }
            } break;
        }
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

    // Advertising watchdog: ensure we are advertising whenever not connected.
    // Some phone stacks (or occasional ESP32 stack glitches) can leave us non-advertising after
    // rapid connect/disconnect cycles. Restart advertising every 5s if not connected.
    if (!btConnected && millis() - lastAdvWatchdog > 5000) {
        lastAdvWatchdog = millis();
        Serial.println("BLE watchdog: ensuring advertising is active");
        BLEDevice::startAdvertising();
    }

    // Heartbeat: publish READY/status JSON every HEARTBEAT_INTERVAL while connected.
    if (btConnected && millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
        lastHeartbeatTime = millis();
        g_bleStats.hb++;
        publishReady(true);
    }

    // Outbound notification dispatcher: send one queued notify per loop iteration to avoid bursts.
    BLEOutbound::Msg outMsg;
    if(btConnected && BLEOutbound::dequeue(outMsg)){
        // Set characteristic value (already set by origin, but ensure correct copy for reliability if needed)
        outMsg.ch->setValue(outMsg.data, outMsg.len);
        outMsg.ch->notify();
    }

    // If locations chunking is active and there is room in outbound queue, schedule next chunk.
    if(locationsChunkInProgress){
        // Simple heuristic: only attempt scheduling if queue not near full
        // (We approximate fullness by ensuring at least one free slot)
        BLEOutbound::Msg dummy;
        // Use head/tail distance directly (not exposed) - instead optimistically attempt schedule every cycle
        scheduleNextLocationsChunk();
    }
}

