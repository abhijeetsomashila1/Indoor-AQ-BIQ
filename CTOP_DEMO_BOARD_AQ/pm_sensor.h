#ifndef PM_SENSOR_H
#define PM_SENSOR_H

#include <Arduino.h>
#include "constants.h"

static byte command_frame[9] = {0xAA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x67, 0xBB};
static byte received_data[9];
static int sum = 0;
static unsigned long pm_prev_time = 0;
static HardwareSerial pmSerial(2);

void send_command(byte cmd) {
  command_frame[1] = cmd;
  int sum = command_frame[0]+command_frame[1]+command_frame[2]+command_frame[3]+command_frame[4]+command_frame[5]+command_frame[8];
  int rem = sum % 256;
  command_frame[6] = (sum - rem) / 256;
  command_frame[7] = rem;
  pmSerial.write(command_frame, sizeof(command_frame));
  pmSerial.flush();
}

inline bool checksum() {
  sum = received_data[0]+received_data[1]+received_data[2]+received_data[3]+received_data[4]+received_data[5]+received_data[8];
  return sum == (received_data[6]*256+received_data[7]);
}

inline void PM_setup() {
  pmSerial.begin(9600, SERIAL_8N1, PM_RX_PIN, PM_TX_PIN);
  pmSerial.setTimeout(250);
  send_command(0x01);
}

inline void  PM_read() {
  send_command(0x02);
  if (pmSerial.readBytes(received_data, sizeof(received_data)) != sizeof(received_data)) {
    Serial.println("PM sensor timeout");
    return;
  }

  if (received_data[0] != 0xAA || received_data[8] != 0xBB || !checksum()) {
    Serial.println("PM sensor invalid frame");
    return;
  }

  pm2 = received_data[4] * 256 + received_data[5];
  pm10 = received_data[2] * 256 + received_data[3];
  Serial.print("PM2.5: "); Serial.println(pm2);
  Serial.print("PM10: "); Serial.println(pm10);
}

#endif
