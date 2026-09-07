#include "wifi_handler.h"
#include "oled_handler.h"
#include "pm_sensor.h"
#include "sht_sgp_handler.h"
#include "co2_sensor.h"
#include "ctop_handler.h"
#include "constants.h"
#include "om2mHandler.h"
#include "timeHandler.h"


void setup() {
  Serial.begin(9600);          // Start serial first (important for logs)
  delay(1000);

  initOLED();
  initWiFi();                  // Connect WiFi after Serial is ready

  if (!sync_time()) {
    Serial.println("[Main] NTP sync failed, will retry later...");
  } else {
    Serial.println("[Main] NTP sync successful.");
  }

  PM_setup();
  sht_sgp_setup();

  oledText("Indoor Air", 1, 35, 20);
  oledText("Quality Monitor", 1, 21, 35);
  delay(5000);
  display.clearDisplay();
}


void loop() {
  Serial.println(".........................");
  PM_read();
  sht_loop();
  sgp_loop();
  CO2_Monitor();
  Serial.println(".........................");
  // simple AQI (placeholder, you can keep your full formula)
  aqi = max(pm2, pm10);
  postData();
  postToOneM2M();
  showDataOLED();
    if (WiFi.status() != WL_CONNECTED)
    {
        initWiFi();
        return;
    }
  delay(5000);
}
