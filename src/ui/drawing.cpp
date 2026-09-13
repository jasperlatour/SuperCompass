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

// Draw the compass background with ticks and rings rotated so that the dial
// rotates with heading. A fixed marker (drawn separately) marks screen-up as
// the device's current facing direction.
void drawCompassBackgroundToCanvas(M5Canvas& c, int centerX, int centerY, int R, double heading_rad) {
    // Clear & base rings (smooth-edged so the dial doesn't look pixelated on the round panel)
    c.fillScreen(THEME_BG);
    c.fillSmoothCircle(centerX, centerY, R, THEME_PANEL);
    c.fillSmoothCircle(centerX, centerY, R - 18, THEME_BG);

    double heading_deg = heading_rad * 180.0 / M_PI;

    // One tick every 10 degrees keeps the face legible instead of a dense 5-degree ring.
    for (int ang = 0; ang < 360; ang += 10) {
        double rad = (ang - heading_deg) * M_PI / 180.0;

        int len;
        float halfWidth;
        uint16_t color;
        if (ang == 0)            { len = 18; halfWidth = 2.0f; color = THEME_ACCENT_NORTH; } // North
        else if (ang % 90 == 0)  { len = 15; halfWidth = 1.5f; color = THEME_TEXT_MUTED; }   // E/S/W
        else if (ang % 45 == 0)  { len = 11; halfWidth = 1.0f; color = THEME_TEXT_MUTED; }   // intercardinal
        else                     { len = 6;  halfWidth = 0.6f; color = THEME_TEXT_DIM; }     // minor

        int xOuter = centerX + (int)(R * sin(rad));
        int yOuter = centerY - (int)(R * cos(rad));
        int xInner = centerX + (int)((R - len) * sin(rad));
        int yInner = centerY - (int)((R - len) * cos(rad));

        c.drawWideLine(xInner, yInner, xOuter, yOuter, halfWidth, color);
    }
}

// Fixed "lubber line" marker at the top of the dial: always points to the
// direction the device currently faces, regardless of heading rotation.
void drawFixedHeadingMarker(M5Canvas& c, int centerX, int centerY, int R) {
    int tipY = centerY - R + 3;
    int baseY = centerY - R + 15;
    const int halfW = 6;

    c.fillTriangle(centerX, tipY, centerX - halfW, baseY, centerX + halfW, baseY, THEME_ACCENT_MARKER);
    c.drawTriangle(centerX, tipY, centerX - halfW, baseY, centerX + halfW, baseY, THEME_BG);
}

// Draw rotating labels N,E,S,W around the dial
void drawCompassLabels(M5Canvas& canvas, double heading_rad, int centerX, int centerY, int R) {
    canvas.setTextSize(2);
    canvas.setTextDatum(MC_DATUM);

    int labelR = R - 32;
    const char* labels[] = {"N","NE","E","SE","S","SW","W","NW"};

    for (int i = 0; i < 8; ++i) {
        double ang = i * 45.0 * M_PI/180.0 - heading_rad;
        int x = centerX + (int)(labelR * sin(ang));
        int y = centerY - (int)(labelR * cos(ang));
        canvas.setTextColor(i == 0 ? THEME_ACCENT_NORTH : THEME_TEXT_PRIMARY);
        drawLetterInternal(canvas, x, y, labels[i]);
    }
}

// Draw the dynamic heading and degree text at center
void drawHeadingValue(M5Canvas& c, double heading_deg, int centerX, int centerY) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f°", heading_deg);
    c.setTextSize(4);
    c.setTextDatum(MC_DATUM);
    c.setTextColor(THEME_ACCENT_PRIMARY, THEME_BG);
    c.drawString(buf, centerX, centerY - 6);

    // Small directional suffix
    const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
    int idx = (int)round(heading_deg / 45.0) % 8;
    c.setTextSize(2);
    c.setTextColor(THEME_TEXT_PRIMARY, THEME_BG);
    c.drawString(dirs[idx], centerX, centerY + 14);
}

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
    // Solid dart in the target accent color, filled on both halves for a bold,
    // unambiguous pointer (previously one half was outline-only).
    uint16_t arrowColor = THEME_ACCENT_TARGET;

    canvas.fillTriangle(Ax, Ay, Mx, My, Cx, Cy, arrowColor);
    canvas.fillTriangle(Ax, Ay, Mx, My, Bx, By, arrowColor);

}


void drawGpsInfo(M5Canvas& canvas, const TinyGPSPlus& gps, int centerX, int centerY) {
    // Only surface this when there's something noteworthy (no fix, or using BLE
    // instead of GPS) - a normal fix stays out of the way of the heading readout.
    int fixQuality = getFixQuality();
    bool usingBlePosition = (fixQuality == 9);

    const char* msg;
    uint16_t bg, fg;
    if (usingBlePosition) {
        msg = "BLE Position";
        bg = TFT_NAVY; fg = TFT_SKYBLUE;
    } else if (!gps.location.isValid()) {
        msg = "No GPS Fix";
        bg = THEME_PANEL; fg = THEME_WARN;
    } else {
        return;
    }

    canvas.setTextSize(1);
    canvas.setTextDatum(MC_DATUM);
    int yPos = centerY - 60;
    int w = canvas.textWidth(msg) + 16;
    int h = 18;

    canvas.fillSmoothRoundRect(centerX - w / 2, yPos - h / 2, w, h, h / 2, bg);
    canvas.setTextColor(fg, bg);
    canvas.drawString(msg, centerX, yPos);
}

// Formats a distance for display: meters below 1km, kilometers (1 decimal) above.
static String formatDistanceForDisplay(double meters) {
    if (meters < 1000.0) {
        return String((int)round(meters)) + " m";
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f km", meters / 1000.0);
    return String(buf);
}

void drawTargetInfoBanner(M5Canvas& canvas, int centerX, int centerY, int R,
                           bool targetSet, bool locationValid,
                           const String& targetName, double distanceMeters) {
    String line1, line2;
    uint16_t bg, fg;

    if (!targetSet) {
        line1 = "No Target Set";
        bg = THEME_PANEL; fg = THEME_TEXT_MUTED;
    } else if (!locationValid) {
        line1 = targetName;
        line2 = "Waiting for fix...";
        bg = THEME_PANEL; fg = THEME_WARN;
    } else {
        line1 = targetName;
        line2 = formatDistanceForDisplay(distanceMeters);
        bg = THEME_ACCENT_TARGET; fg = THEME_BG;
    }

    canvas.setTextSize(1);
    canvas.setTextDatum(MC_DATUM);

    int w1 = canvas.textWidth(line1.c_str());
    int w2 = line2.length() ? canvas.textWidth(line2.c_str()) : 0;
    int w = (w1 > w2 ? w1 : w2) + 24;
    if (w > canvas.width() - 4) w = canvas.width() - 4; // stay within the round panel

    int h = line2.length() ? 34 : 20;
    int yPos = centerY + 52;
    int x = centerX - w / 2;
    int y = yPos - h / 2;
    int cornerR = (h / 2 < 14) ? h / 2 : 14;

    canvas.fillSmoothRoundRect(x, y, w, h, cornerR, bg);
    canvas.setTextColor(fg, bg);
    if (line2.length()) {
        canvas.drawString(line1, centerX, yPos - 8);
        canvas.drawString(line2, centerX, yPos + 8);
    } else {
        canvas.drawString(line1, centerX, yPos);
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
uint32_t popupDurationMs = 0;

void showPopupNotification(const char* message, uint32_t durationMs, uint16_t color, uint16_t bgColor) {
    Serial.print("Showing popup: ");
    Serial.println(message);

    // Store popup information in global variables
    popupMessage = String(message);
    popupDurationMs = durationMs;
    popupEndTime = millis() + durationMs;
    popupActive = true;
    popupTextColor = color;
    popupBgColor = bgColor;
    // Do not push to display here; main loop will composite to avoid flicker
}

void drawPopupIfActive(M5Canvas& canvas) {
    if (!popupActive) return;

    uint32_t now = millis();

    // Expired? clear state and return
    if (now > popupEndTime) {
        popupActive = false;
        return;
    }

    // Ease in/out over 150ms at each end (smoothstep) so the popup slides into
    // place and eases back out, instead of snapping on/off instantly.
    const float transitionMs = 150.0f;
    uint32_t remaining = popupEndTime - now;
    uint32_t elapsed = popupDurationMs - remaining;

    float entrance = (transitionMs > 0.0f) ? min(1.0f, elapsed / transitionMs) : 1.0f;
    float exit = (transitionMs > 0.0f) ? min(1.0f, remaining / transitionMs) : 1.0f;
    float t = min(entrance, exit);
    float eased = t * t * (3.0f - 2.0f * t); // smoothstep

    int slideOffset = (int)((1.0f - eased) * 24.0f); // slides up into place, eases back down on exit

    // Save state
    int oldTextSize = canvas.getTextSizeX();
    uint8_t oldDatum = canvas.getTextDatum();

    canvas.setTextSize(2);
    canvas.setTextDatum(MC_DATUM);

    int popupWidth = canvas.textWidth(popupMessage.c_str()) + 40;
    int popupHeight = 50;
    if (popupWidth > canvas.width() - 10) popupWidth = canvas.width() - 10; // clamp
    int popupX = (canvas.width() - popupWidth) / 2;
    int popupY = (canvas.height() - popupHeight) / 2 + slideOffset;

    // Draw body
    canvas.fillRoundRect(popupX, popupY, popupWidth, popupHeight, 15, popupBgColor);
    canvas.drawRoundRect(popupX, popupY, popupWidth, popupHeight, 15, TFT_WHITE);
    canvas.drawRoundRect(popupX+1, popupY+1, popupWidth-2, popupHeight-2, 14, TFT_WHITE);

    canvas.setTextColor(popupTextColor);
    canvas.drawString(popupMessage, canvas.width() / 2, canvas.height() / 2 + slideOffset);

    // Restore
    canvas.setTextSize(oldTextSize);
    canvas.setTextDatum(oldDatum);
}
