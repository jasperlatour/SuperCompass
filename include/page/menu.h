// filepath: c:\Users\latou\Git Repo's Pesronal\SuperCompass\include\menu.h
#ifndef MENU_H
#define MENU_H

#include "globals_and_includes.h"
#include "saved_locations.h"
#include "page/settings.h"



typedef struct {
    const char* name;
    void (*action)();
    const tImage* icon;
} MenuItem;


// Externe declaraties van menuvariabelen
extern MenuItem menuItems[];
extern int numMenuItems;
extern int selectedMenuItemIndex;
extern bool settingsMenuActive;
extern bool soundEnabled;
extern bool touchEnabled;


// Functies voor menu beheer
void initMenu();
void handleMenuInput();
void drawAppMenu(M5Canvas &canvas, int centerX, int centerY, int radius, int arrowSize);


// Actie functies die door menu-items worden gebruikt
void action_startNavigation();
void action_showSettings();
void action_showGpsInfo();
void action_showSavedLocations();
void action_showBluetoothInfo();



#endif // MENU_H