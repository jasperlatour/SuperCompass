#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include "globals_and_includes.h" // For WebServer, String, TARGET_LAT/LON, etc.
#include "html_content.h"         // For HTML_PAGE_TEMPLATE, HTML_RESPONSE_TEMPLATE

// Function to send a formatted HTML response page
void sendDynamicResponsePage(const String& title, const String& statusType, const String& headerMsg, const String& bodyMsg, int refreshDelay = 3);

// Web request handlers
void handleRoot();
void handleSetTarget();
void handleSetTargetByAddress();
void handleNotFound();

// Function to set up all server routes
void setupServerRoutes();

#endif // WEB_HANDLERS_H