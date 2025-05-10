#ifndef ICONS_H
#define ICONS_H

#include <stdint.h>

// Structure for 16-bit color images
typedef struct {
    void *data;          // Changed from uint16_t* to void*
    uint16_t width;
    uint16_t height;
    uint8_t depth;
} tImage;


extern const tImage icon_temp;
extern const tImage icon_more;
extern const tImage icon_wifi;


#endif // ICONS_H