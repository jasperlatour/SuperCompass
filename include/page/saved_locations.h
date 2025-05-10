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

extern std::vector<SavedLocation> savedLocations;
extern int numActualSavedLocations;   

// Functions for persistence
void loadSavedLocations(); // Call this in setup()
void saveSavedLocations(); // Call this after any modification

void initSavedLocationsMenu();
void handleSavedLocationsInput();
void drawSavedLocationsMenu(M5Canvas &canvas, int centerX, int centerY);



#endif // saved_locations_h