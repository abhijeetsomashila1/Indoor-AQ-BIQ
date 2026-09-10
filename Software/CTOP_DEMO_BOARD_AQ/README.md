# CTOP_DEMO_BOARD_AQ

Air Quality demo project for CTOP hardware.

## Purpose

This demo reads and publishes air quality sensors (CO2, particulate matter, temperature/humidity, gas) and displays information on an OLED display. It includes handlers for OM2M and Wi‑Fi connectivity.

## Key files

- `CTOP_DEMO_BOARD_AQ.ino` - Main Arduino sketch to build and upload.
- `co2_sensor.h` - CO2 sensor helper functions.
- `pm_sensor.h` - Particulate matter sensor helper functions.
- `sht_sgp_handler.h` - Temperature/humidity and gas sensor handling.
- `oled_handler.h` - OLED display helper functions.
- `wifi_handler.h` - Wi‑Fi setup and reconnect logic.
- `om2mHandler.h` - OM2M-specific messaging and publishing functions.
- `timeHandler.h` - Time synchronisation utilities.

## Hardware Requirements

### Components
- ESP32 development board
- CO2 sensor 
- PM sensor 
- SHT3x/SHT4x temperature & humidity sensor
- SGP30/SGP40 gas sensor (TVOC, eCO2)
- OLED display 128x64 (SSD1306)


### Libraries Required

Install these via Arduino Library Manager or PlatformIO:
- **Adafruit SSD1306** - OLED display driver
- **Adafruit GFX Library** - Graphics library for display
- **Sensirion I2C SHT** - Temperature/humidity sensor
- **Sensirion SGP** - Gas/VOC sensor
- **WiFi** - Built-in ESP32 library
- **HTTPClient** - Built-in ESP32 library for HTTPS
- **ArduinoJson** - JSON serialization/deserialization
- **TimeLib** or **NTPClient** - For time synchronization

## Wiring

### Pin Connections (ESP32)

| Component | ESP32 Pin | Notes |
|-----------|-----------|-------|
| **I2C Sensors** | | |
| SDA (OLED, SHT, SGP) | GPIO 21 | Default I2C SDA |
| SCL (OLED, SHT, SGP) | GPIO 22 | Default I2C SCL |
| **CO2 Sensor** | | |
| Analog/Digital Out | GPIO 34 (ADC) | CO2 level reading |
| **PM Sensor** | | |
| RX | GPIO 16 | Connect to PM sensor TX |
| TX | GPIO 17 | Connect to PM sensor RX |
| **Trigger Pin** | GPIO 12 | Optional trigger control |
| **Power** | | |
| All sensors VCC | 3.3V | **Do not use 5V for I2C sensors** |
| All sensors GND | GND | Common ground |

**Note:** Pin assignments may vary. Check your specific code in the `.ino` file for actual pin definitions.

### I2C Addresses
- OLED: 0x3C or 0x3D
- SHT sensor: 0x44 or 0x45
- SGP sensor: 0x58

Use an I2C scanner to verify addresses if sensors are not detected.

## Build & Upload

Open `CTOP_DEMO_BOARD_AQ.ino` in the Arduino IDE or import into PlatformIO and upload to an ESP32-compatible board.

## Configuration

### Wi-Fi Settings

Edit `wifi_handler.h` or the main `.ino` file:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### OM2M Server

Edit `constants.h`:

```cpp
#define CSE_IP "onem2m.iiit.ac.in"
#define CSE_PORT 443
#define OM2M_ORGIN "AQSRMon@20:psX9MSnnrvyH"
#define OM2M_AE "AE-SR/SR-AQ"
#define OM2M_Node_ID "SR-AQ-KH95-01"
#define OM2M_DATA_CONT "SR-AQ-KH95-01/Data"
```

### Sensor Calibration

- **CO2 sensor:** May require 24-hour warm-up for accurate readings
- **PM sensor:** Allow 30 seconds warm-up after power-on
- **SGP sensor:** Baseline calibration improves over first 12 hours

## Usage

1. **Power on** - Device connects to Wi-Fi
2. **Initialization** - Sensors warm up (wait for "Ready" on OLED)
3. **Data collection** - Readings taken every 10-60 seconds (configurable)
4. **Display** - Current values shown on OLED
5. **Transmission** - Data sent to OM2M server

### Serial Monitor Output

Connect at **115200 baud** to see:
- Wi-Fi connection status
- Sensor readings (raw values)
- HTTP POST responses
- Error messages and debug info

## Troubleshooting

### Sensor Issues

**CO2 sensor not responding:**
- Check UART connections (TX ↔ RX crossover)
- Verify baud rate (usually 9600)
- Ensure sensor has adequate warm-up time

**PM sensor shows 0 values:**
- Check UART wiring
- Verify sensor is powered (fan should run)
- Wait for 30-second initialization

**I2C sensors not detected:**
- Run I2C scanner sketch
- Check SDA/SCL wiring
- Verify pull-up resistors (usually built-in)
- Ensure 3.3V power (not 5V)

**OLED display issues:**
- Verify I2C address (0x3C or 0x3D)
- Check contrast settings in code
- Test with simple display example first

### Network Issues

**Wi-Fi won't connect:**
- Verify SSID and password
- Check 2.4GHz network (ESP32 doesn't support 5GHz)
- Monitor serial output for error codes
- Try restarting router

**OM2M server unreachable:**
- Ping server to verify network connectivity
- Check firewall settings
- Verify server is running on configured port
- Review server logs

### General

- **Use serial monitor** (115200 baud) for debug output
- **Check library versions** - update to latest compatible versions
- **Power supply** - ensure adequate current (500mA+ recommended)
- **Reset** - press EN/RST button to restart if frozen
