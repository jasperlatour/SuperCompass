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
    // Convert angle to radians for math functions. Adjust by -90 deg because 0 deg is East in standard math, but we want 0 deg North.
    double arrowAngleRad = (arrowAngleDeg - 90.0) * M_PI / 180.0;
    int arrowLength = R - 20; // Length of the arrow line

    int endX = centerX + arrowLength * cos(arrowAngleRad);
    int endY = centerY + arrowLength * sin(arrowAngleRad);

    // Draw the main line of the arrow
    canvas.drawLine(centerX, centerY, endX, endY, TFT_MAGENTA);

    // Draw the arrowhead (optional, simple triangle)
    int arrowHeadSize = 8;
    double angle1 = arrowAngleRad + M_PI - 0.4; // Angle for one side of arrowhead
    double angle2 = arrowAngleRad + M_PI + 0.4; // Angle for other side
    int x1 = endX + arrowHeadSize * cos(angle1);
    int y1 = endY + arrowHeadSize * sin(angle1);
    int x2 = endX + arrowHeadSize * cos(angle2);
    int y2 = endY + arrowHeadSize * sin(angle2);

    canvas.fillTriangle(endX, endY, x1, y1, x2, y2, TFT_MAGENTA);
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
        canvas.drawString("No GPS Fix", centerX, centerY);
    }
}

void drawStatusMessage(M5Canvas& canvas, const char* message, int centerX, int yPos, uint16_t color, uint16_t bgColor) {
    canvas.setTextColor(color, bgColor);
    canvas.setTextSize(1);
    canvas.setTextDatum(BC_DATUM); // Bottom Center
    canvas.drawString(message, centerX, yPos);
}