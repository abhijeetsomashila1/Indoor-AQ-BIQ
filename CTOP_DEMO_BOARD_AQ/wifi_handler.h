#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <WiFi.h>
#include "constants.h"
#include "oled_handler.h"

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  display.clearDisplay();
  Serial.println("Connecting to WiFi...");
  oledText("Connecting to ", 1, 20, 20);
  oledText(WIFI_SSID, 1, 20, 35);
  while (WiFi.status() != WL_CONNECTED ) {
    Serial.print(".");
    delay(1000);
  }
  display.clearDisplay();
  oledText("Connected to ", 1, 20, 20);
  oledText(WIFI_SSID, 1, 20, 35);
  delay(3000);
  display.clearDisplay();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Failed. Restarting...");
    ESP.restart();
  }
  delay(3000);
}

#endif






