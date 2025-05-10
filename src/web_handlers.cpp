#include "web_handlers.h"
#include "saved_locations.h"

void sendDynamicResponsePage(const String& title, const String& statusType, const String& headerMsg, const String& bodyMsg, int refreshDelay) {
    String responseHtml(FPSTR(HTML_RESPONSE_TEMPLATE)); // Load from PROGMEM
    
    // Calculate buffer size more accurately
    // Base length + lengths of all replacement strings + some slack
    size_t bufferSize = responseHtml.length() + title.length() + statusType.length() + headerMsg.length() + bodyMsg.length() + 50; 
    char* buffer = new char[bufferSize];

    if (!buffer) {
        Serial.println(F("Failed to allocate memory for response buffer!"));
        server.send(500, "text/plain", "Internal Server Error: Memory allocation failed.");
        return;
    }

    sprintf(buffer, responseHtml.c_str(), 
            title.c_str(), 
            refreshDelay, 
            statusType.c_str(), 
            headerMsg.c_str(), 
            bodyMsg.c_str(), 
            refreshDelay);
    
    server.send(200, "text/html", buffer);
    delete[] buffer; // Free allocated memory
}

void handleRoot() {
    String htmlPage(FPSTR(HTML_PAGE_TEMPLATE)); // Load from PROGMEM
    
    // Calculate buffer size: base length + space for two doubles (e.g., -180.123456) + slack
    size_t bufferSize = htmlPage.length() + (2 * 15) + 50; 
    char* buffer = new char[bufferSize];

    if (!buffer) {
        Serial.println(F("Failed to allocate memory for root page buffer!"));
        server.send(500, "text/plain", "Internal Server Error: Memory allocation failed for root page.");
        return;
    }

    sprintf(buffer, htmlPage.c_str(), TARGET_LAT, TARGET_LON);
    server.send(200, "text/html", buffer);
    delete[] buffer; // Free allocated memory
}

void handleSetTarget() {
    String messageBody = "";
    bool success = false;

    if (server.hasArg("latitude") && server.hasArg("longitude")) {
        String latStr = server.arg("latitude");
        String lonStr = server.arg("longitude");
        double tempLat = latStr.toDouble();
        double tempLon = lonStr.toDouble();

        if (fabs(tempLat) <= 90.0 && fabs(tempLon) <= 180.0) { // Use fabs for double comparison
            TARGET_LAT = tempLat;
            TARGET_LON = tempLon;
            Setaddress = "Coordinates Set"; // Indicate target is set by coordinates
            Serial.print(F("New Target (Lat/Lon): ")); 
            Serial.print(TARGET_LAT, 6); 
            Serial.print(F(", ")); 
            Serial.println(TARGET_LON, 6);
            messageBody = "Target updated successfully!<br>Lat: " + String(TARGET_LAT, 6) + "<br>Lon: " + String(TARGET_LON, 6);
            success = true;
        } else {
            messageBody = "Error: Invalid latitude or longitude values. Latitude must be -90 to 90, Longitude -180 to 180.";
            Serial.println(messageBody);
        }
    } else {
        messageBody = "Error: Missing latitude or longitude parameters.";
        Serial.println(messageBody);
    }
    sendDynamicResponsePage("Target Status", success ? "success" : "error", success ? "Success!" : "Error!", messageBody);
}

void handleSetTargetByAddress() {
    String addressArg = "";
    String messageBody = "";
    bool geocodeOverallSuccess = false;

    if (!server.hasArg("address")) {
        messageBody = "Error: Address parameter missing.";
        Serial.println(messageBody);
        sendDynamicResponsePage("Geocoding Error", "error", "Error!", messageBody);
        return;
    }
    addressArg = server.arg("address");
    Serial.print(F("Address received for geocoding: ")); Serial.println(addressArg);

    if (WiFi.status() != WL_CONNECTED) {
        messageBody = "Error: M5Dial not connected to Wi-Fi with Internet access. Cannot geocode address.";
        Serial.println(messageBody);
        sendDynamicResponsePage("Geocoding Error", "error", "Network Error!", messageBody, 5); // Longer delay
        return;
    }

    HTTPClient http;
    String encodedAddress = "";
    // URL encode the address argument
    for (char c : addressArg) {
        if (isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.') {
            encodedAddress += (c == ' ') ? "%20" : String(c);
        } else {
            char temp[4]; // %XX and null terminator
            sprintf(temp, "%%%02X", (unsigned char)c);
            encodedAddress += temp;
        }
    }

    String url = "https://nominatim.openstreetmap.org/search?q=" + encodedAddress + "&format=json&limit=1&addressdetails=0&extratags=0&namedetails=0";
    Serial.print(F("Geocoding URL: ")); Serial.println(url);
    
    http.begin(url); // Specify URL
    http.setUserAgent(GEOCODING_USER_AGENT); // Set User-Agent from config.h
    int httpCode = http.GET();
    
    double foundLat = 0.0, foundLon = 0.0;
    bool geocodeApiSuccess = false;

    if (httpCode > 0) { // Check for the returning code
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.println(F("Payload received:"));
            //Serial.println(payload); // Can be very long

            // Increased JSON document size for potentially larger responses
            DynamicJsonDocument doc(2048); // Adjust size as needed (was 1024)
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                Serial.print(F("deserializeJson() failed: ")); Serial.println(error.f_str());
                messageBody = "Failed to parse geocoding API response. Error: " + String(error.f_str());
            } else {
                if (doc.is<JsonArray>() && doc.as<JsonArray>().size() > 0) {
                    JsonObject firstResult = doc.as<JsonArray>()[0];
                    if (firstResult.containsKey("lat") && firstResult.containsKey("lon")) {
                        // Nominatim returns lat/lon as strings, convert them
                        foundLat = firstResult["lat"].as<String>().toDouble();
                        foundLon = firstResult["lon"].as<String>().toDouble();
                        geocodeApiSuccess = true;
                        Setaddress = firstResult["display_name"].as<String>(); // Update global Setaddress
                    } else { 
                        messageBody = "Latitude or Longitude not found in API response."; 
                        Serial.println(messageBody); 
                    }
                } else { 
                    messageBody = "No results found for the address, or unexpected API response format (not an array or empty)."; 
                    Serial.println(messageBody); 
                }
            }
        } else {
            messageBody = "Geocoding API request failed. HTTP Error: " + String(httpCode) + " " + http.errorToString(httpCode).c_str();
            Serial.println(messageBody);
        }
    } else {
        messageBody = "Geocoding API request failed. Error: " + String(http.errorToString(httpCode).c_str());
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end(); // Free the resources

    if (geocodeApiSuccess) {
        if (fabs(foundLat) <= 90.0 && fabs(foundLon) <= 180.0) {
            TARGET_LAT = foundLat; 
            TARGET_LON = foundLon;
            Serial.print(F("New Target (Address: '")); Serial.print(addressArg); Serial.print(F("'): ")); 
            Serial.print(TARGET_LAT, 6); Serial.print(F(", ")); Serial.println(TARGET_LON, 6);
            Serial.print(F("Display Name: ")); Serial.println(Setaddress);
            messageBody = "Target updated from address: <b>" + addressArg + "</b><br>(" + Setaddress + ")<br>Lat: " + String(TARGET_LAT, 6) + "<br>Lon: " + String(TARGET_LON, 6);
            geocodeOverallSuccess = true;
        } else {
            messageBody = "Error: Invalid latitude/longitude values received from geocoding API.";
            Serial.println(messageBody);
            Setaddress = "Geocode Coords Invalid"; // Clear or set error for display
        }
    } else {
        if (messageBody.isEmpty()) messageBody = "Could not geocode address. Unknown error.";
        Setaddress = "Geocode Failed"; // Clear or set error for display
    }
    // Longer redirect for errors to allow reading the message
    sendDynamicResponsePage("Geocoding Status", geocodeOverallSuccess ? "success" : "error", 
                           geocodeOverallSuccess ? "Success!" : "Geocoding Error!", messageBody, 
                           geocodeOverallSuccess ? 3 : 7);
}

void handleManageLocationsPage() {
    String htmlPage(FPSTR(HTML_MANAGE_LOCATIONS)); // Load from PROGMEM
    server.send(200, "text/html", htmlPage); // Send the string directly without sprintf
}

void handleGetLocationsJson() {
    DynamicJsonDocument doc(1024 + (savedLocations.size() * 128)); // Adjust size based on expected data
    JsonArray array = doc.to<JsonArray>();

    for (const auto& loc : savedLocations) {
        JsonObject obj = array.createNestedObject();
        obj["name"] = loc.name; // Assumes loc.name is String or const char*
        obj["lat"] = loc.lat;
        obj["lon"] = loc.lon;
    }

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handleAddLocation() {
    if (!server.hasArg("name") || !server.hasArg("latitude") || !server.hasArg("longitude")) {
        server.send(404, "text/plain","Missing parameters");
        return;
    }

    String nameStr = server.arg("name");
    String latStr = server.arg("latitude");
    String lonStr = server.arg("longitude");

    if (nameStr.isEmpty() || latStr.isEmpty() || lonStr.isEmpty()) {
        server.send(404, "text/plain","Parameters cannot be empty");
        return;
    }

    double lat = latStr.toDouble();
    double lon = lonStr.toDouble();

    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        server.send(404, "text/plain","Invalid latitude or longitude values.");
        return;
    }
    
    // Dynamically allocate memory for the name and copy the string
    char* name_copy = new char[nameStr.length() + 1];
    strcpy(name_copy, nameStr.c_str()); // Use strcpy to copy the content

    savedLocations.push_back({name_copy, lat, lon}); // Store the pointer to the new copy

    saveSavedLocations();
    Serial.print("Added location: "); Serial.println(name_copy);
     server.send(200, "text/plain","Location added successfully");
}

void handleDeleteLocation() {
    if (!server.hasArg("index")) {
        server.send(404, "text/plain","Missing index parameter");
        return;
    }

    int index = server.arg("index").toInt();

    if (index < 0 || index >= savedLocations.size()) {
        server.send(404, "text/plain","Invalid index");
        return;
    }

    Serial.print("Deleting location at index "); Serial.print(index); Serial.print(": "); Serial.println(savedLocations[index].name);
    
    // Free the dynamically allocated memory for the name
    delete[] savedLocations[index].name; 

    savedLocations.erase(savedLocations.begin() + index);
    saveSavedLocations();
    server.send(200, "text/plain","Location deleted successfully");
}

void handleGeocodeForAdd() {
    if (!server.hasArg("address")) {
        // Consistently send JSON for errors
        DynamicJsonDocument errorDoc(128);
        errorDoc["status"] = "error";
        errorDoc["message"] = "Error: Address parameter missing.";
        String errorJson;
        serializeJson(errorDoc, errorJson);
        server.send(400, "application/json", errorJson);
        return;
    }
    String addressArg = server.arg("address");
    Serial.print(F("[GeocodeForAdd] Address received: ")); Serial.println(addressArg);

    if (WiFi.status() != WL_CONNECTED) {
        DynamicJsonDocument errorDoc(128);
        errorDoc["status"] = "error";
        errorDoc["message"] = "Error: Not connected to Wi-Fi.";
        String errorJson;
        serializeJson(errorDoc, errorJson);
        server.send(503, "application/json", errorJson);
        return;
    }

    HTTPClient http;
    String encodedAddress = "";
    // URL encode the address argument
    for (char c : addressArg) {
        if (isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.') {
            encodedAddress += (c == ' ') ? "%20" : String(c);
        } else {
            char temp[4]; // %XX and null terminator
            sprintf(temp, "%%%02X", (unsigned char)c);
            encodedAddress += temp;
        }
    }

    String url = "https://nominatim.openstreetmap.org/search?q=" + encodedAddress + "&format=json&limit=1&addressdetails=0&extratags=0&namedetails=0";
    Serial.print(F("[GeocodeForAdd] URL: ")); Serial.println(url);
    
    http.begin(url);
    
    // Simplified User-Agent setting
    const char* userAgent = "M5DialSuperCompass/1.0 (ESP32)";
    #ifdef GEOCODING_USER_AGENT
        if (strlen(GEOCODING_USER_AGENT) > 0) {
            userAgent = GEOCODING_USER_AGENT;
        }
    #endif
    http.setUserAgent(userAgent);

    int httpCode = http.GET();
    String responseJsonPayload = ""; // To hold the final JSON string to send

    if (httpCode > 0) {
        String payload = http.getString(); // Get payload regardless of httpCode for potential error messages from server
        if (httpCode == HTTP_CODE_OK) {
            DynamicJsonDocument doc(1024); 
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                Serial.print(F("[GeocodeForAdd] deserializeJson() failed: ")); Serial.println(error.f_str());
                DynamicJsonDocument errorDoc(256);
                errorDoc["status"] = "error";
                errorDoc["message"] = "Failed to parse API response: " + String(error.f_str());
                serializeJson(errorDoc, responseJsonPayload);
            } else {
                if (doc.is<JsonArray>() && doc.as<JsonArray>().size() > 0) {
                    JsonObject firstResult = doc.as<JsonArray>()[0];
                    if (firstResult.containsKey("lat") && firstResult.containsKey("lon") && firstResult.containsKey("display_name")) {
                        String foundName = firstResult["display_name"].as<String>();
                        double foundLat = firstResult["lat"].as<String>().toDouble(); // Nominatim lat/lon are strings
                        double foundLon = firstResult["lon"].as<String>().toDouble();
                        
                        DynamicJsonDocument successDoc(512); // Increased size slightly for safety
                        successDoc["status"] = "success";
                        successDoc["name"] = foundName;
                        successDoc["lat"] = foundLat;
                        successDoc["lon"] = foundLon;
                        serializeJson(successDoc, responseJsonPayload);
                        Serial.println("[GeocodeForAdd] Success: " + responseJsonPayload);
                    } else { 
                        DynamicJsonDocument errorDoc(128);
                        errorDoc["status"] = "error";
                        errorDoc["message"] = "Lat/Lon/Display_name not found in API response.";
                        serializeJson(errorDoc, responseJsonPayload);
                        Serial.println("[GeocodeForAdd] " + responseJsonPayload);
                    }
                } else { 
                    DynamicJsonDocument errorDoc(128);
                    errorDoc["status"] = "error";
                    errorDoc["message"] = "No results found for the address.";
                    serializeJson(errorDoc, responseJsonPayload);
                    Serial.println("[GeocodeForAdd] " + responseJsonPayload);
                }
            }
        } else { // HTTP error from Nominatim
            DynamicJsonDocument errorDoc(256);
            errorDoc["status"] = "error";
            errorDoc["message"] = "API request failed. HTTP Error: " + String(httpCode) + ". Response: " + payload;
            serializeJson(errorDoc, responseJsonPayload);
            Serial.println("[GeocodeForAdd] " + responseJsonPayload);
        }
    } else { // HTTPClient local error (e.g., connection failed)
        DynamicJsonDocument errorDoc(256);
        errorDoc["status"] = "error";
        errorDoc["message"] = "API request failed. Error: " + String(http.errorToString(httpCode).c_str());
        serializeJson(errorDoc, responseJsonPayload);
        Serial.printf("[GeocodeForAdd] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();

    // Determine final HTTP status code for our server's response
    int serverResponseCode = 500; // Default to server error
    if (!responseJsonPayload.isEmpty()) {
        // Check if the generated JSON indicates success
        DynamicJsonDocument tempCheckDoc(64); // Small doc just to check status field
        deserializeJson(tempCheckDoc, responseJsonPayload);
        if (tempCheckDoc.containsKey("status") && strcmp(tempCheckDoc["status"], "success") == 0) {
            serverResponseCode = 200;
        }
    } else {
        // Should not happen if logic is correct, but as a fallback
        DynamicJsonDocument errorDoc(128);
        errorDoc["status"] = "error";
        errorDoc["message"] = "Internal server error: No JSON payload generated.";
        serializeJson(errorDoc, responseJsonPayload);
    }
    
    server.sendHeader("Access-Control-Allow-Origin", "*"); // Optional: For CORS
    server.send(serverResponseCode, "application/json", responseJsonPayload);
}

void handleNotFound() {
    server.send(404, "text/plain", "404: Not found");
}

void setupServerRoutes() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/settarget", HTTP_POST, handleSetTarget);
    server.on("/settargetbyaddress", HTTP_POST, handleSetTargetByAddress);
    server.on("/manage-locations", HTTP_GET, handleManageLocationsPage);
    server.on("/api/locations", HTTP_GET, handleGetLocationsJson);
    server.on("/api/locations/add", HTTP_POST, handleAddLocation);
    server.on("/api/locations/delete", HTTP_POST, handleDeleteLocation);
    server.on("/api/geocode-for-add", HTTP_GET, handleGeocodeForAdd); // Changed HTTP_POST to HTTP_GET

    server.onNotFound(handleNotFound);
    server.begin(); 
    // IP address is printed in the main setup after connectWiFiWithFallback()
}