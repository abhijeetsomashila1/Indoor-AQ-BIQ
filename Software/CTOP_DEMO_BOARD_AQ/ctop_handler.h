#ifndef CTOP_HANDLER_H
#define CTOP_HANDLER_H
#include "constants.h"
#include "wifi_handler.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

void postData() {
     if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

      http.begin("https://ctop.iiit.ac.in/api/nodes/create-cin/15");  // Use https for secure connection
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Authorization", "Bearer 2f5d786104198a54f5016ce12286dc96");

        // Create a JSON document to hold the data
        DynamicJsonDocument jsonDoc(1024);
        jsonDoc["type"] = String("AQ");        
        jsonDoc["pm2.5"] = String(pm2);
        jsonDoc["pm10"] = String(pm10);
        jsonDoc["co2"] = String(co2);
        jsonDoc["voc_raw"] = String(raw);
        jsonDoc["voc_index"] = String(voc_index);
        jsonDoc["temperature"] = String(t);
        jsonDoc["humidity"] = String(h);
        jsonDoc["aqi"] = String(aqi);

        // Serialize JSON to string
        String requestBody;
        serializeJson(jsonDoc, requestBody);
        Serial.println(requestBody);
        // Send the POST request
        int httpResponseCode = http.POST(requestBody);

        // Check for successful POST request
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println(httpResponseCode);
            Serial.println(response);
            display.clearDisplay();
            oledText("Send to ctOP", 1, 20, 20);
            oledText("Successfully", 1, 20, 35);
            delay(1000);
            display.clearDisplay();
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }
        http.end();  // End the HTTP connection
    }
}
#endif