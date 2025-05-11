#include "gpsinfo.h" // Assuming gps_info.h is in the same directory or include path

// Define the global variables
double latitude_ = 0.0;
double longitude_ = 0.0;
double altitude_ = 0.0;
double speed_ = 0.0;
int satellitesInView_ = 0;
int fixQuality_ = 0;

// Setters
void setLatitude(double lat) {
    latitude_ = lat;
}

void setLongitude(double lon) {
    longitude_ = lon;
}

void setAltitude(double alt) {
    altitude_ = alt;
}

void setSpeed(double spd) {
    speed_ = spd;
}

void setSatellitesInView(int count) {
    satellitesInView_ = count;
}

void setFixQuality(int quality) {
    fixQuality_ = quality;
}

// Getters
double getLatitude()  {
    return latitude_;
}

double getLongitude()  {
    return longitude_;
}

double getAltitude()  {
    return altitude_;
}

double getSpeed()  {
    return speed_;
}

int getSatellitesInView()  {
    return satellitesInView_;
}

int getFixQuality()  {
    return fixQuality_;
}

// Utility
bool hasFix()  {
    return fixQuality_ > 0; // Assuming 0 means no fix, and >0 means some kind of fix
}

// Drawing function
void drawGpsInfoPage(M5Canvas &canvas, int centerX, int centerY) {
    canvas.fillSprite(TFT_BLACK); // Clear the screen
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setFont(&fonts::Font2); // Set a font

    char buffer[50]; // Buffer for formatting strings

    int yPos = 30; // Adjusted initial Y position for drawing text, considering centering
    int lineHeight = 25; // Adjusted line height for better spacing

    // --- Centering Text ---
    canvas.setTextDatum(MC_DATUM); // Middle-Center datum for all text

    // Latitude
    sprintf(buffer, "Lat: %.6f", latitude_);
    canvas.drawString(buffer, centerX, yPos);
    yPos += lineHeight;

    // Longitude
    sprintf(buffer, "Lon: %.6f", longitude_);
    canvas.drawString(buffer, centerX, yPos);
    yPos += lineHeight;

    // Altitude
    sprintf(buffer, "Alt: %.2f m", altitude_);
    canvas.drawString(buffer, centerX, yPos);
    yPos += lineHeight;

    // Speed
    sprintf(buffer, "Spd: %.2f km/h", speed_);
    canvas.drawString(buffer, centerX, yPos);
    yPos += lineHeight;

    // Satellites
    sprintf(buffer, "Sats: %d", satellitesInView_);
    canvas.drawString(buffer, centerX, yPos);
    yPos += lineHeight;

    // Fix Quality
    const char* fixQualityStr;
    switch (fixQuality_) {
        case 0: fixQualityStr = "Fix: None"; break;
        case 1: fixQualityStr = "Fix: GPS"; break;
        case 2: fixQualityStr = "Fix: DGPS"; break;
        // Add more cases as per your GPS module's documentation
        default: fixQualityStr = "Fix: Unknown"; break;
    }
    canvas.drawString(fixQualityStr, centerX, yPos);
    yPos += lineHeight;

    // Has Fix
    sprintf(buffer, "Has Fix: %s", hasFix() ? "Yes" : "No");
    canvas.drawString(buffer, centerX, yPos);
    yPos += lineHeight * 2; // Add extra space before the back instruction

    canvas.drawString("Press B to go back", centerX, canvas.height() - 20); // Draw "Back" instruction
}

void handleGpsInfoInput(){
    if (M5.BtnA.wasPressed()) {
        gpsinfoActive = false; 
        menuActive = true;    
    }
}