#include "M5Dial.h"
#include <math.h>          // For cos(), sin()
#include <MechaQMC5883.h>  // For the QMC5883 compass sensor

#ifndef PI
#define PI 3.14159265
#endif

MechaQMC5883 qmc;
int x, y, z;

// Global variables for compass center and radius
int centerX, centerY, R;

void drawCompass() {
    // Set text size based on display height
    int textsize = M5Dial.Display.height() / 60;
    if (textsize == 0) {
        textsize = 1;
    }
    M5Dial.Display.setTextSize(textsize);

    // Calculate letter dimensions
    int letterWidth = textsize * 6;
    int letterHeight = textsize * 8;

    // Define center and radius for the compass
    centerX = M5Dial.Display.width() / 2;
    centerY = M5Dial.Display.height() / 2;
    R = M5Dial.Display.height() / 2;
    int R_letters = R - 15;  // Adjust as needed

    // Draw the compass
    M5Dial.Display.fillScreen(TFT_BLACK);
    M5Dial.Display.fillCircle(centerX, centerY, R, TFT_BLUE);
    M5Dial.Display.fillCircle(centerX, centerY, R - 10, TFT_WHITE);

    // Draw compass points (triangles)
    M5Dial.Display.fillTriangle(centerX, 0, centerX - 5, 10, centerX + 5, 10, TFT_BLACK);  // North
    M5Dial.Display.fillTriangle(centerX, M5Dial.Display.height(), centerX - 5, M5Dial.Display.height() - 10, centerX + 5, M5Dial.Display.height() - 10, TFT_BLACK);  // South
    M5Dial.Display.fillTriangle(0, centerY, 10, centerY - 5, 10, centerY + 5, TFT_BLACK);  // West
    M5Dial.Display.fillTriangle(M5Dial.Display.width(), centerY, M5Dial.Display.width() - 10, centerY - 5, M5Dial.Display.width() - 10, centerY + 5, TFT_BLACK);  // East
    M5Dial.Display.fillCircle(centerX, centerY, 5, TFT_BLACK);
    M5Dial.Display.fillCircle(centerX, centerY, 2, TFT_WHITE);

    // Set text color
    M5Dial.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // Calculate positions for N, E, S, W
    double angle_N = -90 * PI / 180.0;
    int x_N = centerX + R_letters * cos(angle_N);
    int y_N = centerY + R_letters * sin(angle_N);

    double angle_E = 0 * PI / 180.0;
    int x_E = centerX + R_letters * cos(angle_E);
    int y_E = centerY + R_letters * sin(angle_E);

    double angle_S = 90 * PI / 180.0;
    int x_S = centerX + R_letters * cos(angle_S);
    int y_S = centerY + R_letters * sin(angle_S);

    double angle_W = 180 * PI / 180.0;
    int x_W = centerX + R_letters * cos(angle_W);
    int y_W = centerY + R_letters * sin(angle_W);

    // Adjust cursor positions to center the letters
    M5Dial.Display.setCursor(x_N - letterWidth / 2, y_N - letterHeight / 2);
    M5Dial.Display.print("N");

    M5Dial.Display.setCursor(x_E - letterWidth / 2, y_E - letterHeight / 2);
    M5Dial.Display.print("E");

    M5Dial.Display.setCursor(x_S - letterWidth / 2, y_S - letterHeight / 2);
    M5Dial.Display.print("S");

    M5Dial.Display.setCursor(x_W - letterWidth / 2, y_W - letterHeight / 2);
    M5Dial.Display.print("W");
}

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5Dial.begin(cfg, false, true);

    // Initialize the compass sensor
    Wire.begin();
    qmc.init();

    drawCompass();
}

void loop() {
    // Read the compass sensor values
    qmc.read(&x, &y, &z);

    // Define the area where the text will be displayed
    int textAreaWidth = 120;  // Adjust as needed
    int textAreaHeight = 60;  // Adjust as needed
    int textAreaX = centerX - textAreaWidth / 2;
    int textAreaY = centerY - textAreaHeight / 2;

    // Clear the text area by filling it with white (same as compass center)
    M5Dial.Display.fillRect(textAreaX, textAreaY, textAreaWidth, textAreaHeight, TFT_WHITE);

    // Set text color and size
    M5Dial.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5Dial.Display.setTextSize(2);  // Adjust as needed

    // Set text datum to center
    M5Dial.Display.setTextDatum(CC_DATUM);  // Center Center

    // Display the values centered in the text area
    M5Dial.Display.drawString("X: " + String(x), centerX, centerY - 20);
    M5Dial.Display.drawString("Y: " + String(y), centerX, centerY);
    M5Dial.Display.drawString("Z: " + String(z), centerX, centerY + 20);

    delay(100);  // Adjust delay as needed
}
