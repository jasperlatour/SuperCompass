// drawing.cpp
#include "drawing.h"
#include "bluetooth.h"
#include "page/gpsinfo.h"


#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

// --- Helper Function ---
void drawLetterInternal(M5Canvas& canvas, int x, int y, const char* letter) {
    canvas.drawString(letter, x, y);
}

// Draw the compass background with ticks, rings, and markers rotated so that
// the dial rotates with heading and the north/red arrow remains fixed to screen up.
void drawCompassBackgroundToCanvas(M5Canvas& c, int centerX, int centerY, int R, double heading_rad) {
    // Clear & base rings
    c.fillScreen(TFT_BLACK);
    // Outer ring
    c.fillCircle(centerX, centerY, R, TFT_DARKGREY);
    // Inner face
    c.fillCircle(centerX, centerY, R - 20, TFT_BLACK);

    // We rotate the tick marks opposite to device heading so they appear to spin.
    // heading_rad: 0 means facing North, so no rotation needed; positive heading rotates dial clockwise visually.
    // For screen coordinates (y up negative), we subtract heading for correct rotation.
    for (int ang = 0; ang < 360; ang += 5) {
        double rad = (ang - (heading_rad * 180.0 / M_PI)) * M_PI / 180.0; // convert ang-heading to radians
        int outerR = R;
        int len;
        if (ang % 45 == 0)       len = 16;  // main intercardinal
        else if (ang % 30 == 0)  len = 12;  // cardinal
        else if (ang % 10 == 0)  len = 8;
        else                     len = 4;

        int xOuter = centerX + (int)(outerR * sin(rad));
        int yOuter = centerY - (int)(outerR * cos(rad));
        int xInner = centerX + (int)((outerR - len) * sin(rad));
        int yInner = centerY - (int)((outerR - len) * cos(rad));

        c.drawLine(xInner, yInner, xOuter, yOuter, TFT_WHITE);
    }

    // Rotating cardinal arrows (N red, others white) that move with the dial.
    const float arrowLen = 12.0f;   // length along radial direction
    const float arrowHalfWidth = 8.0f; // half width at base
    for (int i = 0; i < 4; ++i) {
        // base angles: 0=N,90=E,180=S,270=W (degrees)
        int baseDeg = i * 90;
        double baseRad = baseDeg * M_PI / 180.0;
        double radRel = baseRad - heading_rad; // rotate opposite to heading

        // Radial unit vector (screen coords x=sin, y=-cos)
        double ux = sin(radRel);
        double uy = -cos(radRel);
        // Tangential unit vector (perpendicular, clockwise)
        double tx = cos(radRel);
        double ty = sin(radRel);

        // Tip at outer radius
        double tipX = centerX + R * ux;
        double tipY = centerY + R * uy;
        // Base center inward
        double baseCenterX = centerX + (R - arrowLen) * ux;
        double baseCenterY = centerY + (R - arrowLen) * uy;
        // Base corners
        double leftX = baseCenterX - arrowHalfWidth * tx;
        double leftY = baseCenterY - arrowHalfWidth * ty;
        double rightX = baseCenterX + arrowHalfWidth * tx;
        double rightY = baseCenterY + arrowHalfWidth * ty;

        uint16_t col = (i == 0) ? TFT_RED : TFT_WHITE;
        c.fillTriangle((int)tipX, (int)tipY,
                       (int)leftX, (int)leftY,
                       (int)rightX, (int)rightY,
                       col);
    }
}

// Draw rotating labels N,E,S,W around the dial
void drawCompassLabels(M5Canvas& canvas, double heading_rad, int centerX, int centerY, int R) {
    canvas.setTextSize(2);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextColor(TFT_WHITE);

    int labelR = R - 32;
    const char* labels[] = {"N","NE","E","SE","S","SW","W","NW"};

    for (int i = 0; i < 8; ++i) {
        double ang = i * 45.0 * M_PI/180.0 - heading_rad;
        int x = centerX + (int)(labelR * sin(ang));
        int y = centerY - (int)(labelR * cos(ang));
        drawLetterInternal(canvas, x, y, labels[i]);
    }
}

// Draw the dynamic heading and degree text at center
void drawHeadingValue(M5Canvas& c, double heading_deg, int centerX, int centerY) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f°", heading_deg);
    c.setTextSize(6);
    c.setTextDatum(MC_DATUM);
    c.setTextColor(TFT_GREEN, TFT_BLACK);
    c.drawString(buf, centerX, centerY - 10);

    // Small directional suffix
    const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
    int idx = (int)round(heading_deg / 45.0) % 8;
    c.setTextSize(2);
    c.drawString(dirs[idx], centerX, centerY + 20);
}

// Arrow and GPS info remain unchanged...
// ... (rest of your implementation)



void drawTargetArrow(M5Canvas& canvas, double arrowAngleDeg, int centerX, int centerY, int R) {

    // --- Arrow Dimensions (relative to Radius R) ---
    // Adjust these values to change the arrow's shape and size
    const double arrowTipRadius = R * 0.85;  // How far the tip extends from the center
    const double arrowBaseRadius = R * 0.10; // How far the base midpoint is from the center
    const double arrowHalfWidth = R * 0.20;  // Half the width of the arrow base

    // --- Calculate Angle in Radians ---
    // Convert degrees to radians. Adjust by -90 degrees because 0 degrees is UP (negative Y direction)
    // in screen coordinates, while standard math angle 0 is RIGHT (positive X direction).
    // Alternatively, use screen-coordinate specific rotation below.
    // Let's calculate rotation directly for screen coordinates where 0 is up.
    double arrowAngleRad = arrowAngleDeg * M_PI / 180.0;
    double cosA = cos(arrowAngleRad);
    double sinA = sin(arrowAngleRad);

    // --- Calculate Base Vertex Coordinates (Unrotated - Pointing UP) ---
    // Relative to center (0,0)
    // Tip point (A)
    double tipX_rel = 0;
    double tipY_rel = -arrowTipRadius;
    // Base Left point (B)
    double baseLX_rel = -arrowHalfWidth;
    double baseLY_rel = -arrowBaseRadius;
    // Base Right point (C)
    double baseRX_rel = arrowHalfWidth;
    double baseRY_rel = -arrowBaseRadius;
    // Base Midpoint (M)
    double baseMX_rel = 0;
    double baseMY_rel = -arrowBaseRadius;

    // --- Rotate Vertex Coordinates ---
    // Standard 2D rotation formula:
    // x' = x*cos(a) - y*sin(a)
    // y' = x*sin(a) + y*cos(a)
    // Apply to each relative point

    // Rotated Tip (A')
    int Ax = centerX + (int)(tipX_rel * cosA - tipY_rel * sinA);
    int Ay = centerY + (int)(tipX_rel * sinA + tipY_rel * cosA);

    // Rotated Base Left (B')
    int Bx = centerX + (int)(baseLX_rel * cosA - baseLY_rel * sinA);
    int By = centerY + (int)(baseLX_rel * sinA + baseLY_rel * cosA);

    // Rotated Base Right (C')
    int Cx = centerX + (int)(baseRX_rel * cosA - baseRY_rel * sinA);
    int Cy = centerY + (int)(baseRX_rel * sinA + baseRY_rel * cosA);

    // Rotated Base Midpoint (M')
    int Mx = centerX + (int)(baseMX_rel * cosA - baseMY_rel * sinA);
    int My = centerY + (int)(baseMX_rel * sinA + baseMY_rel * cosA);

    // --- Draw the Arrow ---
    // Define the color (assuming TFT_BLUE is available)
    uint16_t arrowColor = TFT_BLUE; // Or canvas.color565(0, 0, 255);

    // Draw the filled right half (Triangle AMC)
    canvas.fillTriangle(Ax, Ay, Mx, My, Cx, Cy, arrowColor);

    // Draw the outlined left half (Triangle AMB)
    // M5Canvas drawTriangle draws the outline connecting the three points.
    canvas.drawTriangle(Ax, Ay, Mx, My, Bx, By, arrowColor);

}


void drawGpsInfo(M5Canvas& canvas, const TinyGPSPlus& gps, int centerX, int centerY) {
    canvas.setTextSize(1);
    canvas.setTextDatum(MC_DATUM); // Middle Center

    // Get the current fix quality to determine if we're using BLE position
    int fixQuality = getFixQuality();
    bool usingBlePosition = (fixQuality == 9);
    
    // If GPS is valid or we're using BLE position
    if (gps.location.isValid() || usingBlePosition) {
        // Get position from our global variables which might come from BLE
        double lat = getLatitude();
        double lon = getLongitude();
        
        //draws the GPS coordinates
        // String lat_str = "Lat: " + String(lat, 4);
        // String lng_str = "Lng: " + String(lon, 4);

        // canvas.setTextColor(TFT_BLACK, TFT_WHITE);
        // canvas.drawString(lat_str, centerX, centerY - 10);
        // canvas.drawString(lng_str, centerX, centerY + 10);
        
        // If using BLE position, show an indicator
        if (usingBlePosition) {
            canvas.setTextColor(TFT_BLUE, TFT_WHITE);
            canvas.drawString("Using BLE Position", centerX, centerY - 30);
        }
    } else {
        // Show "No GPS" if not valid (on canvas)
        canvas.setTextColor(TFT_RED, TFT_WHITE);
        canvas.drawString("No GPS Fix", centerX, centerY - 50);
    }
}

void drawStatusMessage(M5Canvas& canvas, const char* message, int centerX, int yPos, uint16_t color, uint16_t bgColor) {
    canvas.setTextColor(color, bgColor);
    canvas.setTextSize(1);
    canvas.setTextDatum(BC_DATUM); // Bottom Center
    canvas.drawString(message, centerX, yPos);
}

// Global variables for popup notifications
bool popupActive = false;
uint32_t popupEndTime = 0;
String popupMessage = "";
uint16_t popupTextColor = TFT_WHITE;
uint16_t popupBgColor = TFT_BLUE;

void showPopupNotification(const char* message, uint32_t durationMs, uint16_t color, uint16_t bgColor) {
    Serial.print("Showing popup: ");
    Serial.println(message);
    
    // Store popup information in global variables
    popupMessage = String(message);
    popupEndTime = millis() + durationMs;
    popupActive = true;
    popupTextColor = color;
    popupBgColor = bgColor;
    // Do not push to display here; main loop will composite to avoid flicker
}

void drawPopupIfActive(M5Canvas& canvas) {
    if (!popupActive) return;

    // Expired? clear state and return
    if (millis() > popupEndTime) {
        popupActive = false;
        return;
    }

    // Save state
    int oldTextSize = canvas.getTextSizeX();
    uint8_t oldDatum = canvas.getTextDatum();

    canvas.setTextSize(2);
    canvas.setTextDatum(MC_DATUM);

    int popupWidth = canvas.textWidth(popupMessage.c_str()) + 40;
    int popupHeight = 50;
    if (popupWidth > canvas.width() - 10) popupWidth = canvas.width() - 10; // clamp
    int popupX = (canvas.width() - popupWidth) / 2;
    int popupY = (canvas.height() - popupHeight) / 2;

    // Draw body
    canvas.fillRoundRect(popupX, popupY, popupWidth, popupHeight, 15, popupBgColor);
    canvas.drawRoundRect(popupX, popupY, popupWidth, popupHeight, 15, TFT_WHITE);
    canvas.drawRoundRect(popupX+1, popupY+1, popupWidth-2, popupHeight-2, 14, TFT_WHITE);

    canvas.setTextColor(popupTextColor);
    canvas.drawString(popupMessage, canvas.width() / 2, canvas.height() / 2);

    // Restore
    canvas.setTextSize(oldTextSize);
    canvas.setTextDatum(oldDatum);
}
