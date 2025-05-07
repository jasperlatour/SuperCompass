#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

#include <Arduino.h> // For String, PROGMEM attributes

// Main HTML page for setting target
const char HTML_PAGE_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>M5Dial Target Setter</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; color: #333; }
        .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); max-width: 500px; margin: auto; }
        h1 { color: #007bff; text-align: center; }
        h2 { margin-top: 30px; border-bottom: 1px solid #eee; padding-bottom: 5px;}
        label { display: block; margin-top: 15px; margin-bottom: 5px; font-weight: bold; }
        input[type="text"], input[type="number"] {
            width: calc(100% - 22px); padding: 10px; margin-bottom: 10px; border: 1px solid #ddd;
            border-radius: 4px; box-sizing: border-box;
        }
        input[type="submit"] {
            background-color: #007bff; color: white; padding: 12px 20px; border: none;
            border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%; margin-top: 10px;
        }
        input[type="submit"]:hover { background-color: #0056b3; }
        .current-target { margin-top:20px; margin-bottom:20px; padding:10px; background-color:#e9ecef; border-radius:4px; }
        hr { margin-top: 30px; margin-bottom: 20px; border: 0; border-top: 1px solid #eee; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Set Navigation Target</h1>
        <div class="current-target">
            <strong>Current Target:</strong><br>
            Lat: <span id="current_lat_display">%.6f</span><br>
            Lon: <span id="current_lon_display">%.6f</span>
        </div>
        <h2>Option 1: Set by Coordinates</h2>
        <form action="/settarget" method="POST">
            <label for="lat">Target Latitude:</label>
            <input type="number" step="any" id="lat" name="latitude" placeholder="e.g., 40.7128" required>
            <label for="lon">Target Longitude:</label>
            <input type="number" step="any" id="lon" name="longitude" placeholder="e.g., -74.0060" required>
            <input type="submit" value="Set Target by Lat/Lon">
        </form>
        <hr>
        <h2>Option 2: Set by Address</h2>
        <form action="/settargetbyaddress" method="POST">
            <label for="address">Target Address:</label>
            <input type="text" id="address" name="address" placeholder="e.g., Eiffeltoren, Parijs" required>
            <input type="submit" value="Search Address & Set Target">
        </form>
        <div id="message-area"></div>
    </div>
</body>
</html>
)rawliteral";

// HTML template for response/status pages
const char HTML_RESPONSE_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>%s</title>
<meta http-equiv="refresh" content="%d;url=/" />
<style>
    body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }
    .message { padding: 15px; border-radius: 5px; margin: 20px auto; max-width: 500px; word-wrap: break-word; }
    .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
    .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
    a { color: #007bff; text-decoration: none; }
</style></head><body>
    <div class="message %s">
        <h1>%s</h1>
        <p>%s</p>
        <p>Redirecting back in %d seconds... or <a href="/">click here</a>.</p>
    </div>
</body></html>
)rawliteral";

#endif // HTML_CONTENT_H