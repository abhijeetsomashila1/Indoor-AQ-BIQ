# Indoor-AQ-BIQ

ESP32-based indoor air-quality monitoring node. The firmware reads temperature,
humidity, particulate matter, carbon dioxide, and noise; calculates an AQI from
PM2.5; displays readings on an SH1106 OLED; prints diagnostics to the serial
monitor; and sends measurements to the cTOP API.

## Firmware

The current firmware is [`Software/indoor-upd-code-oled.ino`](Software/indoor-upd-code-oled.ino).
Earlier sketches in `Software/` are retained as historical variants.

### Hardware and pins

| Sensor or function | Interface | ESP32 pins |
| --- | --- | --- |
| AHT10 temperature and humidity | I2C | SDA 22, SCL 21 |
| SH1106 OLED | I2C, address `0x3C` | SDA 22, SCL 21 |
| SDS011 PM2.5 and PM10 | Hardware serial 2 | RX 17, TX 16 |
| CO2 sensor | PWM | GPIO 27 |
| Noise sensor | Analog input | GPIO 34 |
| Configuration button | Active-low input | GPIO 0 |
| Status LED | Digital output | GPIO 2 |

The OLED is configured for 128x64 pixels with no reset pin (`-1`).

### Required libraries

- ESP32 Arduino core
- `WiFi.h`
- `HTTPClient.h`
- `ArduinoJson.h`
- `Wire.h`
- `Preferences.h`
- `WebServer.h`
- `esp_wifi.h`
- Adafruit AHTX0 library
- Adafruit GFX Library
- Adafruit SH110X library

## First-time setup

1. Flash `indoor-upd-code-oled.ino` to the ESP32.
2. Open the serial monitor at **115200 baud**.
3. If no Wi-Fi credentials are saved, the node starts an access point:
	- SSID: `AQ-Node-Setup`
	- Password: `12345678`
	- Configuration page: `http://192.168.4.1/`
4. Connect to the access point and enter the Wi-Fi SSID, Wi-Fi password, API
	URL, bearer token, and posting interval. The interval is in seconds and is
	clamped to a minimum of 10 seconds; the default is 600 seconds.
5. Submit the form. Settings are saved to ESP32 Preferences and the device
	reboots into normal operation.

To reopen the configuration portal after setup, hold the boot button on GPIO 0
for 10 seconds. The status LED blinks while provisioning mode is active.

## Runtime behavior

1. The device loads its saved configuration and attempts to connect to Wi-Fi.
2. If the connection fails, it starts the configuration access point.
3. In normal operation, sensor values are sampled for debug output every 10
	seconds and before each API post.
4. The SDS011 frame is validated with its checksum before updating PM2.5 and
	PM10 values.
5. The OLED rotates through PM2.5, PM10, CO2, temperature, humidity, and AQI,
	showing one screen every 5 seconds.
6. Measurements are posted at the configured interval. Wi-Fi is monitored and
	reconnection is attempted if the connection is lost.

### Sensor calculations

- **Temperature and humidity:** read directly from the AHT10.
- **PM2.5 and PM10:** read from the SDS011 frame and divided by 10 to convert
  the reported integer values to `ug/m3`.
- **CO2:** calculated from the PWM high-time using the sensor's 0 to 5000 ppm
  range. Invalid or missing pulses produce `-1`.
- **Noise:** ADC voltage is converted to an estimated dBA value using
  `voltage * 50`. Values below 30 dBA are clamped to 30, then averaged over
  five readings.
- **AQI:** calculated from PM2.5 using the US EPA-style breakpoint table. AQI
  is `-1` when PM2.5 is unavailable.

## API payload

The API URL and bearer token are entered through the configuration portal and
stored in Preferences. The request uses the configured token directly in the
`Authorization` header, so enter the complete value expected by the API (for
example, `Bearer <token>`).

The sketch sends named JSON properties, so the API should map values by field
name rather than by array position. Example payload:

```json
{
  "noise": 59.19,
  "aqi": 54,
  "pm10": 39.50,
  "pm2.5": 13.50,
  "temperature": 25.20,
  "co2": 966,
  "humidity": 45.60
}
```

The current sketch emits the CO2 property as `co2` in its manually constructed
request body. Invalid sensor readings are represented by `-1`.

## Serial monitor

Use **115200 baud**. The firmware prints a compact summary every 10 seconds,
including temperature, humidity, PM2.5, PM10, CO2, noise, AQI, and the AQI
category. It also prints the JSON request body and HTTP response code when a
post is attempted.

## Security and deployment

Do not commit Wi-Fi passwords or API tokens. Configuration is stored in the
ESP32's Preferences partition, and the provisioning access point uses the
default password shown above unless the firmware is changed.