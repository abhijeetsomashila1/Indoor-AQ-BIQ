#ifndef OLED_HANDLER_H
#define OLED_HANDLER_H

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "constants.h"

// SH1106G constructor
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void initOLED() {
  if (!display.begin(OLED_ADDR, true)) {   // Initialize OLED
    for (;;); // Halt if init fails
  }
  display.clearDisplay();
  display.display();
}

// ✅ Modified helper (no display.display() here!)
void oledText(String text, int fontsize, int x, int y )
{
  
  display.setTextSize(fontsize);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(x, y);
  display.println(text);
  display.display();
}

// ✅ Updated showDataOLED
void showDataOLED() {
  display.clearDisplay();
  oledText("Temp:", 2, 0, 20);
  oledText(String(t) + " C", 2, 0, 45);
  display.display();
  delay(2000);

  display.clearDisplay();
  oledText("Humi:", 2, 0, 20);
  oledText(String(h) + " %", 2, 0, 45);
  display.display();
  delay(2000);

  display.clearDisplay();
  oledText("PM2.5:", 2, 0, 20);
  oledText(String(pm2) + " ppm", 2, 0, 45);
  display.display();
  delay(2000);

  display.clearDisplay();
  oledText("PM10:", 2, 0, 20);
  oledText(String(pm10) + " ppm", 2, 0, 45);
  display.display();
  delay(2000);

  display.clearDisplay();
  oledText("CO2:", 2, 0, 20);
  oledText(String(co2) + " ppm", 2, 0, 45);
  display.display();
  delay(2000);


  display.clearDisplay();
  oledText("VOC ROW:", 2, 0, 20);
  oledText(String(raw), 2, 0, 45);
  display.display();
  delay(2000);
 
  display.clearDisplay();
  oledText("VOC INDEX:", 2, 0, 20);
  oledText(String(voc_index),2, 0, 45);
  display.display();
  delay(2000);

  display.clearDisplay();
  oledText("AQI:", 2, 0, 20);
  oledText(String(aqi) + " ppm", 2, 0, 45);
  display.display();
  delay(2000);
}

#endif
