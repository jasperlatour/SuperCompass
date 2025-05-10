// Structure for a saved location
#ifndef saved_locations_h
#define saved_locations_h

#include "globals_and_includes.h" 

typedef struct {
    const char* name;
    double lat;
    double lon;
} SavedLocation;

extern bool savedLocationsMenuActive; 
extern int selectedLocationIndex;    


extern const int MAX_SAVED_LOCATIONS; 
extern SavedLocation savedLocations[];  
extern int numActualSavedLocations;   

// ADDED: Functions for saved locations page
void initSavedLocationsMenu();
void handleSavedLocationsInput();
void drawSavedLocationsMenu(M5Canvas &canvas, int centerX, int centerY);

#endif // saved_locations_h