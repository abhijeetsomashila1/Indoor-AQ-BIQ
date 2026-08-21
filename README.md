# Indoor-AQ-BIQ

ESP32-based indoor air-quality monitoring node. The firmware reads temperature,
humidity, particulate matter, carbon dioxide, and noise, calculates an AQI from
PM2.5, prints the readings to the serial monitor, and sends them to the cTOP API.

## Firmware

The Arduino sketch is located at [`Software/indoor-aq.ino`](Software/indoor-aq.ino).

### Hardware and pins

| Sensor or function | Interface | ESP32 pins |
| --- | --- | --- |
| AHT10 temperature and humidity | I2C | SDA 22, SCL 21 |
| SDS011 PM2.5 and PM10 | Hardware serial 2 | RX 17, TX 16 |
| CO2 sensor | PWM | GPIO 27 |
| Noise sensor | Analog input | GPIO 34 |

### Required libraries

- ESP32 Arduino core
- `WiFi.h`
- `HTTPClient.h`
- `ArduinoJson.h`
- `Wire.h`
- Adafruit AHTX0 library

### Runtime behavior

1. `setup()` starts the serial monitor at 115200 baud, connects to Wi-Fi, and
	 initializes the AHT10, SDS011 serial port, CO2 input, and noise ADC.
2. `loop()` samples the sensors every 5 seconds.
3. The SDS011 frame is validated with its checksum before updating PM2.5 and
	 PM10 values.
4. AQI is calculated from PM2.5 using the US EPA-style breakpoint table. AQI is
	 `-1` when PM2.5 is unavailable.
5. The readings and AQI category are printed to the serial monitor.
6. A JSON POST request is sent to the configured API endpoint.

### Sensor calculations

- **Temperature and humidity:** read directly from the AHT10.
- **PM2.5 and PM10:** read from the SDS011 frame and divided by 10 to convert
	the reported integer values to `ug/m3`.
- **CO2:** calculated from the PWM high-time using the sensor's 0 to 5000 ppm
	range. Invalid or missing pulses produce `-1`.
- **Noise:** ADC voltage is converted to an estimated dBA value using
	`voltage * 50`. Values below 30 dBA are clamped to 30.

## API payload

The sketch sends named JSON properties, so the API should map values by field
name rather than by array position. The serialized property order is:

```text
noise, aqi, pm10, pm2.5, temperature, co2, humidity
```

Example payload:

```json
{
	"noise": 59.19,
	"aqi": 54,
	"pm10": 39.5,
	"pm2.5": 13.5,
	"temperature": 25.2,
	"co₂": 966,
	"humidity": 45.6
}
```

The CO2 property is encoded by the firmware as `co₂` using `CO2_FIELD`.

The endpoint and bearer token are defined near the top of the sketch. For
deployment, replace the Wi-Fi credentials and API token with device-specific
values instead of committing credentials to source control.

## Serial monitor

Open the serial monitor at **115200 baud**. Each sample reports:

```text
Temperature : <value> C
Humidity    : <value> %
PM2.5       : <value> ug/m3
PM10        : <value> ug/m3
CO2         : <value> ppm
Noise       : <value> dBA
AQI         : <value>
```