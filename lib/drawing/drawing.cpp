// drawing.cpp
#include "drawing.h"
#include "M5Dial.h" // Include again for implementation details like setTextSize, colors, etc.
#include <math.h>   // For sin(), cos()


// Define PI if not already defined (it usually is in math.h)
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

// --- Helper Function (Internal to drawing.cpp, no need to declare in .h) ---
// Function to draw a single letter at a specific position on the main canvas
void drawLetterInternal(M5Canvas& canvas, int x, int y, const char* letter) {
    // Assuming text datum, color, size are set before calling this
    canvas.drawString(letter, x, y);
}
// --- End Helper Function ---


// --- Function Implementations ---

void drawCompassBackgroundToCanvas(M5Canvas& c, int centerX, int centerY, int R) {
    int textsize = M5Dial.Display.height() / 60; // Calculate based on display dims
    if (textsize == 0) {
        textsize = 1;
    }
    c.setTextSize(textsize); // Set text size for any potential text (though none here now)

    // Draw the compass background
    c.fillScreen(TFT_BLACK); // Clear the canvas first
    c.fillCircle(centerX, centerY, R, TFT_BLUE);
    c.fillCircle(centerX, centerY, R - 15, TFT_WHITE);

    // Draw static compass points (triangles) - These are fixed relative to the display
    // North (Top)
    c.fillTriangle(centerX, 0, centerX - 5, 10, centerX + 5, 10, TFT_BLACK);
    // South (Bottom)
    c.fillTriangle(centerX, M5Dial.Display.height(), centerX - 5, M5Dial.Display.height() - 10, centerX + 5, M5Dial.Display.height() - 10, TFT_BLACK);
    // West (Left)
    c.fillTriangle(0, centerY, 10, centerY - 5, 10, centerY + 5, TFT_BLACK);
    // East (Right)
    c.fillTriangle(M5Dial.Display.width(), centerY, M5Dial.Display.width() - 10, centerY - 5, M5Dial.Display.width() - 10, centerY + 5, TFT_BLACK);

    // Draw center circles
    c.fillCircle(centerX, centerY, 5, TFT_BLACK);
    c.fillCircle(centerX, centerY, 2, TFT_WHITE);
}


void drawCompassLabels(M5Canvas& canvas, double heading_rad, int centerX, int centerY, int R) {
    // Set text size based on display height
    int textsize = M5Dial.Display.height() / 60;
    if (textsize == 0) {
        textsize = 1;
    }
    canvas.setTextSize(textsize);
    canvas.setTextDatum(MC_DATUM);   // Middle center alignment
    canvas.setTextColor(TFT_BLACK, TFT_WHITE); // Black text on white background

    int R_letters = R - 25; // Radius for letters, slightly inside the white circle

    // Calculate positions for N, E, S, W based on the current heading
    // Angles are relative to North (0 rad), adjusted by the current heading
    // We use the helper function drawLetterInternal now.
    const char* labels[] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; ++i) {
        double angle_rad = (i * 90.0 * M_PI / 180.0) - heading_rad;
        // Calculate positions using sin for x and -cos for y (screen coordinates)
        int x_pos = centerX + R_letters * sin(angle_rad);
        int y_pos = centerY - R_letters * cos(angle_rad);
        drawLetterInternal(canvas, x_pos, y_pos, labels[i]);
    }
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

    if (gps.location.isValid()) { // Show coordinates even if slightly old
        String lat_str = "Lat: " + String(const_cast<TinyGPSLocation&>(gps.location).lat(), 4);
        String lng_str = "Lng: " + String(const_cast<TinyGPSLocation&>(gps.location).lng(), 4);

        canvas.setTextColor(TFT_BLACK, TFT_WHITE);
        canvas.drawString(lat_str, centerX, centerY - 10);
        canvas.drawString(lng_str, centerX, centerY + 10);
    } else {
        // Show "No GPS" if not valid (on canvas)
        canvas.setTextColor(TFT_RED, TFT_WHITE);
        canvas.drawString("No GPS Fix", centerX , centerY - 50);
    }
}

void drawStatusMessage(M5Canvas& canvas, const char* message, int centerX, int yPos, uint16_t color, uint16_t bgColor) {
    canvas.setTextColor(color, bgColor);
    canvas.setTextSize(1);
    canvas.setTextDatum(BC_DATUM); // Bottom Center
    canvas.drawString(message, centerX, yPos);
}