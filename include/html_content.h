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
        body { font-family: Arial, sans-serif; margin: 0; background-color: #f4f4f4; color: #333; }
        nav { text-align: center; padding: 10px 0; background-color: #e9ecef; border-bottom: 1px solid #ddd; margin-bottom: 20px; }
        nav a { margin: 0 15px; text-decoration: none; color: #007bff; font-weight: bold; }
        nav a:hover { color: #0056b3; text-decoration: underline; }
        .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); max-width: 500px; margin: auto; }
        h1 { color: #007bff; text-align: center; margin-top: 0; /* Adjusted for nav */ }
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
    <nav>
        <a href="/">Set Target</a> |
        <a href="/manage-locations">Manage Locations</a>
    </nav>
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
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta http-equiv="refresh" content="%d;url=/" />
<style>
    body { font-family: Arial, sans-serif; margin: 0; text-align: center; background-color: #f4f4f4; }
    nav { text-align: center; padding: 10px 0; background-color: #e9ecef; border-bottom: 1px solid #ddd; margin-bottom: 20px; }
    nav a { margin: 0 15px; text-decoration: none; color: #007bff; font-weight: bold; }
    nav a:hover { color: #0056b3; text-decoration: underline; }
    .message { padding: 15px; border-radius: 5px; margin: 20px auto; max-width: 500px; word-wrap: break-word; background-color: #fff; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
    .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
    .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
    a { color: #007bff; text-decoration: none; }
    .message h1 { margin-top: 0; }
</style></head><body>
    <nav>
        <a href="/">Set Target</a> |
        <a href="/manage-locations">Manage Locations</a>
    </nav>
    <div class="message %s">
        <h1>%s</h1>
        <p>%s</p>
        <p>Redirecting back in %d seconds... or <a href="/">click here</a>.</p>
    </div>
</body></html>
)rawliteral";

const char HTML_MANAGE_LOCATIONS[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Manage Locations</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 0; background-color: #f4f4f4; color: #333; }
        nav { text-align: center; padding: 10px 0; background-color: #e9ecef; border-bottom: 1px solid #ddd; margin-bottom: 20px; }
        nav a { margin: 0 15px; text-decoration: none; color: #007bff; font-weight: bold; }
        nav a:hover { color: #0056b3; text-decoration: underline; }
        .content-wrapper { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); max-width: 700px; margin: auto; }
        .content-wrapper h2, .content-wrapper h3 { color: #0056b3; margin-top: 0; /* Adjusted for nav/wrapper */ }
        .content-wrapper h3 { margin-top: 20px; border-bottom: 1px solid #eee; padding-bottom: 5px; margin-bottom: 15px;}
        #locationsList div { background-color: #f9f9f9; border: 1px solid #ddd; padding: 10px; margin-bottom: 10px; border-radius: 4px; display: flex; justify-content: space-between; align-items: center; }
        #locationsList button { background-color: #d9534f; color: white; border: none; padding: 5px 10px; border-radius: 4px; cursor: pointer; }
        #locationsList button:hover { background-color: #c9302c; }
        #searchResults div { background-color: #e9f7ef; border: 1px solid #d1e7dd; padding: 10px; margin-top:10px; margin-bottom: 10px; border-radius: 4px; }
        #searchResults button { background-color: #28a745; color: white; border: none; padding: 5px 10px; border-radius: 4px; cursor: pointer; margin-top: 5px; }
        #searchResults button:hover { background-color: #218838; }
        form { background-color: #fff; padding: 15px; border-radius: 4px; border: 1px solid #ddd; margin-top: 10px; }
        input[type="text"], input[type="number"] { width: calc(100% - 22px); padding: 10px; margin-bottom: 10px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
        button[type="submit"], #searchAddressForm button { background-color: #5cb85c; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; width: 100%; box-sizing: border-box; }
        #searchAddressForm button { background-color: #007bff; }
        button[type="submit"]:hover, #searchAddressForm button:hover { background-color: #4cae4c; }
        #searchAddressForm button:hover { background-color: #0056b3; }
        .status { padding: 10px; margin-top:10px; margin-bottom:10px; border-radius: 4px; text-align: center; }
        .success { background-color: #dff0d8; color: #3c763d; }
        .error { background-color: #f2dede; color: #a94442; }
        p.error { color: #a94442; margin: 5px 0; }
    </style>
</head>
<body>
    <nav>
        <a href="/">Set Target</a> |
        <a href="/manage-locations">Manage Locations</a>
    </nav>
    <div class="content-wrapper">
        <h2>Manage Saved Locations</h2>
        <div id="statusMessage"></div>

        <h3>Search and Add Location by Address</h3>
        <form id="searchAddressForm">
            Address: <input type="text" id="search_address" name="search_address" placeholder="e.g., Eiffel Tower, Paris" required><br>
            <button type="submit">Search Address</button>
        </form>
        <div id="searchResults">
            <!-- Search results will be displayed here -->
        </div>

        <h3>Current Saved Locations</h3>
        <div id="locationsList">
            <p>Loading locations...</p>
        </div>

        <h3>Add New Location Manually</h3>
        <form id="addLocationForm">
            Name: <input type="text" id="name" name="name" required><br>
            Latitude: <input type="number" step="any" id="latitude" name="latitude" required><br>
            Longitude: <input type="number" step="any" id="longitude" name="longitude" required><br>
            <button type="submit">Add Location Manually</button>
        </form>
    </div>

    <script>
    function showStatus(message, type) {
        const statusDiv = document.getElementById('statusMessage');
        statusDiv.textContent = message;
        statusDiv.className = 'status ' + type; // 'success' or 'error'
        setTimeout(() => { statusDiv.textContent = ''; statusDiv.className = 'status '; }, 4000);
    }

    function fetchLocations() {
        fetch('/api/locations')
            .then(response => {
                if (!response.ok) throw new Error('Network response was not ok: ' + response.statusText);
                return response.json();
            })
            .then(data => {
                const listDiv = document.getElementById('locationsList');
                listDiv.innerHTML = ''; // Clear old list
                if (data.length === 0) {
                    listDiv.innerHTML = '<p>No locations saved.</p>';
                    return;
                }
                data.forEach((loc, index) => {
                    const item = document.createElement('div');
                    item.innerHTML = `<span>${escapeHtml(loc.name)} (${loc.lat.toFixed(6)}, ${loc.lon.toFixed(6)})</span>
                                      <button onclick="deleteLocation(${index})">Delete</button>`;
                    listDiv.appendChild(item);
                });
            })
            .catch(error => {
                console.error('Error fetching locations:', error);
                document.getElementById('locationsList').innerHTML = '<p>Error loading locations.</p>';
                showStatus('Error fetching locations: ' + error.message, 'error');
            });
    }

    document.getElementById('addLocationForm').addEventListener('submit', function(event) {
        event.preventDefault();
        const formData = new FormData(this);
        if (!formData.get('name').trim() || !formData.get('latitude') || !formData.get('longitude')) {
            showStatus('All fields for manual add are required.', 'error');
            return;
        }
        fetch('/api/locations/add', { method: 'POST', body: formData })
            .then(response => {
                if (!response.ok) return response.text().then(text => { throw new Error(text || 'Failed to add location') });
                return response.text();
            })
            .then(message => {
                showStatus(message || 'Location added successfully!', 'success');
                fetchLocations(); // Refresh list
                this.reset(); // Clear form
            })
            .catch(error => {
                console.error('Error adding location manually:', error);
                showStatus('Error adding location: ' + error.message, 'error');
            });
    });

    document.getElementById('searchAddressForm').addEventListener('submit', function(event) {
        event.preventDefault();
        const address = document.getElementById('search_address').value.trim();
        if (!address) {
            showStatus('Please enter an address to search.', 'error');
            return;
        }
        const searchResultsDiv = document.getElementById('searchResults');
        searchResultsDiv.innerHTML = '<p>Searching...</p>';

        fetch(`/api/geocode-for-add?address=${encodeURIComponent(address)}`)
            .then(response => {
                const responseClone = response.clone(); // Clone to allow reading body twice if needed
                return response.json() // Try to parse as JSON
                    .then(data => ({ // JSON parsing succeeded
                        ok: response.ok, // Use original response.ok to reflect HTTP status
                        status: response.status,
                        data: data
                    }))
                    .catch(jsonError => { // JSON parsing failed
                        // If JSON parsing fails, get the response as text.
                        return responseClone.text().then(textData => ({
                            ok: false, // Mark as not ok because JSON was expected or original response wasn't ok
                            status: responseClone.status,
                            // Store the raw text as a message, or in a way your error handling expects
                            data: { message: textData, status: "error_parsing_json" }, 
                            rawText: textData // Keep raw text for potential direct display or debugging
                        }));
                    });
            })
            .then(result => {
                searchResultsDiv.innerHTML = ''; // Clear previous results or "Searching..."
                // Check result.ok (for HTTP success) AND result.data.status (for application-level success)
                if (result.ok && result.data && result.data.status === 'success') {
                    const item = document.createElement('div');
                    
                    const nameP = document.createElement('p');
                    nameP.innerHTML = `<strong>Found:</strong> ${escapeHtml(result.data.name)}`;
                    
                    const coordsP = document.createElement('p');
                    coordsP.textContent = `Lat: ${result.data.lat.toFixed(6)}, Lon: ${result.data.lon.toFixed(6)}`;
                    
                    const nameInputLabel = document.createElement('label');
                    nameInputLabel.textContent = 'Save as:';
                    nameInputLabel.style.marginTop = '10px';
                    nameInputLabel.style.display = 'block';

                    const nameInput = document.createElement('input');
                    nameInput.type = 'text';
                    nameInput.id = 'searchedLocationName';
                    nameInput.value = result.data.name; // Pre-fill with found name
                    nameInput.style.width = 'calc(100% - 22px)';
                    nameInput.style.padding = '8px';
                    nameInput.style.marginBottom = '10px';
                    nameInput.style.border = '1px solid #ccc';
                    nameInput.style.borderRadius = '4px';

                    const addButton = document.createElement('button');
                    addButton.textContent = 'Add to Favorites';
                    addButton.onclick = function() {
                        const customName = document.getElementById('searchedLocationName').value.trim();
                        if (!customName) {
                            showStatus('Please enter a name for the location.', 'error');
                            return;
                        }
                        addSearchedLocationToFavorites(customName, result.data.lat, result.data.lon);
                    };

                    item.appendChild(nameP);
                    item.appendChild(coordsP);
                    item.appendChild(nameInputLabel);
                    item.appendChild(nameInput);
                    item.appendChild(addButton);
                    searchResultsDiv.appendChild(item);
                } else {
                    // Handle errors: either HTTP error (result.ok is false) 
                    // or application error (result.ok is true but result.data.status is not 'success')
                    // or JSON parsing error (result.ok is false, result.data.message contains raw text)
                    let errorMessage = "An unknown error occurred during geocoding."; 
                    if (result.data && result.data.message) {
                        errorMessage = result.data.message; // Use message from JSON (either server's JSON error or our wrapped text)
                    } else if (result.rawText) { // Fallback to rawText if data.message was not helpful
                        errorMessage = result.rawText;
                    } else if (!result.ok) {
                        errorMessage = `Geocoding request failed with status: ${result.status}`;
                    }
                    
                    searchResultsDiv.innerHTML = `<p class="error">Error: ${escapeHtml(errorMessage)}</p>`;
                    showStatus(`Search error: ${escapeHtml(errorMessage)}`, 'error');
                }
            })
            .catch(networkError => { // This catch is now primarily for network errors or unhandled promise rejections
                console.error('Network or other error geocoding address:', networkError);
                searchResultsDiv.innerHTML = `<p class="error">Network error or unable to reach server: ${escapeHtml(networkError.message)}</p>`;
                showStatus('Network error: ' + escapeHtml(networkError.message), 'error');
            });
    });

    function addSearchedLocationToFavorites(name, lat, lon) {
        const formData = new FormData();
        formData.append('name', name); // Raw name
        formData.append('latitude', lat.toString());
        formData.append('longitude', lon.toString());

        fetch('/api/locations/add', { method: 'POST', body: formData })
            .then(response => {
                if (!response.ok) return response.text().then(text => { throw new Error(text || 'Failed to add location') });
                return response.text();
            })
            .then(message => {
                showStatus(message || 'Location added successfully!', 'success');
                fetchLocations(); // Refresh list
                document.getElementById('searchResults').innerHTML = ''; // Clear search results
                document.getElementById('search_address').value = ''; // Clear search input
            })
            .catch(error => {
                console.error('Error adding searched location:', error);
                showStatus('Error adding location: ' + error.message, 'error');
            });
    }

    function deleteLocation(index) {
        if (!confirm('Are you sure you want to delete this location?')) return;
        const formData = new FormData();
        formData.append('index', index);
        fetch('/api/locations/delete', { method: 'POST', body: formData })
            .then(response => {
                if (!response.ok) return response.text().then(text => { throw new Error(text || 'Failed to delete location') });
                return response.text();
            })
            .then(message => {
                showStatus(message || 'Location deleted successfully!', 'success');
                fetchLocations(); // Refresh list
            })
            .catch(error => {
                console.error('Error deleting location:', error);
                showStatus('Error deleting location: ' + error.message, 'error');
            });
    }

    function escapeHtml(unsafe) {
        if (typeof unsafe !== 'string') {
            console.warn('escapeHtml called with non-string value:', unsafe);
            return String(unsafe); // Convert to string to prevent errors, though this might not be ideal
        }
        return unsafe
             .replace(/&/g, "&amp;")
             .replace(/</g, "&lt;")
             .replace(/>/g, "&gt;")
             .replace(/"/g, "&quot;")
             .replace(/'/g, "&#039;");
    }

    fetchLocations(); // Initial load
    </script>
</body>
</html>
)rawliteral";
#endif // HTML_CONTENT_H