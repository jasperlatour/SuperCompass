// filepath: c:\Users\latou\Git Repo's Pesronal\SuperCompass\include\menu.h
#ifndef MENU_H
#define MENU_H

#include "globals_and_includes.h"



typedef struct {
    const char* name;
    void (*action)();
    const tImage* icon;
} MenuItem;


// Externe declaraties van menuvariabelen
extern MenuItem menuItems[];
extern int numMenuItems;
extern int selectedMenuItemIndex;
extern bool menuActive;

// Functies voor menu beheer
void initMenu();
void handleMenuInput();
void drawAppMenu(M5Canvas &canvas, int centerX, int centerY, int radius, int arrowSize);


// Actie functies die door menu-items worden gebruikt
void action_startNavigation();
void action_showSettings();
void action_showGpsInfo();


#endif // MENU_H