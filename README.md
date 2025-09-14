# NodeMCU Scheduled Timer Controller

A web-based timer controller system built on NodeMCU (ESP8266) that provides automated scheduling for up to 3 digital pins with real-time monitoring and configuration through a responsive web interface.

## 🎯 Features

### Core Functionality
- **Real-time IST Clock Display**: NTP-synchronized 24-hour format time display
- **3-Channel Scheduled Control**: Independent timing control for pins D0, D1, D2
- **Web-based Configuration**: Responsive dark-themed web interface
- **Persistent Storage**: Schedules saved in EEPROM (survives power cycles)
- **Status Monitoring**: Real-time display of pin states and schedules
- **WiFi Status Indicator**: Onboard LED shows connection status

### Hardware Control
- **Pin D0 (GPIO16)**: Schedule 1 control output
- **Pin D1 (GPIO5)**: Schedule 2 control output  
- **Pin D2 (GPIO4)**: Schedule 3 control output
- **Onboard LED (GPIO2)**: WiFi status indicator
- **Active LOW Logic**: Suitable for relay modules (LOW = ON, HIGH = OFF)

## 🔧 Technical Specifications

### Hardware Requirements
- **NodeMCU ESP8266** (ESP-12E Module)
- **WiFi Network** (2.4GHz)
- **Power Supply** (5V via USB or 3.3V direct)

### Software Dependencies
```cpp
#include <ESP8266WiFi.h>        // WiFi connectivity
#include <ESP8266WebServer.h>   // Web server functionality
#include <WiFiUdp.h>            // UDP for NTP
#include <NTPClient.h>          // Network Time Protocol
#include <TimeLib.h>            // Time manipulation
#include <EEPROM.h>             // Persistent storage
```

### Memory Usage
- **EEPROM**: 512 bytes allocated (63 bytes used for schedules)
- **Flash**: ~400KB program size
- **RAM**: ~40KB runtime usage

## 🌐 Network Configuration

### API Endpoints
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main dashboard interface |
| `/schedule` | GET | Schedule configuration page |
| `/time` | GET | JSON time data |
| `/set-schedule` | POST | Save schedule configuration |
| `/get-schedules` | GET | Retrieve current schedules |

### JSON Response Format
```json
{
  "time": "15:30:45",
  "date": "14/09/2025", 
  "day": "Sunday"
}
```

## 📋 Installation & Setup

### 1. Arduino IDE Configuration
```
Board: NodeMCU 1.0 (ESP-12E Module)
CPU Frequency: 80 MHz
Flash Size: 4M (1M SPIFFS)
Upload Speed: 115200
```

### 2. Library Installation
Install via Arduino Library Manager:
- `NTPClient` by Fabrice Weinberg
- `Time` by Michael Margolis

### 3. Code Configuration
Update WiFi credentials in the code:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### 4. Upload & Access
1. Upload code to NodeMCU
2. Open Serial Monitor (115200 baud)
3. Note the assigned IP address
4. Access web interface at `http://[IP_ADDRESS]`

## 🔄 Code Workflow

### System Initialization
```
1. Pin Configuration (OUTPUT modes)
2. EEPROM Initialization (load saved schedules)
3. WiFi Connection Establishment  
4. NTP Client Setup (IST timezone)
5. Web Server Route Registration
6. Server Start & IP Display
```

### Main Loop Operations
```
1. Handle Web Client Requests
2. LED Status Management (blink if connected)
3. Time Update (every 1 second)
4. Schedule Checking & Pin Control
5. EEPROM Updates (when schedules change)
```

### Schedule Logic Flow
```cpp
Current Time (HH:MM) → Time Comparison → Pin State Decision

if (current_time >= on_time && current_time < off_time) {
    pin_state = LOW;  // Device ON
} else {
    pin_state = HIGH; // Device OFF
}
```

### Cross-Midnight Handling
For schedules spanning midnight (e.g., 19:15 to 06:30):
```cpp
if (on_time > off_time) {
    // Crosses midnight
    active = (current >= on_time || current < off_time);
} else {
    // Same day
    active = (current >= on_time && current < off_time);
}
```

## 💾 Data Storage Structure

### EEPROM Layout (63 bytes total)
```
Schedule 1: [0-20]   → On Time(10) + Off Time(10) + Active State(1)
Schedule 2: [21-41]  → On Time(10) + Off Time(10) + Active State(1)  
Schedule 3: [42-62]  → On Time(10) + Off Time(10) + Active State(1)
```

## 🎨 Web Interface Features

### Dashboard (`/`)
- **Real-time Clock**: Large 24-hour format display
- **Date Display**: DD/MM/YYYY format with day name
- **Schedule Status**: Live pin states with color coding
- **Quick Actions**: Refresh and settings navigation

### Schedule Configuration (`/schedule`)
- **Time Input Fields**: HTML5 time pickers for each schedule
- **Save Functionality**: POST form with validation
- **Responsive Design**: Mobile-friendly interface
- **Status Feedback**: Success/error notifications

### Design Specifications
- **Color Scheme**: Dark background (`#1a1a1a`) with light text
- **Accent Colors**: Green (`#00ff88`) for active, Orange (`#ffaa00`) for inactive
- **Typography**: Segoe UI font family, monospace for time display
- **Layout**: CSS Grid and Flexbox for responsiveness

## 🔌 Pin Control Logic

### Timing Precision
- **Update Frequency**: 1-second intervals
- **Time Format**: 24-hour (HH:MM comparison)
- **Timezone**: IST (UTC+5:30) via NTP offset

### Hardware Interface
```
Digital Pin → Relay Module → Load Device
     ↓              ↓            ↓
   LOW State    →  Relay ON  →  Device ON
   HIGH State   →  Relay OFF →  Device OFF
```

### Status Indicators
- **Onboard LED Blinking**: WiFi connected & operating normally  
- **Onboard LED Solid ON**: WiFi disconnected or startup
- **Web Dashboard**: Real-time schedule status display

## 🔧 Troubleshooting

### Common Issues
1. **WiFi Connection Failed**: Check SSID/password, signal strength
2. **Time Not Updating**: Verify internet connection, NTP access
3. **Schedules Not Working**: Check time format (HH:MM), EEPROM corruption
4. **Web Interface Not Loading**: Confirm IP address, firewall settings

### Debug Information
Serial Monitor output includes:
- WiFi connection status
- IP address assignment  
- NTP synchronization status
- Schedule activation/deactivation events
- EEPROM read/write operations

## 📱 Mobile Compatibility

The web interface is fully responsive and supports:
- **iOS Safari**: Native time picker support
- **Android Chrome**: Material Design time inputs  
- **Desktop Browsers**: All modern browsers supported
- **Touch Interface**: Optimized button sizes and spacing

## ⚡ Power Consumption

### Operational Modes
- **Active WiFi**: ~80mA @ 3.3V
- **Deep Sleep**: ~20μA @ 3.3V (not implemented)
- **Pin Output**: ~20mA per active pin (depending on load)

### Recommended Power Supply
- **USB Power**: 5V/1A adapter (most reliable)
- **Battery Operation**: 3.7V Li-ion with voltage regulator
- **External DC**: 7-12V with onboard regulator

## 📊 Performance Metrics

- **Boot Time**: ~3-5 seconds to full operation
- **Web Response**: <100ms for dashboard updates  
- **Time Accuracy**: ±1 second (NTP synchronized)
- **Schedule Precision**: 1-minute resolution
- **Uptime**: Tested >30 days continuous operation

## 🔐 Security Considerations

- **Local Network Only**: No internet exposure by default
- **No Authentication**: Open access on local network
- **Plain HTTP**: No HTTPS encryption
- **Recommended**: Use on isolated IoT network or add authentication layer

## 📈 Future Enhancement Possibilities

- **HTTPS Support**: SSL certificate integration
- **User Authentication**: Login system implementation  
- **Mobile App**: Native iOS/Android applications
- **MQTT Integration**: Home automation protocol support
- **Multiple Timezone**: Support for different time zones
- **Advanced Scheduling**: Weekly/monthly patterns
- **Data Logging**: Schedule execution history
- **OTA Updates**: GitHub-based automatic updates

---

**Author**: NodeMCU Timer Controller Project  
**Version**: 1.0.0  
**Last Updated**: September 2025  
**License**: Open Source