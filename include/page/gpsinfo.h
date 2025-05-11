#ifndef GPS_INFO_H
#define GPS_INFO_H

#include <M5GFX.h> 
#include "globals_and_includes.h"

 

// Setters
void setLatitude(double lat);
void setLongitude(double lon);
void setAltitude(double alt);
void setSpeed(double spd);
void setSatellitesInView(int count);
void setFixQuality(int quality);

// Getters
double getLatitude() ;
double getLongitude() ;
double getAltitude() ;
double getSpeed() ;
int getSatellitesInView() ;
int getFixQuality() ;

// Utility
bool hasFix() ;

void drawGpsInfoPage(M5Canvas &canvas, int centerX, int centerY); 
void handleGpsInfoInput(); 

extern double latitude_;
extern double longitude_;
extern double altitude_;
extern double speed_;
extern int satellitesInView_;
extern int fixQuality_;




#endif // GPS_INFO_H