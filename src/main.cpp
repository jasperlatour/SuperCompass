#include "M5Dial.h"
#include <math.h>          // For cos(), sin(), atan2()
#include <MechaQMC5883.h>  // For the QMC5883 compass sensor

#ifndef PI
    #define PI 3.14159265
#endif

// Initialize the main canvas and background canvas
M5Canvas canvas(&M5Dial.Display);
M5Canvas backgroundCanvas(&M5Dial.Display);

MechaQMC5883 qmc;
double a;
int x, y, z;

// Global variables for compass center and radius
int centerX, centerY, R;

// Calibration constants (replace these with your actual values)
const float offset_x = 345.0;
const float offset_y = -196.0;
const float scale_x = 0.996644295;
const float scale_y = 1.003378378;

// Function to draw the static compass background on a canvas
void drawCompassBackgroundToCanvas(M5Canvas* c) {
    // Set text size based on display height
    int textsize = M5.Lcd.height() / 60;
    if (textsize == 0) {
        textsize = 1;
    }
    c->setTextSize(textsize);

    // Define center and radius for the compass
    centerX = M5.Lcd.width() / 2;
    centerY = M5.Lcd.height() / 2;
    R = M5.Lcd.height() / 2;

    // Draw the compass background
    c->fillScreen(TFT_BLACK);
    c->fillCircle(centerX, centerY, R, TFT_BLUE);
    c->fillCircle(centerX, centerY, R - 10, TFT_WHITE);

    // Draw static compass points (triangles)
    // North
    c->fillTriangle(centerX, 0, centerX - 5, 10, centerX + 5, 10, TFT_BLACK);
    // South
    c->fillTriangle(centerX, M5.Lcd.height(), centerX - 5, M5.Lcd.height() - 10, centerX + 5, M5.Lcd.height() - 10, TFT_BLACK);
    // West
    c->fillTriangle(0, centerY, 10, centerY - 5, 10, centerY + 5, TFT_BLACK);
    // East
    c->fillTriangle(M5.Lcd.width(), centerY, M5.Lcd.width() - 10, centerY - 5, M5.Lcd.width() - 10, centerY + 5, TFT_BLACK);

    // Draw center circles
    c->fillCircle(centerX, centerY, 5, TFT_BLACK);
    c->fillCircle(centerX, centerY, 2, TFT_WHITE);
}

// Function to draw a letter at a specified position
void drawLetter(int x, int y, const char* letter) {
    canvas.setCursor(x, y);
    canvas.print(letter);
}

// Function to draw the dynamic compass labels (N, E, S, W)
void drawCompass(double heading_rad) {
    // Set text size based on display height
    int textsize = M5.Lcd.height() / 60;
    if (textsize == 0) {
        textsize = 1;
    }
    canvas.setTextSize(textsize);
    canvas.setTextDatum(BC_DATUM);  // Bottom center

    int R_letters = R - 15;  // Adjust as needed

    // Calculate positions for N, E, S, W based on corrected angles
    double angle_N = (0 * PI / 180.0) + heading_rad;
    double angle_E = (90 * PI / 180.0) + heading_rad;
    double angle_S = (180 * PI / 180.0) + heading_rad;
    double angle_W = (270 * PI / 180.0) + heading_rad;

    // Adjust angles to ensure continuity
    angle_N = fmod(angle_N, 2 * PI);
    angle_E = fmod(angle_E, 2 * PI);
    angle_S = fmod(angle_S, 2 * PI);
    angle_W = fmod(angle_W, 2 * PI);

    // Calculate positions using cos for x and sin for y
    int x_N = centerX + R_letters * sin(angle_N);
    int y_N = centerY - R_letters * cos(angle_N);

    int x_E = centerX + R_letters * sin(angle_E);
    int y_E = centerY - R_letters * cos(angle_E);

    int x_S = centerX + R_letters * sin(angle_S);
    int y_S = centerY - R_letters * cos(angle_S);

    int x_W = centerX + R_letters * sin(angle_W);
    int y_W = centerY - R_letters * cos(angle_W);

    // Set text color with transparent background
    canvas.setTextColor(TFT_BLACK, TFT_TRANSPARENT);

    // Draw letters without rotation
    drawLetter(x_N, y_N, "N");
    drawLetter(x_E, y_E, "E");
    drawLetter(x_S, y_S, "S");
    drawLetter(x_W, y_W, "W");
}

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5Dial.Display.setBrightness(24);

    // Create the main canvas and background canvas
    canvas.createSprite(240, 240);
    backgroundCanvas.createSprite(240, 240);

    // Initialize the compass sensor
    Wire.begin();
    qmc.init();

    // Draw the compass background on the background canvas
    drawCompassBackgroundToCanvas(&backgroundCanvas);

    // Push the initial background to the display
    backgroundCanvas.pushSprite(0, 0);
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
    double heading = atan2(y_calibrated, x_calibrated) * (180.0 / PI);
    if (heading < 0) {
        heading += 360.0;
    }

    // Convert heading to radians for drawing
    double heading_rad = heading * PI / 180.0;

    // Clear the main canvas
    canvas.fillSprite(TFT_TRANSPARENT);

    // Copy the background canvas to the main canvas
    backgroundCanvas.pushSprite(&canvas, 0, 0);

    // Draw the compass labels on the canvas
    drawCompass(-heading_rad);  // Negative to rotate correctly

    // Push the updated canvas to the display
    canvas.pushSprite(0, 0);

    // Optional: Add a delay to control update rate
    delay(50);  // Adjust as needed
}
