#ifndef CO2_SENSOR_H
#define CO2_SENSOR_H

#include <Arduino.h>
#include "constants.h"



void CO2_Monitor() {


  th = pulseIn(PIN, HIGH, 2008000) / 1000;
  tl = 1004 - th;
  co2 = 2000 * (th - 2) / (th + tl - 4);

  Serial.print("CO2 Concentration: ");
  Serial.println(co2);
}


#endif
