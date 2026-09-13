#include "page/calibration.h"
#include "page/settings.h" // saveCalibrationValues, initSettingsMenu
#include "drawing.h"       // showPopupNotification

static const uint32_t CALIBRATION_DURATION_MS = 12000;
static const int MIN_USABLE_SPAN = 20; // raw sensor counts; below this, rotation wasn't captured

static int32_t calibMinX, calibMaxX, calibMinY, calibMaxY;
static uint32_t calibStartTime = 0;

void initCompassCalibration() {
    calibMinX = INT32_MAX; calibMaxX = INT32_MIN;
    calibMinY = INT32_MAX; calibMaxY = INT32_MIN;
    calibStartTime = millis();
    Serial.println("Compass calibration started");
}

static void finishCalibration() {
    int spanX = calibMaxX - calibMinX;
    int spanY = calibMaxY - calibMinY;

    if (spanX < MIN_USABLE_SPAN || spanY < MIN_USABLE_SPAN) {
        showPopupNotification("Calibration failed, try again", 2000, TFT_WHITE, THEME_WARN);
    } else {
        int newOffsetX = (calibMinX + calibMaxX) / 2;
        int newOffsetY = (calibMinY + calibMaxY) / 2;
        // Soft-iron correction: normalize both axes to the same average span.
        float avgSpan = (spanX + spanY) / 2.0f;
        float newScaleX = avgSpan / (float)spanX;
        float newScaleY = avgSpan / (float)spanY;
        saveCalibrationValues(newOffsetX, newOffsetY, newScaleX, newScaleY);
        showPopupNotification("Calibration saved", 2000, TFT_WHITE, TFT_DARKGREEN);
    }

    compassCalibrationActive = false;
    settingsMenuActive = true;
    initSettingsMenu();
}

void handleCompassCalibrationInput() {
    int x, y, z;
    qmc.read(&x, &y, &z);
    if (x < calibMinX) calibMinX = x;
    if (x > calibMaxX) calibMaxX = x;
    if (y < calibMinY) calibMinY = y;
    if (y > calibMaxY) calibMaxY = y;

    // Long-press cancels without touching the existing calibration.
    if (M5.BtnA.pressedFor(1500)) {
        showPopupNotification("Calibration cancelled", 1500, TFT_WHITE, THEME_PANEL);
        compassCalibrationActive = false;
        settingsMenuActive = true;
        initSettingsMenu();
        return;
    }

    uint32_t elapsed = millis() - calibStartTime;
    bool timeUp = elapsed >= CALIBRATION_DURATION_MS;
    bool enoughSpanToFinishEarly = (calibMaxX - calibMinX) > 40 && (calibMaxY - calibMinY) > 40;

    if (timeUp || (M5.BtnA.wasPressed() && enoughSpanToFinishEarly)) {
        finishCalibration();
    }
}

void drawCompassCalibration(M5Canvas &canvas, int centerX, int centerY, int R) {
    canvas.fillSprite(THEME_BG);
    canvas.setTextDatum(MC_DATUM);

    // Everything below is positioned relative to centerY and kept within
    // +/-60px of it - the round panel's edge cuts off text well before its
    // nominal radius (R), so text there was unreadable. No separate title:
    // the user already knows they're calibrating, it just saved space.
    canvas.setTextSize(1);
    canvas.setTextColor(THEME_TEXT_MUTED);
    canvas.drawString("Rotate the device", centerX, centerY - 56);
    canvas.drawString("in a full circle", centerX, centerY - 42);

    uint32_t elapsed = millis() - calibStartTime;
    float progress = min(1.0f, elapsed / (float)CALIBRATION_DURATION_MS);

    // Small progress ring tight around the countdown, starting at the top
    // (matching the compass's fixed heading marker) and sweeping clockwise.
    int ringR0 = 48;
    int ringR1 = 56;
    canvas.fillArc(centerX, centerY, ringR0, ringR1, 0, 360, THEME_PANEL);
    if (progress > 0.001f) {
        canvas.fillArc(centerX, centerY, ringR0, ringR1, -90.0f, -90.0f + progress * 360.0f, THEME_ACCENT_PRIMARY);
    }

    int secondsLeft = (int)ceil((CALIBRATION_DURATION_MS - elapsed) / 1000.0f);
    if (secondsLeft < 0) secondsLeft = 0;
    char buf[8];
    snprintf(buf, sizeof(buf), "%ds", secondsLeft);
    canvas.setTextSize(4);
    canvas.setTextColor(THEME_TEXT_PRIMARY, THEME_BG);
    canvas.drawString(buf, centerX, centerY);

    canvas.setTextSize(1);
    canvas.setTextColor(THEME_TEXT_DIM, THEME_BG);
    canvas.drawString("Hold: cancel", centerX, centerY + 60);
}
