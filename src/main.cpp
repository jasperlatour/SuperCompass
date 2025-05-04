// #include "M5Dial.h"
// #include <TinyGPS++.h>
// #include <HardwareSerial.h>

// // Initialize TinyGPS++ for parsing NMEA sentences
// TinyGPSPlus gps;

// // Initialize HardwareSerial for UART communication with GPS module
// HardwareSerial GPS_Serial(1);

// void setup() {
//   // Initialize M5Dial
//   auto cfg = M5.config();
//   M5Dial.begin(cfg);

//   // Set up the display
//   M5Dial.Display.setTextColor(TFT_GREEN);
//   M5Dial.Display.setTextDatum(MC_DATUM);
//   M5Dial.Display.setTextFont(2);
//   M5Dial.Display.setTextSize(1);

//   // Initialize UART communication
//   // Port A on M5Dial uses GPIO 13 (TX) and GPIO 15 (RX)
//   GPS_Serial.begin(9600, SERIAL_8N1, 1, 2);

//   // Initialize Serial Monitor
//   Serial.begin(115200);
// }

// void loop() {
//   // Update M5Dial
//   M5Dial.update();

//     if (!GPS_Serial.available() > 0) {
//         Serial.println("No data available");
//     }

//   // Read data from GPS module
//   while (GPS_Serial.available() > 0) {
//     gps.encode(GPS_Serial.read());
//   }

//   // Check if a valid location is available
//   if (gps.location.isUpdated()) {
//     // Clear the display
//     M5Dial.Display.fillScreen(TFT_BLACK);

//     // Retrieve latitude and longitude
//     double latitude = gps.location.lat();
//     double longitude = gps.location.lng();

//     // Create strings to display
//     String lat_str = "Lat: " + String(latitude, 6);
//     String lng_str = "Lng: " + String(longitude, 6);

//     // Display latitude and longitude
//     M5Dial.Display.drawString(lat_str, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 10);
//     M5Dial.Display.drawString(lng_str, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 + 10);

//     // Print latitude and longitude to Serial Monitor
//     Serial.print("Latitude: ");
//     Serial.println(latitude, 6);
//     Serial.print("Longitude: ");
//     Serial.println(longitude, 6);
//     Serial.print("Satellites: ");
//     Serial.println(gps.satellites.value());
//   }

//   // Add a small delay to avoid overwhelming the display
//   delay(500);
// }