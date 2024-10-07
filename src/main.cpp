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
    qmc.read(&x,&y,&z);
    a = qmc.azimuth(&y,&x);

    // Calculate the heading angle in degrees
    double heading = atan2(y, x) * 180.0 / PI;

    

    // Convert heading to radians for drawing
    double heading_rad = a * PI / 180.0;

    // Calculate the end point of the needle
    int needleLength = R - 20;  // Adjust as needed
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

    delay(100);  // Adjust delay as needed
}
