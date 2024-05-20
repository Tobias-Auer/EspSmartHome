#include "wled.h"
#include <Preferences.h>
/*
 * This v1 usermod file allows you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * EEPROM bytes 2750+ are reserved for your custom use case. (if you extend #define EEPSIZE in const.h)
 * If you just need 8 bytes, use 2551-2559 (you do not need to increase EEPSIZE)
 *
 * Consider the v2 usermod API if you need a more advanced feature set!
 */

//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

//gets called once at boot. Do all initialization that doesn't depend on network here
Preferences preferences;
String TYPE;
String NAME;
String FRIENDLY_NAME;
String OPTIONS;

const char setupForm[] PROGMEM = R"rawliteral(<!DOCTYPE html>
  <html lang="en">
  <head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Setup Form</title>
  <style>
      body {
          font-family: Arial, sans-serif;
      }
      form {
          max-width: 400px;
          margin: 0 auto;
      }
      .form-group {
          margin-bottom: 20px;
      }
      label {
          display: block;
          margin-bottom: 5px;
      }
      input[type="text"],
      input[type="password"] {
          width: 100%;
          padding: 8px;
          
          border: 1px solid #ccc;
          border-radius: 5px;
      }
      input[type="submit"] {
          background-color: #4CAF50;
          color: white;
          padding: 10px 20px;
          border: none;
          border-radius: 5px;
          cursor: pointer;
      }
      input[type="submit"]:hover {
          background-color: #45a049;
      }
  </style>
  </head>
  <body>

  <form id="setupForm" action="/setup" method="post">
      <div class="form-group">
          <label for="type">Type:</label>
          <input type="text" id="type" name="type" value="{DEVICE_TYPE}" disabled>
      </div>
      <div class="form-group">
          <label for="ssid">SSID:</label>
          <input type="text" id="ssid" name="ssid" value="managed by wled" disabled>
      </div>
      <div class="form-group">
          <label for="password">Password:</label>
          <input type="text" id="password" name="password" value="managed by wled" disabled>
      </div>
      <div class="form-group">
          <label for="name">Name:</label>
          <input type="text" id="name" name="name" value="{DEVICE_NAME}" required>
      </div>
      <div class="form-group">
          <label for="displayName">Display Name:</label>
          <input type="text" id="displayName" name="displayName" value="{DEVICE_DISPLAY_NAME}" required>
      </div>
      <div class="form-group">
          <label for="options">Options (json):</label>
          <input type="text" id="options" name="options" value='{DEVICE_OPTIONS}' readonly required>
      </div>
      <input type="submit" value="Submit">
  </form>

  <script>
      document.getElementById("setupForm").addEventListener("submit", function(event) {
          event.preventDefault(); // Prevent the form from submitting normally

          // Get form data
          var formData = new FormData(this);

          // Send form data via POST request
          fetch('/setup', {
              method: 'POST',
              body: formData
          })
          .then(response => {
              if (!response.ok) {
                  throw new Error('Network response was not ok');
              }
              return response;
          })
          .then(data => {
              // Handle success response
              console.log('Success:', data);
          })
          .catch(error => {
              // Handle error
              console.error('Error:', error);
              // You can show an error message here
          });
      });
  </script>

  </body>
  </html>
  )rawliteral";

void handleSetup(AsyncWebServerRequest* request) {
  if (request->method() != HTTP_POST) {
    request->send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String ssid3, password3, name, displayName, options;

  int paramsNr = request->params();
  for (int i = 0; i < paramsNr; i++) {
    AsyncWebParameter* p = request->getParam(i);

    Serial.print("Param name: ");
    Serial.println(p->name());
    Serial.print("Param value: ");
    Serial.println(p->value());
    Serial.println("------");

    // Assign values to variables
    if (p->name() == "name") {
      name = p->value();
    } else if (p->name() == "displayName") {
      displayName = p->value();
    } else if (p->name() == "options") {
      options = p->value();
    }
  }
  preferences.putString("name", name);
  preferences.putString("displayName", displayName);
  preferences.putString("options", options);
  request->send(200, "text/plain", "Data received successfully!");  // Close the Preferences
  preferences.end();
  Serial.println("Restarting in 2 seconds...");
  delay(2000);
  ESP.restart();
}
void handleAbout(AsyncWebServerRequest* request) {
  // Construct the JSON string
  String json = "{\"type\": \"" + TYPE + "\", "
                                         "\"name\": \""
                + NAME + "\", "
                         "\"friendlyName\": \""
                + FRIENDLY_NAME + "\", "
                                  "\"options\": "
                + OPTIONS + "}";
  // Send the JSON response
  request->send(200, "application/json", json);
}
String processor(String html) {
  html.replace("{DEVICE_OPTIONS}", String(OPTIONS));
  html.replace("{DEVICE_DISPLAY_NAME}", String(FRIENDLY_NAME));
  html.replace("{DEVICE_NAME}", String(NAME));
  html.replace("{DEVICE_TYPE}", String(TYPE));

  return html;
}

void userSetup()
{
    
  preferences.begin("setup", false);
    TYPE = "WLED";
    NAME = preferences.getString("name", "pls_change");
    FRIENDLY_NAME = preferences.getString("displayName", "pls_change");
    OPTIONS = preferences.getString("options", "{}");



  server.on("/setup", HTTP_POST, handleSetup);
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", processor(setupForm).c_str());
  });
  server.on("/about", HTTP_GET, handleAbout);

}

//gets called every time WiFi is (re-)connected. Initialize own network interfaces here
void userConnected()
{

}

//loop. You can use "if (WLED_CONNECTED)" to check for successful connection
void userLoop()
{

}
