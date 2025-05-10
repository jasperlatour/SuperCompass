// filepath: c:\Users\latou\Git Repo's Pesronal\SuperCompass\src\menu.cpp
#include "menu.h"
#include "drawing.h"
#include "icons.h"

// Define the global variables that were declared as extern in menu.h
bool menuActive = false; // Or true, depending on your desired initial state

int selectedMenuItemIndex = 0;
static int encoder_click_accumulator = 0;
const int ENCODER_COUNTS_PER_DETENT = 4;


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
#define NUM_ITEMS 5




MenuItem menuItems[NUM_ITEMS] = {
    {"Navigate", action_startNavigation, &icon_compass},
    {"Settings", action_showSettings,    &icon_more},
    {"GPS Info", action_showGpsInfo,     &icon_wifi},
    {"Item 4",   action_showGpsInfo,     &icon_more},
    {"Item 5",   action_showGpsInfo,     &icon_wifi}
};

void drawAppMenu(M5Canvas &canvas, int centerX, int centerY, int radius, int arrowSize) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextDatum(MC_DATUM); // Middle Center datum for text

    int placement_radius = static_cast<int>(centerX * 0.78f); 
    // Calculate the angle step based on the number of menu items
    float angle_step_degrees = 360.0f / NUM_ITEMS;

    // Define the starting angle for the first item (e.g., 90.0f for the top of the circle)
    const float start_angle_degrees = 90.0f;

    const char* selectedItemName = nullptr; // To store the name of the selected item

    for (int i = 0; i < NUM_ITEMS; ++i) {
        // Calculate the fixed angle for the current item.
        // Items will be placed clockwise starting from start_angle_degrees.
        float item_angle_deg = start_angle_degrees - (i * angle_step_degrees);

        // Normalize the angle to be within 0-360 degrees
        item_angle_deg = fmod(item_angle_deg, 360.0f);
        if (item_angle_deg < 0) {
            item_angle_deg += 360.0f;
        }

        float rad = item_angle_deg * DEG_TO_RAD; // DEG_TO_RAD = PI / 180.0f
      
        int icon_center_x = centerX + static_cast<int>(cos(rad) * placement_radius); // Icon's center X
        int icon_center_y = centerY - static_cast<int>(sin(rad) * placement_radius); // Icon's center Y (subtract sin because Y is typically downwards on screens)

        const tImage* icon = menuItems[i].icon;
        bool is_selected = (i == selectedMenuItemIndex);

        if (icon) {
            // Determine icon color for 1-bit icons
            // Selected icons are white, non-selected are light grey.
            uint16_t icon_color_for_bitmap = is_selected ? TFT_WHITE : TFT_LIGHTGREY;

            // Draw the icon
            if (icon->depth == 1) {
                canvas.drawXBitmap(
                    icon_center_x - icon->width / 2,
                    icon_center_y - icon->height / 2,
                    (const uint8_t*)icon->data,
                    icon->width, icon->height,
                    icon_color_for_bitmap
                );
            } else if (icon->depth == 16) {
                // For 16-bit icons, they have their own colors.
                // If dimming is desired for non-selected 16-bit icons, more complex handling is needed (e.g., drawing pixel by pixel with color manipulation).
                canvas.pushImage(
                    icon_center_x - icon->width / 2,
                    icon_center_y - icon->height / 2,
                    icon->width, icon->height,
                    (uint16_t*)icon->data
                );
            }

            if (is_selected) {
                // Draw a circle around the selected icon
                int selection_circle_radius = (icon->width / 2) + 4; // Adjust padding as needed
                canvas.drawCircle(icon_center_x, icon_center_y, selection_circle_radius, TFT_WHITE);

                // Store the name of the selected item
                selectedItemName = menuItems[i].name;
            }
        }
    }

    // Draw the selected item's name in the center of the screen
    if (selectedItemName) {
        canvas.setTextSize(2); // Use a larger font for the selected item's name
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString(selectedItemName, centerX, centerY);
    }

    // Reset text properties to default for other parts of your application if necessary
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK); // As per original end of function
}

// Implementations for initMenu() and handleMenuInput()
void initMenu() {
    // Initialize menu state (e.g., set selectedMenuItemIndex to 0)
    selectedMenuItemIndex = 0;
    Serial.println("Menu Initialized");
}

void handleMenuInput() {
    // Read the raw change from the encoder.
    int raw_encoder_change = M5Dial.Encoder.read();

    // Reset the encoder's internal counter if there was a change.
    // This makes raw_encoder_change effectively the delta since the last call.
    if (raw_encoder_change != 0) {
        M5Dial.Encoder.write(0);
    }

    encoder_click_accumulator += raw_encoder_change;

    // Check if enough counts have accumulated for a clockwise turn (positive)
    if (ENCODER_COUNTS_PER_DETENT > 0 && encoder_click_accumulator >= ENCODER_COUNTS_PER_DETENT) {
        int num_detents = encoder_click_accumulator / ENCODER_COUNTS_PER_DETENT;
        for (int i = 0; i < num_detents; ++i) {
            selectedMenuItemIndex++;
            if (selectedMenuItemIndex >= NUM_ITEMS) {
                selectedMenuItemIndex = 0; // Wrap around
            }
        }
        encoder_click_accumulator %= ENCODER_COUNTS_PER_DETENT; // Keep the remainder
    }
    // Check if enough counts have accumulated for a counter-clockwise turn (negative)
    else if (ENCODER_COUNTS_PER_DETENT > 0 && encoder_click_accumulator <= -ENCODER_COUNTS_PER_DETENT) {
        int num_detents = -encoder_click_accumulator / ENCODER_COUNTS_PER_DETENT; // Make positive
        for (int i = 0; i < num_detents; ++i) {
            selectedMenuItemIndex--;
            if (selectedMenuItemIndex < 0) {
                selectedMenuItemIndex = NUM_ITEMS - 1; // Wrap around
            }
        }
        // Keep the remainder, preserving sign.
        // The modulo operator behavior with negative numbers can vary, so ensure it's correct.
        // A common way:
        encoder_click_accumulator = -( -encoder_click_accumulator % ENCODER_COUNTS_PER_DETENT );

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