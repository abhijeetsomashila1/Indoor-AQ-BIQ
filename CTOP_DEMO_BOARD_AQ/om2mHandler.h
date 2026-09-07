#ifndef OM2MHANDLER_H
#define OM2MHANDLER_H

#include <HTTPClient.h>
#include "constants.h"
#include "timeHandler.h"

void postToOneM2M() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OM2M] WiFi not connected, skipping post");
    return;
  }

  time_t epoch = getEpoch();
  if (epoch < MIN_VALID_TIME) {
    Serial.println("[OM2M] Invalid time (epoch 0), skipping post");
    return;
  }

  String data = "[" + String((unsigned long)epoch) + " , " + String(pm2) + " , " + 
                String(pm10) + " , " + String(co2) + " , " + String(voc_index) + " , " + 
                String(raw) + " , " + String(t) + " , " + String(h) + " , " + String(aqi) + "]";

  Serial.println("[OM2M] Payload: " + data);

  String server = "http://" + String(CSE_IP) + ":" + String(CSE_PORT) + String(OM2M_MN);
  String url = server + OM2M_AE + "/" + OM2M_DATA_CONT + "/";

  HTTPClient http;
  http.begin(url);

  http.addHeader("X-M2M-Origin", OM2M_ORGIN); 
  http.addHeader("Content-Type", "application/json;ty=4");

  String req_data = "{\"m2m:cin\": {"
                      "\"con\": \"" + data + "\","
                      "\"lbl\": " + OM2M_DATA_LBL + ","
                      "\"cnf\": \"text\""
                    "}}";

  int code = http.POST(req_data);

  Serial.println("[OM2M] HTTP Response Code: " + String(code));
  if (code > 0) {
    Serial.println("[OM2M] Response: " + http.getString());
  }

  http.end();
}

#endif
