// timeHandler.h
#ifndef TIMEHANDLER_H
#define TIMEHANDLER_H

#include <time.h>
#include <WiFi.h>

// Minimum valid epoch (e.g. Jan 1 2023)
#define MIN_VALID_TIME 1672000000UL  
#define MAX_VALID_TIME 4102444800UL  // year 2100

static unsigned long last_sync_ms = 0;
static const unsigned long SYNC_REFRESH_MS = 60UL * 60UL * 1000UL; // refresh every hour

// Sync system time using configTime() and wait for getLocalTime()
// Returns true on success (valid epoch), false on failure
bool sync_time(uint8_t tries = 5, uint16_t waitMs = 2000) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[timeHandler] WiFi not connected, cannot sync time");
    return false;
  }

  // Configure timezone: IST = UTC+5:30
  // Use reliable NTP servers
  configTime(5 * 3600, 30 * 60, "pool.ntp.org", "time.nist.gov", "time.google.com");

  struct tm timeinfo;
  for (uint8_t i = 0; i < tries; ++i) {
    if (getLocalTime(&timeinfo, waitMs / 2)) { // wait portion each try
      time_t epoch = mktime(&timeinfo);
      if (epoch >= MIN_VALID_TIME && epoch <= MAX_VALID_TIME) {
        last_sync_ms = millis();
        Serial.print("[timeHandler] NTP synced epoch: ");
        Serial.println((unsigned long)epoch);
        return true;
      } else {
        Serial.println("[timeHandler] epoch out of range, retrying...");
      }
    } else {
      Serial.println("[timeHandler] getLocalTime() failed, retrying...");
    }
    delay(500);
  }

  Serial.println("[timeHandler] NTP sync failed after retries");
  return false;
}

// Return a valid epoch (time_t) or 0 if not available.
// This calls getLocalTime() so it always returns system time at call moment.
time_t getEpoch() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 1000)) {
    time_t epoch = mktime(&timeinfo);
    if (epoch >= MIN_VALID_TIME && epoch <= MAX_VALID_TIME) {
      // Auto-refresh periodically if needed (non-blocking here)
      if (millis() - last_sync_ms > SYNC_REFRESH_MS) {
        // schedule a background refresh by calling sync_time() from loop/setup if desired
        // do not block here; just note that refresh is due
      }
      return epoch;
    }
  }
  return 0;
}

#endif // TIMEHANDLER_H
