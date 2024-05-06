#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

Preferences preferences;
String TYPE;
String NAME;
String FRIENDLY_NAME;
String OPTIONS;


bool statusLedWifi = 0;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/statusSSE");

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 1000;

int powerOnStatus = 0;

const int buttonPin = 18;
const int setupPin = 15;

bool setupMode = false;

const int ledPin = LED_BUILTIN;  // Use built-in LED

int getSensorReadings() {
  powerOnStatus = digitalRead(13);
  return powerOnStatus;
}

void toggleStatusLedWifi() {
  statusLedWifi = !statusLedWifi;
  digitalWrite(27, statusLedWifi);
}

// Initialize WiFi
void initWiFi(String ssidParamF, String pwdParamF) {
  if (digitalRead(setupPin) == 1 | ssidParamF == "") {
    setupMode = true;
    for (int i = 0; i < 50; i++) {
      delay(100);
      toggleStatusLedWifi();
    }
    // const char* ssid2 = TYPE "-|-" FRIENDLY_NAME;
    // const char* password2 = "12345678";

    Serial.println("\n[*] Creating AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(TYPE + "-|-" + FRIENDLY_NAME, "12345678");
    Serial.print("[+] AP Created with IP Gateway ");
    Serial.println(WiFi.softAPIP());
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidParamF, pwdParamF);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      toggleStatusLedWifi();
      delay(1000);
    }
    digitalWrite(27, LOW);
    statusLedWifi = 0;
    Serial.println(WiFi.localIP());
  }
}

String processor(const String& var) {
  getSensorReadings();
  Serial.println(var);
  if (var == "POWER_STATUS") {
    return String(powerOnStatus);
  }
  // else if(var == "HUMIDITY"){
  //   return String(humidity);
  // }
  return String();
}

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

void serverToggle() {
  pinMode(buttonPin, OUTPUT);
  digitalWrite(buttonPin, LOW);  // Simulate button press for demonstration
  digitalWrite(ledPin, HIGH);    // Turn on LED for visual feedback (optional)
  delay(1000);                   // Simulate button hold for 1 second
  digitalWrite(ledPin, LOW);     // Turn off LED (optional)
  pinMode(buttonPin, INPUT);     // Restore button pin state
  Serial.println("PRESSED BUTTON");
}

void handleToggle(AsyncWebServerRequest* request) {
  serverToggle();
  request->send(200, "application/json", "{\"status\":202\"}");
}

void handleOn(AsyncWebServerRequest* request) {
  if (powerOnStatus == 0) {
    request->send(200, "application/json", "{\"status\":202}");
    serverToggle();
    return;
  }

  request->send(422, "application/json", "{\"status\": 422, \"error\": \"Unprocessable Entity\", \"message\": \"Server is already on\"}");
}

void handleOff(AsyncWebServerRequest* request) {
  if (powerOnStatus == 1) {
    request->send(200, "application/json", "{\"status\":202}");
    serverToggle();
    return;
  }

  request->send(422, "application/json", "{\"status\": 422, \"error\": \"Unprocessable Entity\", \"message\": \"Server is already off\"}");
}


void handleAbout(AsyncWebServerRequest* request) {
  // Construct the JSON string
  String json = "{\"type\": \""  + TYPE + "\", "
                "\"name\": \"" + NAME + "\", "
                "\"friendlyName\": \"" + FRIENDLY_NAME + "\", "
                "\"options\": " + OPTIONS + "}";
  // Send the JSON response
  request->send(200, "application/json", json);
}

void handleStatus(AsyncWebServerRequest* request) {
  // Construct the JSON string
  String json = "{\"state\": " + String(powerOnStatus) + "}";

  // Send the JSON response
  request->send(200, "application/json", json);
}


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

  // Print the received data outside the loop
  Serial.println("Received data:");
  Serial.print("SSID: ");
  Serial.println(ssid3);
  preferences.putString("ssid", ssid3);
  Serial.print("Password: ");
  Serial.println(password3);
  preferences.putString("password", password3);
  Serial.print("Name: ");
  Serial.println(name);
  preferences.putString("name", name);
  Serial.print("Display Name: ");
  Serial.println(displayName);
  preferences.putString("displayName", displayName);
  Serial.print("Options: ");
  Serial.println(options);
  preferences.putString("options", options);
  request->send(200, "text/plain", "Data received successfully!");  // Close the Preferences
  preferences.end();
  Serial.println("Restarting in 2 seconds...");
  delay(2000);
  ESP.restart();
}

// ======================================= API Functions END ======================================= \\

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Arduino Server Control</title>
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
      <h1>Server Control</h1>
    </header>
    <main>
      <button id="toggleButton">Loading...</button>
      <!-- <p>%POWER_STATUS%</p> -->
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

  pinMode(27, OUTPUT);  // statusLedWifi
  pinMode(setupPin, INPUT_PULLDOWN);
  pinMode(13, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT);


  initWiFi(preferences.getString("ssid", ""),preferences.getString("password", ""));




  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/state/toggle", HTTP_GET | HTTP_POST, handleToggle);
  server.on("/state/on", HTTP_GET | HTTP_POST, handleOn);
  server.on("/state/off", HTTP_GET | HTTP_POST, handleOff);
  // ------------------------ API ------------------------ \\

  // ####################### Setup ########################\\


  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", R"rawliteral(<!DOCTYPE html>
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
        <label for="ssid">SSID:</label>
        <input type="text" id="ssid" name="ssid" required>
    </div>
    <div class="form-group">
        <label for="password">Password:</label>
        <input type="password" id="password" name="password" required>
    </div>
    <div class="form-group">
        <label for="name">Name:</label>
        <input type="text" id="name" name="name" required>
    </div>
    <div class="form-group">
        <label for="displayName">Display Name:</label>
        <input type="text" id="displayName" name="displayName" required>
    </div>
    <div class="form-group">
        <label for="options">Options (json):</label>
        <input type="text" id="options" name="options" required>
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
            return response.json();
        })
        .then(data => {
            // Handle success response
            console.log('Success:', data);
            // You can redirect or show a success message here
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
)rawliteral");
  });
  server.on("/setup", HTTP_POST, handleSetup);


  server.on("/about", HTTP_GET, handleAbout);
  server.on("/status", HTTP_GET | HTTP_POST, handleStatus);

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient* client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    // client->send("hello!", "message", millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    checkWifi();

    getSensorReadings();
    Serial.println(powerOnStatus);

    // Send Events to the Web Client with the Sensor Readings
    // events.send("ping",NULL,millis());
    events.send(String(powerOnStatus).c_str(), "message", millis());

    lastTime = millis();
    Serial.println(digitalRead(setupPin));
  }
}
