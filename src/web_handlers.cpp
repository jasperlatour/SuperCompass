#include "web_handlers.h"
// Assumes globals_and_includes.h and html_content.h are included via web_handlers.h
// External globals (defined in .ino) like 'server', 'TARGET_LAT', 'TARGET_LON', 'Setaddress', 'currentNetworkIP'
// and constants from 'config.h' like 'GEOCODING_USER_AGENT' are accessible.

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

void handleNotFound() {
    server.send(404, "text/plain", "404: Not found");
}

void setupServerRoutes() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/settarget", HTTP_POST, handleSetTarget);
    server.on("/settargetbyaddress", HTTP_POST, handleSetTargetByAddress);
    server.onNotFound(handleNotFound);
    server.begin(); 
    // IP address is printed in the main setup after connectWiFiWithFallback()
}