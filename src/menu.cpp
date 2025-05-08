// filepath: c:\Users\latou\Git Repo's Pesronal\SuperCompass\src\menu.cpp
#include "menu.h"
#include "drawing.h"
#include "icons.h"
#include "bt.h" // Include bt.h

// Define the global variables that were declared as extern in menu.h
bool menuActive = false; // Or true, depending on your desired initial state

int selectedMenuItemIndex = 0;

// Define the actions for the menu-items
void action_startNavigation() { // Definitie
    Serial.println("Action: Start Navigation selected");
    menuActive = false;
    // ...
}

void action_showSettings() { // Definitie
    Serial.println("Action: Show Settings selected");
    // ...
}

void action_showGpsInfo() { // Definitie
    Serial.println("Action: Show GPS Info selected");
    // ...
}

// Define your menu-items WITH RawIconData pointers
#define NUM_ITEMS 3

// Forward declarations of the icons
extern const uint16_t image_data_icon_ble[1764];

// Define tImage instances for your icons
tImage icon_tracker_image = {(uint16_t*)tracker_pixel_data, 36, 36, 1}; // Assuming 1 bit depth
tImage icon_settings_image = {(uint16_t*)settings_pixel_data, 36, 36, 1}; // Assuming 1 bit depth
tImage icon_info_image = {(uint16_t*)info_pixel_data, 36, 36, 1}; // Assuming 1 bit depth
tImage icon_ble_image = {(uint16_t*)image_data_icon_ble, 42, 42, 16};

MenuItem menuItems[NUM_ITEMS] = {
    {"Navigate", action_startNavigation, &icon_tracker_image},
    {"Settings", action_showSettings,    &icon_settings_image},
    {"GPS Info", action_showGpsInfo,     &icon_info_image}
};

void drawAppMenu(M5Canvas &canvas, int centerX, int centerY, int radius, int arrowSize) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextDatum(MC_DATUM);

    // Calculate the angle step based on the number of menu items
    float angle_step_degrees = 360.0f / numMenuItems;

    // Define the base angle for the selected item (e.g., at the top)
    const float selected_angle_degrees = 90.0f;

    int arrowX = centerX + cos(selected_angle_degrees * DEG_TO_RAD) * (radius + 10);
    int arrowY = centerY - sin(selected_angle_degrees * DEG_TO_RAD) * (radius + 10);
    canvas.drawXBitmap(arrowX - arrowSize / 2, arrowY - arrowSize / 2, arrow_32x32_map, arrowSize, arrowSize, TFT_WHITE);

    for (int i = 0; i < numMenuItems; ++i) {
        // Calculate the angle for the current item relative to the selected item
        float current_item_display_angle_deg = selected_angle_degrees - (i - selectedMenuItemIndex) * angle_step_degrees;

        // Adjust the angle to be within 0-360 degrees
        current_item_display_angle_deg = fmod(current_item_display_angle_deg, 360.0f);
        if (current_item_display_angle_deg < 0) {
            current_item_display_angle_deg += 360.0f;
        }

        float rad = current_item_display_angle_deg * DEG_TO_RAD;
        int item_base_x = centerX + cos(rad) * radius;
        int item_base_y = centerY - sin(rad) * radius;

        const tImage* icon = menuItems[i].icon; // Nu tImage*
        uint16_t icon_color = TFT_DARKGREY;
        int textYOffset = 0;

        if (icon) {
            textYOffset = (icon->height / 2) + 2;
        }

        // Determine if the current item is the selected item
        bool is_selected = (i == selectedMenuItemIndex);

        if (is_selected) { // Geselecteerd item
            icon_color = TFT_WHITE;
            canvas.setTextSize(1);
            canvas.setTextColor(TFT_WHITE);
        } else { // Niet-geselecteerde items
            canvas.setTextSize(1);
            canvas.setTextColor(TFT_LIGHTGREY);
        }

        if (icon) {
            // Use drawBitmap for tImage
            canvas.drawBitmap(item_base_x - icon->width / 2,
                               item_base_y - icon->height / 2,
                               icon->width, icon->height,
                               icon->data);
        }

        canvas.setTextColor(is_selected ? TFT_WHITE : TFT_LIGHTGREY);
        canvas.drawString(menuItems[i].name, item_base_x, item_base_y + textYOffset);
    }
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
}

// Implementations for initMenu() and handleMenuInput()
void initMenu() {
    // Initialize menu state (e.g., set selectedMenuItemIndex to 0)
    selectedMenuItemIndex = 0;
    Serial.println("Menu Initialized");
}

void handleMenuInput() {
    // Read the encoder value
    int encoderValue = M5Dial.Encoder.read();

    // Adjust selectedMenuItemIndex based on encoder movement
    if (encoderValue > 0) {
        selectedMenuItemIndex++;
        if (selectedMenuItemIndex >= numMenuItems) {
            selectedMenuItemIndex = 0; // Wrap around
        }
        M5Dial.Encoder.write(0); // Reset encoder value
    } else if (encoderValue < 0) {
        selectedMenuItemIndex--;
        if (selectedMenuItemIndex < 0) {
            selectedMenuItemIndex = numMenuItems - 1; // Wrap around
        }
        M5Dial.Encoder.write(0); // Reset encoder value
    }

    // Check for button press to execute the selected action
    if (M5.BtnA.wasPressed()) {
        // Get the action function pointer for the selected menu item
        void (*selectedAction)() = menuItems[selectedMenuItemIndex].action;
        // Execute the action if it's valid
        if (selectedAction != nullptr) {
            selectedAction(); // Call the action function
        } 
    }
}