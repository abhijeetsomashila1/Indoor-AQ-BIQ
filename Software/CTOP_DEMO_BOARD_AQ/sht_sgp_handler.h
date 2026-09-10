#ifndef SHT_SGP_HANDLER_H
#define SHT_SGP_HANDLER_H

#include "Adafruit_SGP40.h"
#include "Adafruit_SHT4x.h"
#include "constants.h"
Adafruit_SGP40 sgp;
Adafruit_SHT4x sht4;


void sht_sgp_setup() {
  if (!sgp.begin()) { Serial.println("SGP not found"); while(1); }
  if (!sht4.begin()) { Serial.println("SHT not found"); while(1); }
}

void sht_loop() {
  sensors_event_t hum, temp;
  sht4.getEvent(&hum, &temp);
  t = temp.temperature;
  h = hum.relative_humidity;
  Serial.printf("Temp: %.2f, Hum: %.2f\n", t, h);
}

void sgp_loop() {
  voc_index = sgp.measureVocIndex(t,h);
  raw = sgp.measureRaw();
  Serial.printf("VOC idx: %d, Raw: %.2f\n", voc_index, raw);
}

#endif
