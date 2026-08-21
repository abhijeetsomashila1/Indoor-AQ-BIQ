#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "SCRC-WIFI";
const char* password = "SCRC@IIITH";

const char* API_URL =
"https://dev-ctop.iiit.ac.in/api/nodes/create-cin/16";

const char* TOKEN =
"cab2ddd427101e6e551f6727885fe13a";

const char* CO2_FIELD = "co\xE2\x82\x82";

const unsigned long SAMPLE_INTERVAL = 10UL * 60UL * 1000UL;

// =====================================================
// PINS
// =====================================================

// AHT10
#define AHT_SDA 22
#define AHT_SCL 21

// SDS011
#define SDS_RX 17
#define SDS_TX 16

// CO2 PWM
#define CO2_PWM_PIN 27

// Noise Sensor
#define NOISE_PIN 34

// =====================================================
// OBJECTS
// =====================================================

Adafruit_AHTX0 aht;
HardwareSerial sdsSerial(2);

// =====================================================
// VARIABLES
// =====================================================

float temperature = 0;
float humidity = 0;

float pm25 = -1;
float pm10 = -1;

float noiseDBA = 0;

unsigned long lastSample = 0;

// =====================================================
// FUNCTION PROTOTYPES
// =====================================================

void connectWiFi();

void readAHT10();
void readNoise();

int readCO2PWM();
bool readSDS011();

int calculateAQI(float pm25);
String getAQICategory(int aqi);

void postData(
  float temperature,
  float humidity,
  float pm25,
  float pm10,
  int co2,
  float noise,
  int aqi
);

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("AIR QUALITY NODE");
  Serial.println("================================");

  connectWiFi();

  Wire.begin(AHT_SDA, AHT_SCL);

  if (aht.begin())
  {
    Serial.println("AHT10 OK");
  }
  else
  {
    Serial.println("AHT10 NOT DETECTED");
  }

  sdsSerial.begin(
    9600,
    SERIAL_8N1,
    SDS_RX,
    SDS_TX
  );

  pinMode(CO2_PWM_PIN, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(
    NOISE_PIN,
    ADC_11db
  );

  Serial.println("Setup Complete");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  if (millis() - lastSample >= SAMPLE_INTERVAL)
  {
    lastSample = millis();

    readAHT10();
    readSDS011();
    readNoise();

    int co2ppm = readCO2PWM();
    int aqi = -1;

    if (pm25 >= 0)
    {
      aqi = calculateAQI(pm25);
    }

    Serial.println();
    Serial.println("================================");

    Serial.print("Temperature : ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print("Humidity    : ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.println();

    Serial.print("PM2.5       : ");
    Serial.print(pm25);
    Serial.println(" ug/m3");

    Serial.print("PM10        : ");
    Serial.print(pm10);
    Serial.println(" ug/m3");

    Serial.println();

    Serial.print("CO2         : ");
    Serial.print(co2ppm);
    Serial.println(" ppm");

    Serial.println();

    Serial.print("Noise       : ");
    Serial.print(noiseDBA);
    Serial.println(" dBA");

    Serial.println();

    Serial.print("AQI         : ");
    Serial.println(aqi);

    if (aqi >= 0)
    {
      Serial.print("Category    : ");
      Serial.println(getAQICategory(aqi));
    }

    Serial.println("================================");

    postData(
      temperature,
      humidity,
      pm25,
      pm10,
      co2ppm,
      noiseDBA,
      aqi
    );
  }
}

// =====================================================
// WIFI
// =====================================================

void connectWiFi()
{
  WiFi.begin(ssid, password);

  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// HTTP POST
// =====================================================

void postData(
  float temperature,
  float humidity,
  float pm25,
  float pm10,
  int co2,
  float noise,
  int aqi
)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Disconnected");
    return;
  }

  HTTPClient http;

  http.begin(API_URL);

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  http.addHeader(
    "Authorization",
    String("Bearer ") + TOKEN
  );

  DynamicJsonDocument jsonDoc(512);

  JsonArray con = jsonDoc.createNestedArray("con");
  con.add(noise);
  con.add(aqi);
  con.add(pm10);
  con.add(pm25);
  con.add(temperature);
  con.add(co2);
  con.add(humidity);

  String requestBody;

  serializeJson(
    jsonDoc,
    requestBody
  );

  Serial.print("Request body: ");
  Serial.println(requestBody);

  int responseCode =
    http.POST(requestBody);

  Serial.print("HTTP Response: ");
  Serial.println(responseCode);

  if (responseCode > 0)
  {
    Serial.println(
      http.getString()
    );
  }

  http.end();
}

// =====================================================
// AHT10
// =====================================================

void readAHT10()
{
  sensors_event_t hum;
  sensors_event_t temp;

  aht.getEvent(
    &hum,
    &temp
  );

  temperature =
    temp.temperature;

  humidity =
    hum.relative_humidity;
}

// =====================================================
// NOISE SENSOR
// =====================================================

void readNoise()
{
  int raw =
    analogRead(NOISE_PIN);

  float voltage =
    (raw / 4095.0) * 3.3;

  noiseDBA =
    voltage * 50.0;

  if (noiseDBA < 30)
    noiseDBA = 30;
}

// =====================================================
// CO2 PWM
// =====================================================

int readCO2PWM()
{
  unsigned long highTime =
    pulseIn(
      CO2_PWM_PIN,
      HIGH,
      2000000
    );

  if (highTime == 0)
    return -1;

  float highMs =
    highTime / 1000.0;

  if (highMs < 2 ||
      highMs > 1002)
    return -1;

  return
    5000.0 *
    (highMs - 2.0) /
    1000.0;
}

// =====================================================
// SDS011
// =====================================================

bool readSDS011()
{
  while (sdsSerial.available() >= 10)
  {
    if (sdsSerial.read() != 0xAA)
      continue;

    uint8_t buf[10];

    buf[0] = 0xAA;

    for (int i = 1; i < 10; i++)
    {
      unsigned long start =
        millis();

      while (!sdsSerial.available())
      {
        if (millis() - start > 100)
          return false;
      }

      buf[i] =
        sdsSerial.read();
    }

    if (buf[1] != 0xC0)
      continue;

    uint8_t checksum = 0;

    for (int i = 2; i <= 7; i++)
    {
      checksum += buf[i];
    }

    if (checksum != buf[8])
      return false;

    pm25 =
      (((uint16_t)buf[3] << 8)
      | buf[2]) / 10.0;

    pm10 =
      (((uint16_t)buf[5] << 8)
      | buf[4]) / 10.0;

    return true;
  }

  return false;
}

// =====================================================
// AQI
// =====================================================

int calculateAQI(float pm25)
{
  struct AQI
  {
    float cLow;
    float cHigh;
    int iLow;
    int iHigh;
  };

  AQI table[] =
  {
    {0.0,12.0,0,50},
    {12.1,35.4,51,100},
    {35.5,55.4,101,150},
    {55.5,150.4,151,200},
    {150.5,250.4,201,300},
    {250.5,350.4,301,400},
    {350.5,500.4,401,500}
  };

  for (auto &bp : table)
  {
    if (pm25 >= bp.cLow &&
        pm25 <= bp.cHigh)
    {
      return round(
        ((float)(bp.iHigh - bp.iLow) /
        (bp.cHigh - bp.cLow)) *
        (pm25 - bp.cLow) +
        bp.iLow
      );
    }
  }

  return 500;
}

String getAQICategory(int aqi)
{
  if (aqi <= 50)
    return "Good";

  if (aqi <= 100)
    return "Moderate";

  if (aqi <= 150)
    return "Unhealthy for Sensitive";

  if (aqi <= 200)
    return "Unhealthy";

  if (aqi <= 300)
    return "Very Unhealthy";

  return "Hazardous";
}