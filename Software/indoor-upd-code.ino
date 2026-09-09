#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Preferences.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// =====================================================
// PINS
// =====================================================
#define AHT_SDA       22
#define AHT_SCL       21
#define SDS_RX        17
#define SDS_TX        16
#define CO2_PWM_PIN   27
#define NOISE_PIN     34
#define BUTTON_PIN    0            // Boot button (active LOW)
#define STATUS_LED    2            // Built‑in LED (GPIO2)
#define OLED_ADDR     0x3C
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET    -1

// =====================================================
// DEFAULTS (overridden by Preferences)
// =====================================================
const char* DEFAULT_SSID     = "";
const char* DEFAULT_PASSWORD = "";
const char* DEFAULT_API_URL  = "";
const char* DEFAULT_TOKEN    = "";
const uint32_t DEFAULT_INTERVAL_SEC = 600;   // 10 minutes

// AP mode credentials
const char* AP_NAME      = "AQ-Node-Setup";
const char* AP_PASSWORD  = "12345678";

// =====================================================
// OBJECTS
// =====================================================
Adafruit_AHTX0 aht;
HardwareSerial sdsSerial(2);
Preferences preferences;
WebServer webServer(80);
Adafruit_SH1106G oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// CONFIGURABLE PARAMETERS (loaded from Preferences)
// =====================================================
String wifi_ssid;
String wifi_password;
String api_url;
String bearer_token;
String co2_field = "co₂";          // fixed, can be made configurable
unsigned long post_interval_ms = DEFAULT_INTERVAL_SEC * 1000UL;

// =====================================================
// SENSOR VARIABLES
// =====================================================
float temperature = 0.0f;
float humidity    = 0.0f;
float pm25        = -1.0f;
float pm10        = -1.0f;
float noiseDBA    = 30.0f;
int   co2ppm      = -1;
bool  ahtOk       = false;
unsigned long lastSample = 0;
unsigned long lastDebugPrint = 0;   // for 10‑sec debug logs
unsigned long lastOLEDUpdate = 0;
uint8_t oledScreen = 0;
bool oledOk = false;
const unsigned long OLED_SCREEN_INTERVAL = 5000UL;

// =====================================================
// SYSTEM STATE
// =====================================================
bool provisioningMode = false;
bool wifiReady = false;
bool wifiConnecting = false;
unsigned long wifiConnectStart = 0;
const unsigned long WIFI_TIMEOUT_MS = 30000UL;

// Button handling
unsigned long buttonPressStart = 0;
bool buttonHeld = false;
const unsigned long LONG_PRESS_MS = 10000UL;

// =====================================================
// FUNCTION PROTOTYPES
// =====================================================
void loadConfig();
void saveConfig();
void connectWiFi();
void startProvisioningAP();
void handleWiFi();
void setupWebRoutes();
void initSensors();
void readAHT10();
void readNoise();
int  readCO2PWM();
bool readSDS011();
int  calculateAQI(float pm25);
String getAQICategory(int aqi);
void postData(float temp, float hum, float pm25, float pm10, int co2, float noise, int aqi);
void printSensorSummary(const char* prefix);
void initOLED();
void updateOLED();

void initOLED() {
  if (!oled.begin(OLED_ADDR, true)) {
    Serial.println("[OLED] NOT DETECTED");
    return;
  }

  oledOk = true;
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  oled.println("Indoor Air Quality");
  oled.display();
  Serial.println("[OLED] OK");
}

void updateOLED() {
  if (!oledOk || millis() - lastOLEDUpdate < OLED_SCREEN_INTERVAL) return;

  lastOLEDUpdate = millis();
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);

  switch (oledScreen) {
    case 0:
      oled.println("PM2.5");
      oled.setTextSize(2);
      oled.setCursor(0, 24);
      oled.print(pm25, 1);
      oled.println(" ug/m3");
      break;
    case 1:
      oled.println("PM10");
      oled.setTextSize(2);
      oled.setCursor(0, 24);
      oled.print(pm10, 1);
      oled.println(" ug/m3");
      break;
    case 2:
      oled.println("CO2");
      oled.setTextSize(2);
      oled.setCursor(0, 24);
      oled.print(co2ppm);
      oled.println(" ppm");
      break;
    case 3:
      oled.println("Temperature");
      oled.setTextSize(2);
      oled.setCursor(0, 24);
      oled.print(temperature, 1);
      oled.println(" C");
      break;
    case 4:
      oled.println("Humidity");
      oled.setTextSize(2);
      oled.setCursor(0, 24);
      oled.print(humidity, 1);
      oled.println(" %");
      break;
    default:
      int aqi = pm25 >= 0.0f ? calculateAQI(pm25) : -1;
      oled.println("AQI");
      oled.setTextSize(2);
      oled.setCursor(0, 24);
      oled.print(aqi);
      break;
  }

  oled.display();
  oledScreen = (oledScreen + 1) % 6;
}

// =====================================================
// SENSOR INITIALISATION
// =====================================================
void initSensors() {
  Wire.begin(AHT_SDA, AHT_SCL);
  initOLED();
  if ((ahtOk = aht.begin())) {
    Serial.println("AHT10 OK");
  } else {
    Serial.println("AHT10 NOT DETECTED");
  }
  sdsSerial.begin(9600, SERIAL_8N1, SDS_RX, SDS_TX);
  pinMode(CO2_PWM_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(NOISE_PIN, ADC_11db);
  Serial.println("Sensors initialised.");
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("AIR QUALITY NODE (Provisioning)");
  Serial.println("========================================");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);   // off initially

  // Load saved config
  preferences.begin("aqconfig", false);
  loadConfig();

  // If no SSID saved, directly start AP mode
  if (wifi_ssid.isEmpty()) {
    Serial.println("No SSID saved. Starting configuration portal...");
    startProvisioningAP();
    return;
  }

  // Try to connect with saved credentials
  connectWiFi();

  // If connection failed, start AP mode
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed. Starting configuration portal...");
    startProvisioningAP();
    return;
  }

  // Normal operation – init sensors
  initSensors();
  Serial.println("Ready for normal operation.");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  // ------------------------------------------------
  // Check for long button press (10 sec) to enter AP
  // ------------------------------------------------
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonHeld) {
      buttonPressStart = millis();
      buttonHeld = true;
    }
    if (millis() - buttonPressStart >= LONG_PRESS_MS) {
      Serial.println("Button held 10s – starting configuration portal...");
      startProvisioningAP();
      // After this, we stay in AP mode; never return
      while (1) {
        webServer.handleClient();
        delay(10);
      }
    }
  } else {
    buttonHeld = false;
  }

  // ------------------------------------------------
  // If in provisioning mode, just handle web server
  // ------------------------------------------------
  if (provisioningMode) {
    webServer.handleClient();
    // Blink LED to indicate AP mode
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      ledState = !ledState;
      digitalWrite(STATUS_LED, ledState);
    }
    return;
  }

  // ------------------------------------------------
  // Normal operation: maintain Wi‑Fi, read sensors, post
  // ------------------------------------------------
  handleWiFi();

  // If Wi‑Fi is ready, handle sampling and debug
  if (wifiReady) {
    // --- Debug print every 10 seconds ---
    if (millis() - lastDebugPrint >= 10000UL) {
      lastDebugPrint = millis();
      // Read sensors to get latest values
      if (ahtOk) readAHT10();
      readSDS011();
      readNoise();
      co2ppm = readCO2PWM();
      printSensorSummary("[DEBUG]");
    }

    // --- Regular posting at configured interval ---
    if (millis() - lastSample >= post_interval_ms) {
      lastSample = millis();

      // Read sensors (already read by debug, but we re‑read to ensure fresh data)
      if (ahtOk) readAHT10();
      readSDS011();
      readNoise();
      co2ppm = readCO2PWM();

      int aqi = -1;
      if (pm25 >= 0.0f) aqi = calculateAQI(pm25);

      // Print summary without prefix (regular post)
      printSensorSummary("");

      // Post data
      postData(temperature, humidity, pm25, pm10, co2ppm, noiseDBA, aqi);
    }
  }

  updateOLED();

  // Small delay to prevent watchdog
  delay(10);
}

// =====================================================
// PRINT SENSOR SUMMARY (with optional prefix)
// =====================================================
void printSensorSummary(const char* prefix) {
  int aqi = -1;
  if (pm25 >= 0.0f) aqi = calculateAQI(pm25);

  Serial.println("\n========================================");
  if (strlen(prefix) > 0) {
    Serial.printf("%s ", prefix);
  }
  Serial.printf("Temp: %.2f °C   Hum: %.2f %%\n", temperature, humidity);
  Serial.printf("PM2.5: %.1f µg/m³   PM10: %.1f µg/m³\n", pm25, pm10);
  Serial.printf("CO2: %d ppm   Noise: %.1f dBA\n", co2ppm, noiseDBA);
  Serial.printf("AQI: %d (%s)\n", aqi, (aqi >= 0) ? getAQICategory(aqi).c_str() : "N/A");
  Serial.println("========================================");
}

// =====================================================
// CONFIGURATION STORAGE (Preferences)
// =====================================================
void loadConfig() {
  wifi_ssid       = preferences.getString("ssid", "");
  wifi_password   = preferences.getString("pass", "");
  api_url         = preferences.getString("apiurl", "");
  bearer_token    = preferences.getString("token", "");

  // CRITICAL: Remove any accidental whitespace
  wifi_ssid.trim();
  wifi_password.trim();
  api_url.trim();
  bearer_token.trim();

  uint32_t intervalSec = preferences.getUInt("interval", DEFAULT_INTERVAL_SEC);
  if (intervalSec < 10) intervalSec = 10;
  post_interval_ms = intervalSec * 1000UL;

  Serial.println("Loaded configuration:");
  Serial.println("  SSID: " + (wifi_ssid.isEmpty() ? "<not set>" : wifi_ssid));
  Serial.println("  API URL: " + (api_url.isEmpty() ? "<not set>" : api_url));
  Serial.print("  Token length: ");
  Serial.println(bearer_token.length());  // helpful for debugging
  Serial.println("  Interval: " + String(post_interval_ms / 1000) + " s");
}

void saveConfig() {
  // Trim again just in case
  wifi_ssid.trim();
  wifi_password.trim();
  api_url.trim();
  bearer_token.trim();

  preferences.putString("ssid", wifi_ssid);
  preferences.putString("pass", wifi_password);
  preferences.putString("apiurl", api_url);
  preferences.putString("token", bearer_token);
  preferences.putUInt("interval", post_interval_ms / 1000);
  preferences.end();

  Serial.println("Configuration saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

// =====================================================
// WIFI CONNECTION
// =====================================================
void connectWiFi() {
  if (wifi_ssid.isEmpty()) {
    Serial.println("No SSID saved.");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  wifiConnecting = true;
  wifiConnectStart = millis();
  Serial.print("Connecting to Wi‑Fi");
  while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStart < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  wifiConnecting = false;
  if (WiFi.status() == WL_CONNECTED) {
    wifiReady = true;
    Serial.println("\nWiFi Connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiReady = false;
    Serial.println("\nWiFi connection failed.");
  }
}

void handleWiFi() {
  if (provisioningMode) return;
  if (wifiReady) {
    if (WiFi.status() == WL_CONNECTED) return;
    // Lost connection – try to reconnect
    Serial.println("[WiFi] Connection lost – reconnecting...");
    wifiReady = false;
    wifiConnecting = false;
    return;
  }

  if (!wifiConnecting && !wifi_ssid.isEmpty()) {
    connectWiFi();
  }
}

// =====================================================
// PROVISIONING AP MODE
// =====================================================
void startProvisioningAP() {
  provisioningMode = true;
  wifiReady = false;
  wifiConnecting = false;

  WiFi.mode(WIFI_AP);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.softAP(AP_NAME, AP_PASSWORD);
  esp_wifi_set_max_tx_power(WIFI_POWER_11dBm);

  Serial.printf("[AP] Started: %s (IP: %s)\n", AP_NAME, WiFi.softAPIP().toString().c_str());
  setupWebRoutes();
  webServer.begin();
  Serial.println("[AP] Connect to this AP and open http://192.168.4.1/ to configure.");
}

// =====================================================
// WEB PAGE (HTML) – stored in PROGMEM
// =====================================================
const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Air Quality Node Config</title>
<style>
body { font-family: Arial; background: #f4f4f4; margin: 20px; }
.container { max-width: 600px; margin: auto; background: white; padding: 20px; border-radius: 8px; }
h2 { margin-top: 0; }
label { display: block; margin: 10px 0 5px; font-weight: bold; }
input { width: 100%; padding: 8px; box-sizing: border-box; }
.btn { background: #222; color: white; border: none; padding: 12px; width: 100%; border-radius: 4px; cursor: pointer; font-size: 16px; }
</style>
</head>
<body>
<div class="container">
<h2>ESP32 Air Quality Node</h2>
<form action="/save" method="POST">
    <h3>WiFi</h3>
    <label>SSID</label>
    <input type="text" name="ssid" required>
    <label>Password</label>
    <input type="password" name="password">

    <h3>Cloud API</h3>
    <label>API URL</label>
    <input type="text" name="apiurl" placeholder="http://...">
    <label>Bearer Token</label>
    <input type="text" name="token" placeholder="your_token_here">

    <h3>Timing</h3>
    <label>Post Interval (seconds)</label>
    <input type="number" name="interval" placeholder="600" min="10">

    <button type="submit" class="btn">Save & Reboot</button>
</form>
<p id="status"></p>
</div>
<script>
window.onload = function() {
    fetch('/config')
      .then(res => res.json())
      .then(data => {
          document.querySelector('input[name="ssid"]').value = data.ssid || '';
          document.querySelector('input[name="password"]').value = data.password || '';
          document.querySelector('input[name="apiurl"]').value = data.apiurl || '';
          document.querySelector('input[name="token"]').value = data.token || '';
          document.querySelector('input[name="interval"]').value = data.interval || '';
      })
      .catch(() => {});
};
</script>
</body>
</html>
)rawliteral";

// =====================================================
// WEB ROUTES
// =====================================================
void setupWebRoutes() {
  webServer.on("/", HTTP_GET, []() {
    webServer.send(200, "text/html", CONFIG_PAGE);
  });

  webServer.on("/config", HTTP_GET, []() {
    JsonDocument doc;
    doc["ssid"] = preferences.getString("ssid", "");
    doc["password"] = preferences.getString("pass", "");
    doc["apiurl"] = preferences.getString("apiurl", "");
    doc["token"] = preferences.getString("token", "");
    doc["interval"] = preferences.getUInt("interval", DEFAULT_INTERVAL_SEC);
    String json;
    serializeJson(doc, json);
    webServer.send(200, "application/json", json);
  });

  webServer.on("/save", HTTP_POST, []() {
    String ssid = webServer.arg("ssid");
    String password = webServer.arg("password");
    String apiurl = webServer.arg("apiurl");
    String token = webServer.arg("token");
    String intervalStr = webServer.arg("interval");

    if (ssid.isEmpty()) {
      webServer.send(400, "text/plain", "SSID is required");
      return;
    }

    // Trim before saving
    ssid.trim();
    password.trim();
    apiurl.trim();
    token.trim();

    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.putString("apiurl", apiurl);
    preferences.putString("token", token);
    uint32_t intervalSec = intervalStr.isEmpty() ? DEFAULT_INTERVAL_SEC : intervalStr.toInt();
    if (intervalSec < 10) intervalSec = 10;
    preferences.putUInt("interval", intervalSec);
    preferences.end();

    webServer.send(200, "text/html", "<h2>Settings saved.</h2><p>Rebooting...</p>");
    delay(1000);
    ESP.restart();
  });
}

// =====================================================
// SENSOR READING FUNCTIONS
// =====================================================

void readAHT10() {
  sensors_event_t hum, temp;
  aht.getEvent(&hum, &temp);
  temperature = temp.temperature;
  humidity    = hum.relative_humidity;
}

// Noise with moving average
#define NOISE_SAMPLES 5
float noiseBuffer[NOISE_SAMPLES] = {0};
int noiseIndex = 0;
float noiseSum = 0;

void readNoise() {
  int raw = analogRead(NOISE_PIN);
  float voltage = (raw / 4095.0f) * 3.3f;
  float dBA = voltage * 50.0f;
  if (dBA < 30.0f) dBA = 30.0f;

  noiseSum -= noiseBuffer[noiseIndex];
  noiseBuffer[noiseIndex] = dBA;
  noiseSum += dBA;
  noiseIndex = (noiseIndex + 1) % NOISE_SAMPLES;
  noiseDBA = noiseSum / NOISE_SAMPLES;
}

int readCO2PWM() {
  unsigned long highTime = pulseIn(CO2_PWM_PIN, HIGH, 2000000UL);
  if (highTime == 0) return -1;
  float highMs = highTime / 1000.0f;
  if (highMs < 2.0f || highMs > 1002.0f) return -1;
  return (int)(5000.0f * (highMs - 2.0f) / 1000.0f);
}

bool readSDS011() {
  unsigned long startTime = millis();
  while (sdsSerial.available() < 1) {
    if (millis() - startTime > 500) return false;
  }
  if (sdsSerial.read() != 0xAA) {
    while (sdsSerial.available()) sdsSerial.read();
    return false;
  }
  uint8_t buf[10];
  buf[0] = 0xAA;
  for (int i = 1; i < 10; i++) {
    unsigned long byteStart = millis();
    while (!sdsSerial.available()) {
      if (millis() - byteStart > 100) return false;
    }
    buf[i] = sdsSerial.read();
  }
  if (buf[1] != 0xC0) return false;
  uint8_t checksum = 0;
  for (int i = 2; i <= 7; i++) checksum += buf[i];
  if (checksum != buf[8]) return false;
  pm25 = (((uint16_t)buf[3] << 8) | buf[2]) / 10.0f;
  pm10 = (((uint16_t)buf[5] << 8) | buf[4]) / 10.0f;
  return true;
}

int calculateAQI(float pm25) {
  struct AQIBreakpoint {
    float cLow, cHigh;
    int iLow, iHigh;
  };
  static const AQIBreakpoint table[] = {
    {0.0f,   12.0f,  0,   50},
    {12.1f,  35.4f,  51,  100},
    {35.5f,  55.4f,  101, 150},
    {55.5f,  150.4f, 151, 200},
    {150.5f, 250.4f, 201, 300},
    {250.5f, 350.4f, 301, 400},
    {350.5f, 500.4f, 401, 500}
  };
  for (const auto& bp : table) {
    if (pm25 >= bp.cLow && pm25 <= bp.cHigh) {
      return round(
        ((float)(bp.iHigh - bp.iLow) / (bp.cHigh - bp.cLow)) *
        (pm25 - bp.cLow) + bp.iLow
      );
    }
  }
  return 500;
}

String getAQICategory(int aqi) {
  if (aqi <= 50)   return "Good";
  if (aqi <= 100)  return "Moderate";
  if (aqi <= 150)  return "Unhealthy for Sensitive";
  if (aqi <= 200)  return "Unhealthy";
  if (aqi <= 300)  return "Very Unhealthy";
  return "Hazardous";
}

// =====================================================
// HTTP POST (uses loaded api_url and token)
// =====================================================
void postData(float temperature, float humidity, float pm25, float pm10,
              int co2, float noise, int aqi) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost – skipping post");
    return;
  }
  if (api_url.isEmpty()) {
    Serial.println("API URL not configured – skipping post");
    return;
  }
  if (bearer_token.isEmpty()) {
    Serial.println("Bearer token not configured – skipping post");
    return;
  }

  // Round to 2 decimals
  float tempRounded  = round(temperature * 100.0) / 100.0;
  float humRounded   = round(humidity * 100.0) / 100.0;
  float pm25Rounded  = round(pm25 * 100.0) / 100.0;
  float pm10Rounded  = round(pm10 * 100.0) / 100.0;
  float noiseRounded = round(noise * 100.0) / 100.0;

  // Build JSON manually with 2‑decimal formatting for all floats
  String requestBody = "{";
  requestBody += "\"noise\":" + String(noiseRounded, 2) + ",";
  requestBody += "\"aqi\":" + String(aqi) + ",";
  requestBody += "\"pm10\":" + String(pm10Rounded, 2) + ",";
  requestBody += "\"pm2.5\":" + String(pm25Rounded, 2) + ",";
  requestBody += "\"temperature\":" + String(tempRounded, 2) + ",";
  requestBody += "\"co₂\":" + String(co2) + ",";
  requestBody += "\"humidity\":" + String(humRounded, 2);
  requestBody += "}";

  HTTPClient http;
  http.begin(api_url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", bearer_token);

  Serial.print("Request body: ");
  Serial.println(requestBody);

  int responseCode = http.POST(requestBody);
  Serial.print("HTTP Response: ");
  Serial.println(responseCode);
  if (responseCode > 0) {
    String response = http.getString();
    if (response.length() > 0) Serial.println(response);
  }
  http.end();
}