#ifndef CONSTANTS_H
#define CONSTANTS_H

// WiFi
#define WIFI_SSID "SCRC-WIFI"
#define WIFI_PASSWORD "SCRC@IIITH"


#define CSE_IP			  "onem2m.iiit.ac.in"
#define CSE_PORT      443
#define HTTPS         false
#define OM2M_ORGIN    "AQSRMon@20:psX9MSnnrvyH"
#define OM2M_MN       "/~/in-cse/in-name/"
#define OM2M_AE       "AE-SR/SR-AQ"

#define OM2M_Node_ID "SR-AQ-KH95-01"
#define OM2M_DATA_CONT  "SR-AQ-KH95-01/Data"
#define OM2M_DATA_LBL "[\"AE-SR-AQ\", \"SR-AQ-KH95-01\", \"V3.0.00\", \"SR-AQ-V3.0.00\"]"

// OLED
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Pins
#define TRIGGER 12
#define CO2_PIN 27   // Example, update based on your wiring
#define PM_RX_PIN 16
#define PM_TX_PIN 17



int co2 = 0;
unsigned long duration, th, tl;
int PIN = 18;
float aqi = 0;
int pm2 = 0, pm10 = 0;
float t=0, h=0, raw=0; 
int voc_index=0;
#endif
