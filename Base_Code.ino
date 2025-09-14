#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <EEPROM.h>

// WiFi credentials
const char* ssid = "Surya_space_4g";        // Replace with your WiFi SSID
const char* password = "Surya@79802@02"; // Replace with your WiFi password

// Pin definitions
const int ONBOARD_LED = 2;  // Built-in LED (active LOW)
const int RELAY_PIN_1 = 16; // D0
const int RELAY_PIN_2 = 5;  // D1
const int RELAY_PIN_3 = 4;  // D2

// Web server on port 80
ESP8266WebServer server(80);

// NTP Client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// IST is UTC+5:30 (19800 seconds offset)
const long utcOffsetInSeconds = 19800;

// Global variables for time
String currentTime24 = "";
String currentDate = "";
String currentDay = "";
unsigned long lastUpdate = 0;
unsigned long ledBlinkTimer = 0;
bool ledState = false;

// Schedule structure
struct Schedule {
  String onTime;
  String offTime;
  bool isActive;
};

Schedule schedules[3];

// EEPROM addresses
const int EEPROM_SIZE = 512;
const int SCHEDULE_START_ADDR = 0;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting NodeMCU Scheduled Timer Controller...");
  
  // Initialize pins
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  pinMode(RELAY_PIN_3, OUTPUT);
  
  // Set initial states (HIGH = OFF for relays)
  digitalWrite(RELAY_PIN_1, HIGH);
  digitalWrite(RELAY_PIN_2, HIGH);
  digitalWrite(RELAY_PIN_3, HIGH);
  digitalWrite(ONBOARD_LED, HIGH); // LED OFF initially
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadSchedules();
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(ONBOARD_LED, HIGH); // LED ON when not connected
  }
  
  Serial.println();
  Serial.println("WiFi connected successfully!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Initialize NTP client
  timeClient.begin();
  timeClient.setTimeOffset(utcOffsetInSeconds);
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/schedule", handleSchedulePage);
  server.on("/time", handleTimeAPI);
  server.on("/set-schedule", HTTP_POST, handleSetSchedule);
  server.on("/get-schedules", handleGetSchedules);
  server.onNotFound(handleNotFound);
  
  // Start server
  server.begin();
  Serial.println("Web server started!");
  Serial.print("Access the controller at: http://");
  Serial.println(WiFi.localIP());
  Serial.print("Schedule settings at: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/schedule");
}

void loop() {
  server.handleClient();
  
  // Handle LED blinking if connected
  if (WiFi.status() == WL_CONNECTED) {
    if (millis() - ledBlinkTimer > 1000) { // Blink every second
      ledState = !ledState;
      digitalWrite(ONBOARD_LED, ledState ? LOW : HIGH); // Invert for active LOW LED
      ledBlinkTimer = millis();
    }
  } else {
    digitalWrite(ONBOARD_LED, LOW); // LED ON when disconnected
  }
  
  // Update time every second
  if (millis() - lastUpdate > 1000) {
    updateTime();
    checkSchedules();
    lastUpdate = millis();
  }
}

void updateTime() {
  timeClient.update();
  
  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawtime = epochTime;
  struct tm * ti = localtime(&rawtime);
  
  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();
  int second = timeClient.getSeconds();
  
  int day = ti->tm_mday;
  int month = ti->tm_mon + 1;
  int year = ti->tm_year + 1900;
  
  String weekDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  currentDay = weekDays[ti->tm_wday];
  
  // Format time (24-hour format)
  currentTime24 = "";
  if (hour < 10) currentTime24 += "0";
  currentTime24 += String(hour) + ":";
  if (minute < 10) currentTime24 += "0";
  currentTime24 += String(minute) + ":";
  if (second < 10) currentTime24 += "0";
  currentTime24 += String(second);
  
  // Format date
  currentDate = "";
  if (day < 10) currentDate += "0";
  currentDate += String(day) + "/";
  if (month < 10) currentDate += "0";
  currentDate += String(month) + "/" + String(year);
}

void checkSchedules() {
  String currentHM = currentTime24.substring(0, 5); // HH:MM format
  
  for (int i = 0; i < 3; i++) {
    if (schedules[i].onTime != "" && schedules[i].offTime != "") {
      int relayPin = (i == 0) ? RELAY_PIN_1 : (i == 1) ? RELAY_PIN_2 : RELAY_PIN_3;
      
      // Convert times to minutes for easier comparison
      int currentMinutes = timeStringToMinutes(currentHM);
      int onMinutes = timeStringToMinutes(schedules[i].onTime);
      int offMinutes = timeStringToMinutes(schedules[i].offTime);
      
      bool shouldBeActive = false;
      
      if (onMinutes < offMinutes) {
        // Same day schedule (e.g., 09:00 to 17:00)
        shouldBeActive = (currentMinutes >= onMinutes && currentMinutes < offMinutes);
      } else {
        // Crosses midnight (e.g., 19:15 to 06:30)
        shouldBeActive = (currentMinutes >= onMinutes || currentMinutes < offMinutes);
      }
      
      if (shouldBeActive && !schedules[i].isActive) {
        digitalWrite(relayPin, LOW); // Turn ON
        schedules[i].isActive = true;
        Serial.println("Schedule " + String(i+1) + " activated at " + currentHM);
      } else if (!shouldBeActive && schedules[i].isActive) {
        digitalWrite(relayPin, HIGH); // Turn OFF
        schedules[i].isActive = false;
        Serial.println("Schedule " + String(i+1) + " deactivated at " + currentHM);
      }
    }
  }
}
int timeStringToMinutes(String timeStr) {
  int colonIndex = timeStr.indexOf(':');
  int hours = timeStr.substring(0, colonIndex).toInt();
  int minutes = timeStr.substring(colonIndex + 1).toInt();
  return hours * 60 + minutes;
}

void saveSchedules() {
  int addr = SCHEDULE_START_ADDR;
  for (int i = 0; i < 3; i++) {
    // Save on time (10 bytes)
    for (int j = 0; j < 10; j++) {
      EEPROM.write(addr++, j < schedules[i].onTime.length() ? schedules[i].onTime[j] : 0);
    }
    // Save off time (10 bytes)
    for (int j = 0; j < 10; j++) {
      EEPROM.write(addr++, j < schedules[i].offTime.length() ? schedules[i].offTime[j] : 0);
    }
    // Save active state (1 byte)
    EEPROM.write(addr++, schedules[i].isActive ? 1 : 0);
  }
  EEPROM.commit();
}

void loadSchedules() {
  int addr = SCHEDULE_START_ADDR;
  for (int i = 0; i < 3; i++) {
    // Load on time
    char onTimeStr[11] = {0};
    for (int j = 0; j < 10; j++) {
      onTimeStr[j] = EEPROM.read(addr++);
    }
    schedules[i].onTime = String(onTimeStr);
    
    // Load off time
    char offTimeStr[11] = {0};
    for (int j = 0; j < 10; j++) {
      offTimeStr[j] = EEPROM.read(addr++);
    }
    schedules[i].offTime = String(offTimeStr);
    
    // Load active state
    schedules[i].isActive = EEPROM.read(addr++) == 1;
    
    // Clean up strings
    schedules[i].onTime.trim();
    schedules[i].offTime.trim();
  }
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>NodeMCU Timer Controller</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #1a1a1a;
            color: #e0e0e0;
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        
        .container {
            text-align: center;
            padding: 40px;
            background: #2d2d2d;
            border-radius: 15px;
            border: 1px solid #404040;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5);
            max-width: 90vw;
        }
        
        h1 {
            font-size: 2.5em;
            margin-bottom: 30px;
            color: #ffffff;
            font-weight: 300;
        }
        
        .time-display {
            font-size: 4em;
            font-weight: bold;
            color: #00ff88;
            margin: 20px 0;
            font-family: 'Courier New', monospace;
            text-shadow: 0 0 10px rgba(0, 255, 136, 0.3);
        }
        
        .date-display {
            font-size: 2em;
            color: #87CEEB;
            margin: 15px 0;
        }
        
        .day-display {
            font-size: 1.5em;
            color: #ffaa00;
            margin: 10px 0 30px 0;
        }
        
        .status-panel {
            background: #333333;
            border-radius: 10px;
            padding: 20px;
            margin: 20px 0;
            border: 1px solid #555555;
        }
        
        .status-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin: 10px 0;
            padding: 10px;
            background: #404040;
            border-radius: 5px;
        }
        
        .status-active {
            background: rgba(0, 255, 136, 0.2);
            border-left: 4px solid #00ff88;
        }
        
        .status-inactive {
            background: rgba(255, 170, 0, 0.2);
            border-left: 4px solid #ffaa00;
        }
        
        .buttons {
            margin-top: 30px;
        }
        
        button {
            background: linear-gradient(45deg, #555555, #777777);
            border: 1px solid #888888;
            color: #ffffff;
            padding: 15px 30px;
            font-size: 1.1em;
            border-radius: 8px;
            cursor: pointer;
            margin: 10px;
            transition: all 0.3s ease;
        }
        
        button:hover {
            background: linear-gradient(45deg, #666666, #888888);
            transform: translateY(-2px);
        }
        
        .nav-button {
            background: linear-gradient(45deg, #007acc, #0099ff);
        }
        
        .nav-button:hover {
            background: linear-gradient(45deg, #0088dd, #00aaff);
        }
        
        @media (max-width: 768px) {
            .time-display { font-size: 2.5em; }
            .date-display { font-size: 1.5em; }
            h1 { font-size: 2em; }
            .container { padding: 20px; }
            .status-row { flex-direction: column; text-align: center; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>⏰ Timer Controller</h1>
        
        <div class="time-display" id="timeDisplay">--:--:--</div>
        <div class="date-display" id="dateDisplay">--/--/----</div>
        <div class="day-display" id="dayDisplay">Loading...</div>
        
        <div class="status-panel">
            <h3 style="margin-bottom: 15px; color: #ffffff;">Schedule Status</h3>
            <div id="schedule1" class="status-row status-inactive">
                <span><strong>D0 (Pin 1):</strong> <span id="status1">OFF</span></span>
                <span id="time1">--:-- to --:--</span>
            </div>
            <div id="schedule2" class="status-row status-inactive">
                <span><strong>D1 (Pin 2):</strong> <span id="status2">OFF</span></span>
                <span id="time2">--:-- to --:--</span>
            </div>
            <div id="schedule3" class="status-row status-inactive">
                <span><strong>D2 (Pin 3):</strong> <span id="status3">OFF</span></span>
                <span id="time3">--:-- to --:--</span>
            </div>
        </div>
        
        <div class="buttons">
            <button onclick="refreshTime()">🔄 Refresh</button>
            <button class="nav-button" onclick="location.href='/schedule'">⚙️ Schedule Settings</button>
        </div>
    </div>
    
    <script>
        function updateTime() {
            fetch('/time')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('timeDisplay').textContent = data.time;
                    document.getElementById('dateDisplay').textContent = data.date;
                    document.getElementById('dayDisplay').textContent = data.day;
                })
                .catch(error => {
                    console.error('Error fetching time:', error);
                    document.getElementById('dayDisplay').textContent = 'Connection Error';
                });
        }
        
        function updateScheduleStatus() {
            fetch('/get-schedules')
                .then(response => response.json())
                .then(data => {
                    for (let i = 0; i < 3; i++) {
                        const schedule = data.schedules[i];
                        const statusElement = document.getElementById('status' + (i + 1));
                        const timeElement = document.getElementById('time' + (i + 1));
                        const rowElement = document.getElementById('schedule' + (i + 1));
                        
                        statusElement.textContent = schedule.active ? 'ON' : 'OFF';
                        timeElement.textContent = schedule.onTime + ' to ' + schedule.offTime;
                        
                        rowElement.className = 'status-row ' + (schedule.active ? 'status-active' : 'status-inactive');
                    }
                })
                .catch(error => console.error('Error fetching schedules:', error));
        }
        
        function refreshTime() {
            updateTime();
            updateScheduleStatus();
        }
        
        // Initialize
        updateTime();
        updateScheduleStatus();
        setInterval(() => {
            updateTime();
            updateScheduleStatus();
        }, 2000); // Update every 2 seconds
    </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleSchedulePage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Schedule Settings</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #1a1a1a;
            color: #e0e0e0;
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 30px;
            background: #2d2d2d;
            border-radius: 15px;
            border: 1px solid #404040;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5);
        }
        
        h1 {
            text-align: center;
            margin-bottom: 30px;
            color: #ffffff;
            font-weight: 300;
        }
        
        .schedule-group {
            background: #333333;
            border-radius: 10px;
            padding: 20px;
            margin: 20px 0;
            border: 1px solid #555555;
        }
        
        .schedule-group h3 {
            color: #00ff88;
            margin-bottom: 15px;
        }
        
        .time-inputs {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin-bottom: 15px;
        }
        
        .input-group {
            display: flex;
            flex-direction: column;
        }
        
        label {
            margin-bottom: 5px;
            color: #cccccc;
            font-weight: 500;
        }
        
        input[type="time"] {
            padding: 12px;
            border: 1px solid #555555;
            border-radius: 6px;
            background: #404040;
            color: #ffffff;
            font-size: 1.1em;
        }
        
        input[type="time"]:focus {
            outline: none;
            border-color: #00ff88;
            box-shadow: 0 0 0 2px rgba(0, 255, 136, 0.2);
        }
        
        .button-group {
            text-align: center;
            margin-top: 30px;
        }
        
        button {
            background: linear-gradient(45deg, #555555, #777777);
            border: 1px solid #888888;
            color: #ffffff;
            padding: 15px 30px;
            font-size: 1.1em;
            border-radius: 8px;
            cursor: pointer;
            margin: 10px;
            transition: all 0.3s ease;
        }
        
        button:hover {
            background: linear-gradient(45deg, #666666, #888888);
            transform: translateY(-2px);
        }
        
        .save-button {
            background: linear-gradient(45deg, #007acc, #0099ff);
        }
        
        .save-button:hover {
            background: linear-gradient(45deg, #0088dd, #00aaff);
        }
        
        .back-button {
            background: linear-gradient(45deg, #666666, #888888);
        }
        
        .status-message {
            text-align: center;
            padding: 10px;
            border-radius: 6px;
            margin: 10px 0;
            display: none;
        }
        
        .success {
            background: rgba(0, 255, 136, 0.2);
            border: 1px solid #00ff88;
            color: #00ff88;
        }
        
        .error {
            background: rgba(255, 85, 85, 0.2);
            border: 1px solid #ff5555;
            color: #ff5555;
        }
        
        @media (max-width: 768px) {
            .time-inputs {
                grid-template-columns: 1fr;
                gap: 15px;
            }
            .container {
                padding: 20px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>⚙️ Schedule Settings</h1>
        
        <form id="scheduleForm">
            <div class="schedule-group">
                <h3>Schedule 1 - Pin D0</h3>
                <div class="time-inputs">
                    <div class="input-group">
                        <label for="on1">On Time:</label>
                        <input type="time" id="on1" name="on1" required>
                    </div>
                    <div class="input-group">
                        <label for="off1">Off Time:</label>
                        <input type="time" id="off1" name="off1" required>
                    </div>
                </div>
            </div>
            
            <div class="schedule-group">
                <h3>Schedule 2 - Pin D1</h3>
                <div class="time-inputs">
                    <div class="input-group">
                        <label for="on2">On Time:</label>
                        <input type="time" id="on2" name="on2" required>
                    </div>
                    <div class="input-group">
                        <label for="off2">Off Time:</label>
                        <input type="time" id="off2" name="off2" required>
                    </div>
                </div>
            </div>
            
            <div class="schedule-group">
                <h3>Schedule 3 - Pin D2</h3>
                <div class="time-inputs">
                    <div class="input-group">
                        <label for="on3">On Time:</label>
                        <input type="time" id="on3" name="on3" required>
                    </div>
                    <div class="input-group">
                        <label for="off3">Off Time:</label>
                        <input type="time" id="off3" name="off3" required>
                    </div>
                </div>
            </div>
            
            <div id="statusMessage" class="status-message"></div>
            
            <div class="button-group">
                <button type="submit" class="save-button">💾 Save Schedules</button>
                <button type="button" class="back-button" onclick="location.href='/'">🏠 Back to Home</button>
            </div>
        </form>
    </div>
    
    <script>
        // Load existing schedules
        function loadSchedules() {
            fetch('/get-schedules')
                .then(response => response.json())
                .then(data => {
                    for (let i = 0; i < 3; i++) {
                        const schedule = data.schedules[i];
                        document.getElementById('on' + (i + 1)).value = schedule.onTime || '';
                        document.getElementById('off' + (i + 1)).value = schedule.offTime || '';
                    }
                })
                .catch(error => console.error('Error loading schedules:', error));
        }
        
        // Save schedules
        document.getElementById('scheduleForm').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const formData = new FormData();
            for (let i = 1; i <= 3; i++) {
                formData.append('on' + i, document.getElementById('on' + i).value);
                formData.append('off' + i, document.getElementById('off' + i).value);
            }
            
            fetch('/set-schedule', {
                method: 'POST',
                body: formData
            })
            .then(response => response.text())
            .then(data => {
                showMessage(data, 'success');
            })
            .catch(error => {
                showMessage('Error saving schedules: ' + error, 'error');
            });
        });
        
        function showMessage(message, type) {
            const statusDiv = document.getElementById('statusMessage');
            statusDiv.textContent = message;
            statusDiv.className = 'status-message ' + type;
            statusDiv.style.display = 'block';
            
            setTimeout(() => {
                statusDiv.style.display = 'none';
            }, 3000);
        }
        
        // Load schedules on page load
        loadSchedules();
    </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleTimeAPI() {
  String json = "{";
  json += "\"time\":\"" + currentTime24 + "\",";
  json += "\"date\":\"" + currentDate + "\",";
  json += "\"day\":\"" + currentDay + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleSetSchedule() {
  for (int i = 1; i <= 3; i++) {
    String onParam = "on" + String(i);
    String offParam = "off" + String(i);
    
    if (server.hasArg(onParam) && server.hasArg(offParam)) {
      schedules[i-1].onTime = server.arg(onParam);
      schedules[i-1].offTime = server.arg(offParam);
      schedules[i-1].isActive = false; // Reset active state
      
      // Reset pin to HIGH state when schedule is updated
      int relayPin = (i == 1) ? RELAY_PIN_1 : (i == 2) ? RELAY_PIN_2 : RELAY_PIN_3;
      digitalWrite(relayPin, HIGH);
    }
  }
  
  saveSchedules();
  server.send(200, "text/plain", "Schedules saved successfully!");
  
  Serial.println("Schedules updated:");
  for (int i = 0; i < 3; i++) {
    Serial.println("Schedule " + String(i+1) + ": " + schedules[i].onTime + " to " + schedules[i].offTime);
  }
}

void handleGetSchedules() {
  String json = "{\"schedules\":[";
  
  for (int i = 0; i < 3; i++) {
    json += "{";
    json += "\"onTime\":\"" + schedules[i].onTime + "\",";
    json += "\"offTime\":\"" + schedules[i].offTime + "\",";
    json += "\"active\":";
    json += (schedules[i].isActive ? "true" : "false");
    json += "}";
    if (i < 2) json += ",";
  }
  
  json += "]}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: " + server.uri();
  message += "\nMethod: ";
  message += ((server.method() == HTTP_GET) ? "GET" : "POST");
  message += "\nArguments: " + String(server.args()) + "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}