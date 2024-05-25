#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

//                                      V A R I A B L E S

Preferences preferences;
String TYPE;
String NAME;
String FRIENDLY_NAME;
String OPTIONS;

bool statusLedWifi = 0;
bool setupMode = false;

unsigned long lastTime = 0;
unsigned long timerDelay = 1000;

int powerOnStatus = 0;
int resetTimer = 0;

const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
    <head>
      <meta charset="UTF-8" />
      <meta name="viewport" content="width=device-width, initial-scale=1.0" />
      <title>{DEVICE_DISPLAY_NAME} - SmartESP</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          margin: 0;
          padding: 0;
          background: rgb(1, 116, 249);
          background: radial-gradient(
            circle,
            rgba(1, 116, 249, 1) 0%,
            rgb(21, 119, 52) 100%
          );
          color: #fff;
        }

        header {
          background-color: #333;
          padding: 20px;
          text-align: center;
        }

        h1 {
          margin: 0;
        }

        main {
          display: flex;
          justify-content: center;
          align-items: center;
          height: calc(100vh - 100px);
        }

        button {
          padding: 15px 30px;
          font-size: 18px;
          border: none;
          border-radius: 5px;
          background-color: #4caf50;
          color: white;
          cursor: pointer;
          transition: background-color 0.3s ease;
        }
        button:hover {
          background-color: #45a049;
        }
        @media screen and (max-width: 394px) {
          h1 {
            font-size: 24px;
          }
        }
      </style>
    </head>
    <body>
      <header>
        <h1>{DEVICE_DISPLAY_NAME}</h1>
      </header>
      <main>
        <button id="toggleButton">Loading...</button>
        <!-- <p>{POWER_STATUS}</p> -->
      </main>
      <script>
        document.addEventListener("DOMContentLoaded", function () {
          const toggleButton = document.getElementById("toggleButton");
          toggleButton.textContent = "Loading...";
          function updateButtonState(state) {
            toggleButton.textContent = state === "1"
              ? "Computer ausschalten"
              : "Computer einschalten";
          }
  toggleButton.addEventListener("click", function () {
    // Send fetch request to /toggle
    fetch("/state/toggle")
      .then(response => {
        if (!response.ok) {
          throw new Error("Network response was not ok");
        }
        return response.text();
      })
      .then(responseText => {
        console.log("Response:", responseText);
      })
      .catch(error => {
        console.error("Fetch error:", error);
      });
  });
          function handleSSE(event) {
            console.log("-" + event.data + "-");
            // const data = JSON.parse(event.data);
            const computerStateOn = event.data;
            updateButtonState(computerStateOn);
          }
          const eventSource = new EventSource("/statusSSE");
          eventSource.addEventListener("message", handleSSE);
        });
      </script>
    </body>
  </html>
  )rawliteral";

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
          <input type="text" id="ssid" name="ssid" value="{WIFI_SSID}" required>
      </div>
      <div class="form-group">
          <label for="password">Password:</label>
          <input type="password" id="password" name="password" value="{WIFI_PASSWORD}" required>
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
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/statusSSE");


// Pins
const int buttonPin = 18;
const int setupPin = 15;
const int ledPin = LED_BUILTIN;  // Use built-in LED
const int wifiLed = 27;
const int powerInput = 13;

//                                      F U N C T I O N S

//update all variables
void getSensorReadings() {
  powerOnStatus = digitalRead(powerInput);
}
//toggle wifi led
void toggleStatusLedWifi() {
  statusLedWifi = !statusLedWifi;
  digitalWrite(wifiLed, statusLedWifi);
}

// Initialize WiFi or AP
void initWiFi(String ssidParamF, String pwdParamF) {
  if (digitalRead(setupPin) == 1 | ssidParamF == "") {
    Serial.println("\n[*] Creating AP");
    setupMode = true;
    for (int i = 0; i < 50; i++) {
      delay(100);
      toggleStatusLedWifi();
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(FRIENDLY_NAME, "12345678");
    Serial.print("[+] AP Created with IP Gateway ");
    Serial.println(WiFi.softAPIP());
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidParamF, pwdParamF);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {
      if (resetTimer >= 450) { // 15 min.
        ESP.restart();
      }
      Serial.print('.');
      toggleStatusLedWifi();
      delay(500);
      toggleStatusLedWifi();
      delay(1500);
      resetTimer += 1;
    }
    Serial.println(WiFi.localIP());
  }
  digitalWrite(wifiLed, LOW);
  statusLedWifi = 0;
}

// replaces variables in html code
String processor(String html) {
  getSensorReadings();
  html.replace("{POWER_STATUS}", String(powerOnStatus));
  html.replace("{DEVICE_OPTIONS}", String(OPTIONS));
  html.replace("{DEVICE_DISPLAY_NAME}", String(FRIENDLY_NAME));
  html.replace("{DEVICE_NAME}", String(NAME));
  html.replace("{DEVICE_TYPE}", String(TYPE));
  html.replace("{WIFI_PASSWORD}", String(preferences.getString("password", "")));
  html.replace("{WIFI_SSID}", String(preferences.getString("ssid", "")));

  return html;
}


// check if the esp is connected and restart if not
void checkWifi() {
  if (WiFi.status() != WL_CONNECTED && !setupMode) {
    Serial.println("WiFi connection lost. Waiting...");
    for (int i = 0; i < 50; i++) {
      delay(100);
      toggleStatusLedWifi();
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Restarting...");
      ESP.restart();
    }
  }
}

// ======================================= API Functions START ======================================= \\

// presses the power button on the pc
void serverToggle() {
  pinMode(buttonPin, OUTPUT);
  digitalWrite(buttonPin, LOW);  // Simulate button press for demonstration
  digitalWrite(ledPin, HIGH);    // Turn on LED for visual feedback (optional)
  delay(1000);                   // Simulate button hold for 1 second
  digitalWrite(ledPin, LOW);     // Turn off LED (optional)
  pinMode(buttonPin, INPUT);     // Restore button pin state
  Serial.println("PRESSED BUTTON");
}

//handle the toggle request
void handleToggle(AsyncWebServerRequest* request) {
  serverToggle();
  request->send(200, "application/json", "{\"status\":202\"}");
}

//handle the om request
void handleOn(AsyncWebServerRequest* request) {
  if (powerOnStatus == 0) {
    request->send(200, "application/json", "{\"status\":202}");
    serverToggle();
    return;
  }
  request->send(422, "application/json", "{\"status\": 422, \"error\": \"Unprocessable Entity\", \"message\": \"Server is already on\"}");
}

//handle the of request
void handleOff(AsyncWebServerRequest* request) {
  if (powerOnStatus == 1) {
    request->send(200, "application/json", "{\"status\":202}");
    serverToggle();
    return;
  }
  request->send(422, "application/json", "{\"status\": 422, \"error\": \"Unprocessable Entity\", \"message\": \"Server is already off\"}");
}

//handle the about request
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

//handle the status request
void handleStatus(AsyncWebServerRequest* request) {
  // Construct the JSON string
  String json = "{\"state\": " + String(powerOnStatus) + "}";

  // Send the JSON response
  request->send(200, "application/json", json);
}

//handle the setup request | Get + Post
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
    if (p->name() == "ssid") {
      ssid3 = p->value();
    } else if (p->name() == "password") {
      password3 = p->value();
    } else if (p->name() == "name") {
      name = p->value();
    } else if (p->name() == "displayName") {
      displayName = p->value();
    } else if (p->name() == "options") {
      options = p->value();
    }
  }
  preferences.putString("ssid", ssid3);
  preferences.putString("password", password3);
  preferences.putString("name", name);
  preferences.putString("displayName", displayName);
  preferences.putString("options", options);
  request->send(200, "text/plain", "Data received successfully!");  // Close the Preferences
  preferences.end();
  Serial.println("Restarting in 2 seconds...");
  delay(2000);
  ESP.restart();
}

// ======================================= API Functions END ======================================= \\

void setup() {
  Serial.begin(115200);
  preferences.begin("setup", false);

  TYPE = "serverControl";
  NAME = preferences.getString("name", "pls_change");
  FRIENDLY_NAME = preferences.getString("displayName", "pls_change");
  OPTIONS = preferences.getString("options", "{}");

  Serial.println("---------");
  Serial.println(NAME);
  Serial.println(FRIENDLY_NAME);
  Serial.println(OPTIONS);
  Serial.println(preferences.getString("ssid", ""));
  Serial.println(preferences.getString("password", ""));
  Serial.println("---------");

  pinMode(wifiLed, OUTPUT);
  pinMode(setupPin, INPUT_PULLDOWN);
  pinMode(powerInput, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT);


  initWiFi(preferences.getString("ssid", ""), preferences.getString("password", ""));

  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", processor(index_html).c_str());
  });

  server.on("/state/toggle", HTTP_GET | HTTP_POST, handleToggle);
  server.on("/state/on", HTTP_GET | HTTP_POST, handleOn);
  server.on("/state/off", HTTP_GET | HTTP_POST, handleOff);

  server.on("/setup", HTTP_POST, handleSetup);
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", processor(setupForm).c_str());
  });
  server.on("/about", HTTP_GET, handleAbout);
  server.on("/status", HTTP_GET | HTTP_POST, handleStatus);

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient* client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
  });

  server.addHandler(&events);
  server.begin();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    checkWifi();

    getSensorReadings();
    Serial.println(powerOnStatus);

    events.send(String(powerOnStatus).c_str(), "message", millis());

    lastTime = millis();
    Serial.println(digitalRead(setupPin));
    Serial.println();
  }
}
