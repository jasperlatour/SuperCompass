#include "M5Dial.h"
#include <math.h>          // For cos(), sin(), atan2()
#include <MechaQMC5883.h>  // For the QMC5883 compass sensor

#ifndef PI
    #define PI 3.14159265
#endif

MechaQMC5883 qmc;
double a;
int x, y, z;

// Global variables for compass center and radius
int centerX, centerY, R;

// Variables to store the previous needle coordinates
int prevNeedleX = 0;
int prevNeedleY = 0;

// Calibration constants (replace these with your actual values)
const float offset_x = 345.0;
const float offset_y = -196.0;
const float scale_x = 0.996644295;
const float scale_y = 1.003378378;

void drawCompassBackground() {
    // Set text size based on display height
    int textsize = M5Dial.Display.height() / 60;
    if (textsize == 0) {
        textsize = 1;
    }
    M5Dial.Display.setTextSize(textsize);

    // Define center and radius for the compass
    centerX = M5Dial.Display.width() / 2;
    centerY = M5Dial.Display.height() / 2;
    R = M5Dial.Display.height() / 2;

    // Draw the compass background
    M5Dial.Display.fillScreen(TFT_BLACK);
    M5Dial.Display.fillCircle(centerX, centerY, R, TFT_BLUE);
    M5Dial.Display.fillCircle(centerX, centerY, R - 10, TFT_WHITE);

    // Draw static compass points (triangles)
    M5Dial.Display.fillTriangle(centerX, 0, centerX - 5, 10, centerX + 5, 10, TFT_BLACK);  // North
    M5Dial.Display.fillTriangle(centerX, M5Dial.Display.height(), centerX - 5, M5Dial.Display.height() - 10, centerX + 5, M5Dial.Display.height() - 10, TFT_BLACK);  // South
    M5Dial.Display.fillTriangle(0, centerY, 10, centerY - 5, 10, centerY + 5, TFT_BLACK);  // West
    M5Dial.Display.fillTriangle(M5Dial.Display.width(), centerY, M5Dial.Display.width() - 10, centerY - 5, M5Dial.Display.width() - 10, centerY + 5, TFT_BLACK);  // East
    M5Dial.Display.fillCircle(centerX, centerY, 5, TFT_BLACK);
    M5Dial.Display.fillCircle(centerX, centerY, 2, TFT_WHITE);
}

void drawCompass(double heading_rad) {
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

    // Draw the compass background
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


    // Corrected base angles for compass labels
    double angle_N = 0 * PI / 180.0 + heading_rad;
    double angle_E = 90 * PI / 180.0 + heading_rad;
    double angle_S = 180 * PI / 180.0 + heading_rad;
    double angle_W = 270 * PI / 180.0 + heading_rad;
    // Calculate positions for N, E, S, W based on heading
   
    int x_N = centerX + R_letters * cos(angle_N);
    int y_N = centerY + R_letters * sin(angle_N);

    int x_E = centerX + R_letters * cos(angle_E);
    int y_E = centerY + R_letters * sin(angle_E);
   
    int x_S = centerX + R_letters * cos(angle_S);
    int y_S = centerY + R_letters * sin(angle_S);
 
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

    // Draw the static compass background
    drawCompassBackground();
}

void loop() {
    // Read the compass sensor values
    qmc.read(&x, &y, &z);

    // Apply hard-iron offsets
    float x_corrected = x - offset_x;
    float y_corrected = y - offset_y;

    // Apply soft-iron scaling
    float x_calibrated = x_corrected * scale_x;
    float y_calibrated = y_corrected * scale_y;

    // Calculate the heading angle in degrees using calibrated values
    double heading = atan2(x_calibrated, y_calibrated) * (180.0 / PI) - 180;
    if (heading < 0) {
        heading += 360.0;
    }

    // Update 'a' if it's used elsewhere
    a = heading;

    // Convert heading to radians for drawing
    double heading_rad = heading * PI / 180.0;

    // Draw the rotating compass labels
    drawCompass(heading_rad);

    // Calculate the end point of the needle
    int needleLength = R - 10;  // Adjust as needed
    int needleX = centerX + needleLength * cos(heading_rad);
    int needleY = centerY + needleLength * sin(heading_rad);

    // Erase the previous needle by drawing over it
    if (prevNeedleX != 0 && prevNeedleY != 0) {
        M5Dial.Display.drawLine(centerX, centerY, prevNeedleX, prevNeedleY, TFT_WHITE);
    }

    // Draw the new needle
    M5Dial.Display.drawLine(centerX, centerY, needleX, needleY, TFT_RED);

    // Save the current needle position for the next loop
    prevNeedleX = needleX;
    prevNeedleY = needleY;

    // Optionally, display the heading value in degrees
    M5Dial.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextDatum(TC_DATUM);  // Top Center
    M5Dial.Display.fillRect(centerX - 50, centerY + R - 30, 100, 20, TFT_WHITE);  // Clear previous text
    M5Dial.Display.drawString("Heading: " + String(heading, 1) + "°", centerX, centerY + R - 30);

    
}
