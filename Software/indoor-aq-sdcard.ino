#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <RTClib.h>
#include <Adafruit_AHTX0.h>
#include <time.h>

// =====================================================
// SD CARD
// =====================================================
#define SD_CS 5
const char* LOG_FILE = "/aq_log.csv";
const unsigned long SAMPLE_INTERVAL = 60UL * 1000UL;   // 1 minute

// =====================================================
// PINS
// =====================================================
#define AHT_SDA       22
#define AHT_SCL       21
#define SDS_RX        17
#define SDS_TX        16
#define CO2_PWM_PIN   27
#define NOISE_PIN     34
#define BUTTON_PIN    0        // Boot button (active LOW)
#define STATUS_LED    2        // Built-in LED (used in AP mode)
#define WIFI_LED_PIN  25       // Wi-Fi indicator LED

// =====================================================
// PROVISIONING
// =====================================================
const char* AP_NAME     = "AQ-Node-Setup";
// const char* AP_PASSWORD = "12345678";
const unsigned long LONG_PRESS_MS   = 10000UL;
const unsigned long WIFI_TIMEOUT_MS = 30000UL;

// NTP / Wi-Fi time sync (used once at boot to set the RTC)
const char* NTP_SERVER      = "pool.ntp.org";
const long  GMT_OFFSET_SEC  = 19800;   // IST (+5:30)
const int   DAYLIGHT_OFFSET = 0;

// =====================================================
// OBJECTS
// =====================================================
Adafruit_AHTX0 aht;
RTC_DS3231 rtc;
HardwareSerial sdsSerial(2);
Preferences    preferences;
WebServer      webServer(80);

// =====================================================
// STATE
// =====================================================
String wifi_ssid;
String wifi_password;

bool provisioningMode = false;
bool wifiReady        = false;
bool rtcAvailable     = false;

float temperature = 0;
float humidity    = 0;
float pm25        = -1;
float pm10        = -1;
float noiseDBA    = 0;

unsigned long lastSample        = 0;
unsigned long buttonPressStart  = 0;
bool          buttonHeld        = false;

// =====================================================
// FUNCTION PROTOTYPES
// =====================================================
void loadWiFiConfig();
void saveWiFiConfig(const String& ssid, const String& pass);
void connectWiFi();
bool syncRTCFromWiFi();
void startProvisioningAP();
void setupWebRoutes();
String formatTimestamp();
void updateWiFiLED();
void updateStatusLED();
void readAHT10();
void readNoise();
int  readCO2PWM();
bool readSDS011();
int  calculateAQI(float pm25);
String getAQICategory(int aqi);
void logToSD(float temperature, float humidity, float pm25, float pm10,
             int co2, float noise, int aqi);

// =====================================================
// WIFI INDICATOR LED
//   - AP mode            → OFF
//   - Connected          → BLINK (500 ms)
//   - Connecting / fail  → SOLID ON
// =====================================================
void updateWiFiLED() {
  if (provisioningMode) {
    digitalWrite(WIFI_LED_PIN, LOW);
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Connected → BLINK
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    if (millis() - lastBlink >= 500) {
      lastBlink = millis();
      ledState  = !ledState;
      digitalWrite(WIFI_LED_PIN, ledState);
    }
    return;
  }

  // Not connected / connecting / failed → SOLID ON
  digitalWrite(WIFI_LED_PIN, HIGH);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("AIR QUALITY NODE (NTP + SD)");
  Serial.println("================================");

  // Pins
  pinMode(BUTTON_PIN,   INPUT_PULLUP);
  pinMode(STATUS_LED,   OUTPUT);
  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED,   LOW);
  digitalWrite(WIFI_LED_PIN, LOW);

  // Load saved Wi-Fi credentials
  preferences.begin("aqwifi", false);
  loadWiFiConfig();

  // No creds → AP mode
  if (wifi_ssid.isEmpty()) {
    Serial.println("No Wi-Fi credentials saved. Starting AP mode...");
    startProvisioningAP();
    return;
  }

  // Try connecting
  connectWiFi();

  // If failed → AP mode
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi failed. Starting AP mode...");
    startProvisioningAP();
    return;
  }

  // Init RTC
  Wire.begin(AHT_SDA, AHT_SCL);
  if (!rtc.begin()) {
    rtcAvailable = false;
    Serial.println("RTC NOT FOUND");
  } else {
    rtcAvailable = true;
    Serial.println("RTC OK");

    if (rtc.lostPower()) {
      Serial.println("RTC LOST POWER");
    }

    DateTime now = rtc.now();
    Serial.printf("RTC Date/Time before sync: %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second());

    // Sync RTC from Wi-Fi/NTP every boot so the RTC starts from the correct time
    syncRTCFromWiFi();
  }

  // Init sensors
  if (aht.begin()) Serial.println("AHT10 OK");
  else             Serial.println("AHT10 NOT DETECTED");

  sdsSerial.begin(9600, SERIAL_8N1, SDS_RX, SDS_TX);

  pinMode(CO2_PWM_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(NOISE_PIN, ADC_11db);

  // SD card
  SPI.begin(18, 19, 23, SD_CS);
  if (SD.begin(SD_CS)) {
    Serial.println("SD Card OK");
    if (!SD.exists(LOG_FILE)) {
      File logFile = SD.open(LOG_FILE, FILE_WRITE);
      if (logFile) {
        logFile.println("Timestamp,Temperature,Humidity,PM2.5,PM10,CO2,Noise,AQI");
        logFile.close();
      }
    }
  } else {
    Serial.println("SD Card FAILED");
  }

  Serial.println("Setup Complete");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  // Wi-Fi LED status update
  updateWiFiLED();

  // ---------- BOOT button long-press → AP mode ----------
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonHeld) {
      buttonPressStart = millis();
      buttonHeld = true;
    }
    if (millis() - buttonPressStart >= LONG_PRESS_MS) {
      Serial.println("BOOT held 10s – entering AP mode...");
      startProvisioningAP();
      // Stay in AP mode forever (until reboot via web form)
      while (1) {
        updateWiFiLED();     // keeps Wi-Fi LED off in AP mode
        updateStatusLED();   // blinks built-in LED
        webServer.handleClient();
        delay(10);
      }
    }
  } else {
    buttonHeld = false;
  }

  // ---------- If already in AP mode ----------
  if (provisioningMode) {
    updateStatusLED();
    webServer.handleClient();
    delay(10);
    return;
  }

  // ---------- Regular sampling ----------
  if (millis() - lastSample >= SAMPLE_INTERVAL) {
    lastSample = millis();

    readAHT10();
    readSDS011();
    readNoise();
    int co2ppm = readCO2PWM();
    int aqi = -1;
    if (pm25 >= 0) aqi = calculateAQI(pm25);

    String timestamp = formatTimestamp();

    Serial.println();
    Serial.println("================================");
    Serial.println("Timestamp   : " + timestamp);
    Serial.printf("Temperature : %.2f C\n", temperature);
    Serial.printf("Humidity    : %.2f %%\n", humidity);
    Serial.printf("PM2.5       : %.1f ug/m3\n", pm25);
    Serial.printf("PM10        : %.1f ug/m3\n", pm10);
    Serial.printf("CO2         : %d ppm\n", co2ppm);
    Serial.printf("Noise       : %.1f dBA\n", noiseDBA);
    Serial.printf("AQI         : %d (%s)\n",
                  aqi, (aqi >= 0) ? getAQICategory(aqi).c_str() : "N/A");
    Serial.println("================================");

    logToSD(temperature, humidity, pm25, pm10, co2ppm, noiseDBA, aqi);
  }

  delay(5);
}

// =====================================================
// STATUS LED (AP mode blink)
// =====================================================
void updateStatusLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    ledState  = !ledState;
    digitalWrite(STATUS_LED, ledState);
  }
}

// =====================================================
// WI-FI CONFIG STORAGE
// =====================================================
void loadWiFiConfig() {
  wifi_ssid     = preferences.getString("ssid", "");
  wifi_password = preferences.getString("pass", "");
  wifi_ssid.trim();
  wifi_password.trim();
  Serial.println("Saved SSID: " + (wifi_ssid.isEmpty() ? "<none>" : wifi_ssid));
}

void saveWiFiConfig(const String& ssid, const String& pass) {
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  Serial.println("Wi-Fi credentials saved.");
}

// =====================================================`
// WI-FI CONNECT
// =====================================================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  Serial.print("Connecting to Wi-Fi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    updateWiFiLED();     // keep LED blinking during connect
    delay(100);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiReady = true;
    // digitalWrite(WIFI_LED_PIN, HIGH);   // solid ON
    Serial.println("Wi-Fi Connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiReady = false;
    Serial.println("Wi-Fi connection failed.");
  }
}

// =====================================================
// Wi-Fi/NTP -> RTC sync
// =====================================================
bool syncRTCFromWiFi() {
  if (!rtcAvailable) {
    Serial.println("RTC not available, skipping Wi-Fi time sync.");
    return false;
  }

  Serial.println("Syncing RTC from Wi-Fi/NTP...");
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);

  time_t nowEpoch = 0;
  for (int i = 0; i < 20; i++) {
    nowEpoch = time(nullptr);
    if (nowEpoch > 100000) {
      break;
    }
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (nowEpoch <= 100000) {
    Serial.println("Wi-Fi/NTP time sync failed; keeping existing RTC time.");
    return false;
  }

  struct tm timeinfo;
  localtime_r(&nowEpoch, &timeinfo);

  rtc.adjust(DateTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  ));

  DateTime synced = rtc.now();
  Serial.printf("RTC synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                synced.year(), synced.month(), synced.day(),
                synced.hour(), synced.minute(), synced.second());
  return true;
}

// =====================================================
// TIMESTAMP
// =====================================================
String formatTimestamp() {
  if (!rtcAvailable) {
    return "RTC_NOT_AVAILABLE";
  }

  DateTime now = rtc.now();
  char buf[20];
  snprintf(buf, sizeof(buf),
           "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(),
           now.month(),
           now.day(),
           now.hour(),
           now.minute(),
           now.second());
  return String(buf);
}

// =====================================================
// CAPTIVE PORTAL (SIMPLE)
// =====================================================
const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AQ Node Setup</title>
<style>
body { font-family: Arial; background:#f4f4f4; margin:20px; }
.container { max-width:500px; margin:auto; background:#fff; padding:20px; border-radius:8px; }
label { display:block; margin:10px 0 5px; font-weight:bold; }
input { width:100%; padding:8px; box-sizing:border-box; }
.btn { background:#222; color:#fff; border:none; padding:12px; width:100%; border-radius:4px; cursor:pointer; margin-top:15px; font-size:16px; }
</style>
</head>
<body>
<div class="container">
<h2>AQ Node Wi-Fi Setup</h2>
<form action="/save" method="POST">
    <label>SSID</label>
    <input type="text" name="ssid" required>
    <label>Password</label>
    <input type="password" name="password">
    <button type="submit" class="btn">Save & Reboot</button>
</form>
</div>
</body>
</html>
)rawliteral";

void setupWebRoutes() {
  webServer.on("/", HTTP_GET, []() {
    webServer.send(200, "text/html", CONFIG_PAGE);
  });

  webServer.on("/save", HTTP_POST, []() {
    String ssid = webServer.arg("ssid");
    String password = webServer.arg("password");
    ssid.trim();
    password.trim();

    if (ssid.isEmpty()) {
      webServer.send(400, "text/plain", "SSID required");
      return;
    }
    saveWiFiConfig(ssid, password);
    webServer.send(200, "text/html",
      "<h2>Saved.</h2><p>Rebooting...</p>");
    delay(1000);
    ESP.restart();
  });
}

void startProvisioningAP() {
  provisioningMode = true;
  wifiReady = false;

  WiFi.mode(WIFI_AP);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.softAP(AP_NAME);
  esp_wifi_set_max_tx_power(WIFI_POWER_11dBm);

  Serial.printf("[AP] Started: %s  IP: %s\n",
                AP_NAME, WiFi.softAPIP().toString().c_str());
  Serial.println("[AP] Connect and open http://192.168.4.1/");

  setupWebRoutes();
  webServer.begin();
}

// =====================================================
// SD LOGGING
// =====================================================
void logToSD(float temperature, float humidity, float pm25, float pm10,
             int co2, float noise, int aqi) {
  String row = formatTimestamp() + "," +
               String(temperature, 1) + "," +
               String(humidity, 1)    + "," +
               String(pm25, 1)        + "," +
               String(pm10, 1)        + "," +
               String(co2)            + "," +
               String(noise, 1)       + "," +
               String(aqi);

  File logFile = SD.open(LOG_FILE, FILE_APPEND);
  if (!logFile) {
    Serial.println("SD: open failed");
    return;
  }
  logFile.println(row);
  logFile.flush();
  logFile.close();
  Serial.println("SD: logged -> " + row);
}

// =====================================================
// SENSOR READING
// =====================================================
void readAHT10() {
  sensors_event_t hum, temp;
  aht.getEvent(&hum, &temp);
  temperature = temp.temperature;
  humidity    = hum.relative_humidity;
}

void readNoise() {
  int raw = analogRead(NOISE_PIN);
  float voltage = (raw / 4095.0) * 3.3;
  noiseDBA = voltage * 50.0;
  if (noiseDBA < 30) noiseDBA = 30;
}

int readCO2PWM() {
  unsigned long highTime = pulseIn(CO2_PWM_PIN, HIGH, 2000000);
  if (highTime == 0) return -1;
  float highMs = highTime / 1000.0;
  if (highMs < 2 || highMs > 1002) return -1;
  return (int)(5000.0 * (highMs - 2.0) / 1000.0);
}

bool readSDS011() {
  while (sdsSerial.available() >= 10) {
    if (sdsSerial.read() != 0xAA) continue;

    uint8_t buf[10];
    buf[0] = 0xAA;

    for (int i = 1; i < 10; i++) {
      unsigned long start = millis();
      while (!sdsSerial.available()) {
        if (millis() - start > 100) return false;
      }
      buf[i] = sdsSerial.read();
    }

    if (buf[1] != 0xC0) continue;

    uint8_t checksum = 0;
    for (int i = 2; i <= 7; i++) checksum += buf[i];
    if (checksum != buf[8]) return false;

    pm25 = (((uint16_t)buf[3] << 8) | buf[2]) / 10.0;
    pm10 = (((uint16_t)buf[5] << 8) | buf[4]) / 10.0;
    return true;
  }
  return false;
}

// =====================================================
// AQI
// =====================================================
int calculateAQI(float pm25) {
  struct AQI { float cLow, cHigh; int iLow, iHigh; };
  AQI table[] = {
    {0.0,12.0,0,50},
    {12.1,35.4,51,100},
    {35.5,55.4,101,150},
    {55.5,150.4,151,200},
    {150.5,250.4,201,300},
    {250.5,350.4,301,400},
    {350.5,500.4,401,500}
  };
  for (auto& bp : table) {
    if (pm25 >= bp.cLow && pm25 <= bp.cHigh) {
      return round(((float)(bp.iHigh - bp.iLow) / (bp.cHigh - bp.cLow)) *
                   (pm25 - bp.cLow) + bp.iLow);
    }
  }
  return 500;
}

String getAQICategory(int aqi) {
  if (aqi <= 50)  return "Good";
  if (aqi <= 100) return "Moderate";
  if (aqi <= 150) return "Unhealthy for Sensitive";
  if (aqi <= 200) return "Unhealthy";
  if (aqi <= 300) return "Very Unhealthy";
  return "Hazardous";
}