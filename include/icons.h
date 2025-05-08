#ifndef ICONS_H
#define ICONS_H

#include <stdint.h>

// Structure for 16-bit color images
typedef struct {
    const uint16_t *data;   // Pointer to image data (array of uint16_t for RGB565)
    uint16_t       width;  // Image width in pixels
    uint16_t       height; // Image height in pixels
    uint8_t        dataSize;
} tImage;

// Forward declarations of the icon data (defined in icons.cpp)
extern const uint8_t tracker_pixel_data[];
extern const uint8_t settings_pixel_data[];
extern const uint8_t info_pixel_data[];

// Forward declaration for the BLE icon data
extern const uint16_t image_data_icon_ble[];

// Declare the tImage instances (defined in icons.cpp)
extern tImage icon_tracker_image;
extern tImage icon_settings_image;
extern tImage icon_info_image;

//Declare the BLE icon instance
extern tImage icon_ble_image;

#endif // ICONS_H