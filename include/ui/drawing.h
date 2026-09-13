// drawing.h
#ifndef DRAWING_H
#define DRAWING_H

#include "globals_and_includes.h" 

// Function declarations

/**
 * @brief Draws the compass background (outer rim, ticks, cardinal arrows) rotated by heading.
 *        The red (north) arrow will always point toward true north while the dial rotates.
 * @param c Reference to the M5Canvas to draw on.
 * @param centerX The x-coordinate of the canvas center.
 * @param centerY The y-coordinate of the canvas center.
 * @param R The radius of the main compass circle.
 * @param heading_rad Current heading in radians (0 = facing north / up).
 */
void drawCompassBackgroundToCanvas(M5Canvas& c, int centerX, int centerY, int R, double heading_rad);

/**
 * @brief Draws the dynamic compass labels (N, E, S, W) rotating with heading.
 * @param canvas Reference to the M5Canvas to draw on.
 * @param heading_rad The current true heading in radians.
 * @param centerX The x-coordinate of the canvas center.
 * @param centerY The y-coordinate of the canvas center.
 * @param R The radius of the main compass circle.
 */
void drawCompassLabels(M5Canvas& canvas, double heading_rad, int centerX, int centerY, int R);

/**
 * @brief Draws a fixed (non-rotating) marker at the top of the dial, indicating the
 *        direction the device currently faces (a "lubber line"), independent of heading.
 * @param c Reference to the M5Canvas to draw on.
 * @param centerX The x-coordinate of the canvas center.
 * @param centerY The y-coordinate of the canvas center.
 * @param R The radius of the main compass circle.
 */
void drawFixedHeadingMarker(M5Canvas& c, int centerX, int centerY, int R);

/**
 * @brief Draws the large numeric heading readout (degrees + cardinal letter) at the dial center.
 * @param c Reference to the M5Canvas to draw on.
 * @param heading_deg The current true heading in degrees (0-360).
 * @param centerX The x-coordinate of the canvas center.
 * @param centerY The y-coordinate of the canvas center.
 */
void drawHeadingValue(M5Canvas& c, double heading_deg, int centerX, int centerY);

/**
 * @brief Draws the arrow pointing towards the target location.
 * @param canvas Reference to the M5Canvas to draw on.
 * @param arrowAngleDeg The angle (0-360 deg) relative to the top of the screen where the arrow should point.
 * @param centerX The x-coordinate of the canvas center.
 * @param centerY The y-coordinate of the canvas center.
 * @param R The radius of the main compass circle.
 */
void drawTargetArrow(M5Canvas& canvas, double arrowAngleDeg, int centerX, int centerY, int R);

/**
 * @brief Displays GPS information (coordinates or status) on the canvas.
 * @param canvas Reference to the M5Canvas to draw on.
 * @param gps Reference to the TinyGPSPlus object containing GPS data.
 * @param centerX The x-coordinate of the canvas center.
 * @param centerY The y-coordinate of the canvas center.
 */
void drawGpsInfo(M5Canvas& canvas, const TinyGPSPlus& gps, int centerX, int centerY);

/**
 * @brief Displays a status message on the canvas.
 * @param canvas Reference to the M5Canvas to draw on.
 * @param message The text message to display.
 * @param centerX The x-coordinate of the canvas center.
 * @param yPos The y-coordinate for the message position (datum is usually BC_DATUM).
 * @param color The color of the text (default TFT_RED).
 * @param bgColor The background color for the text (default TFT_WHITE).
 */
void drawStatusMessage(M5Canvas& canvas, const char* message, int centerX, int yPos, uint16_t color = TFT_RED, uint16_t bgColor = TFT_WHITE);

/**
 * @brief Draws the target status banner below the dial center: current target name and
 *        distance when a target and fix are available, or a neutral placeholder otherwise.
 * @param canvas Reference to the M5Canvas to draw on.
 * @param centerX The x-coordinate of the canvas center.
 * @param centerY The y-coordinate of the canvas center.
 * @param R The radius of the main compass circle.
 * @param targetSet Whether a navigation target is currently set.
 * @param locationValid Whether a current position (GPS or BLE) is available.
 * @param targetName The name of the current target.
 * @param distanceMeters The great-circle distance to the target, in meters.
 */
void drawTargetInfoBanner(M5Canvas& canvas, int centerX, int centerY, int R,
                           bool targetSet, bool locationValid,
                           const String& targetName, double distanceMeters);

/**
 * @brief Shows a temporary popup notification on the screen.
 * @param message The text message to display in the popup.
 * @param durationMs The duration in milliseconds to show the popup (default 2000ms).
 * @param color The color of the text (default TFT_WHITE).
 * @param bgColor The background color for the popup (default TFT_BLUE).
 */
void showPopupNotification(const char* message, uint32_t durationMs = 2000, uint16_t color = TFT_WHITE, uint16_t bgColor = TFT_BLUE);

/**
 * @brief If an active popup exists, draw it onto the provided canvas (no pushSprite inside).
 *        Automatically clears the popup state after its duration.
 * @param canvas Reference to the M5Canvas to draw on.
 */
void drawPopupIfActive(M5Canvas& canvas);

#endif // DRAWING_H