#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "config.h"
#include "types.h"
#include <cstdint>

// ============================================================
// DATA MANAGER CLASS - RTC & CRC Handling
// ============================================================
class DataManager {
private:
  RTCData rtcData = {0};
  bool rtcValid = false;
  
public:
  bool initialize() {
    ESP.rtcUserMemoryRead(0, (uint32_t*)&rtcData, sizeof(rtcData));
    rtcValid = validateRTC();
    
    if (!rtcValid) {
      #if ENABLE_SERIAL_DEBUG
      Serial.println("! RTC data invalid, resetting");
      #endif
      resetRTC();
    }
    
    return rtcValid;
  }
  
  bool validateRTC() {
    // Check if CRC is valid
    uint32_t calculatedCRC = calculateCRC32((uint8_t*)&rtcData, sizeof(rtcData) - 4);
    
    if (calculatedCRC != rtcData.crc32) {
      return false;
    }

    if (rtcData.displayPage >= PAGE_COUNT) {
      return false;
    }
    
    return true;
  }
  
  void resetRTC() {
    memset(&rtcData, 0, sizeof(rtcData));
    rtcData.heizungAktiv = false;
    rtcData.heizungStartZeit = 0;
    rtcData.letzteDisplayAktualisierung = 0;
    rtcData.letzteLogZeit = 0;
    rtcData.unixBaseTime = 0;
    rtcData.unixBaseTick = 0;
    rtcData.startTick = 0;
    rtcData.gesamtEnergie_Wh_x100 = 0;
    rtcData.displayAktiv = false;
    rtcData.zeitGueltig = false;
    rtcData.displayPage = PAGE_STATUS;
    rtcData.lastSleepDuration_s = 0;
    rtcValid = true;
  }
  
  void save() {
    rtcData.crc32 = calculateCRC32((uint8_t*)&rtcData, sizeof(rtcData) - 4);
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtcData, sizeof(rtcData));
    
    #if ENABLE_SERIAL_DEBUG
    Serial.printf(">> RTC data saved (CRC: 0x%08X)\n", rtcData.crc32);
    #endif
  }
  
  // Accessors with safety checks
  bool isHeatingActive() const { return rtcData.heizungAktiv && rtcValid; }

  void recordWake(uint8_t resetReason) {
    if (!rtcValid) {
      return;
    }

    if (resetReason == REASON_DEEP_SLEEP_AWAKE) {
      rtcData.startTick += rtcData.lastSleepDuration_s;
    }

    rtcData.lastSleepDuration_s = 0;
  }

  uint32_t getCurrentTime() const {
    return rtcData.startTick + (millis() / 1000);
  }

  void setUnixTime(uint32_t unixTime, uint32_t currentTick) {
    rtcData.unixBaseTime = unixTime;
    rtcData.unixBaseTick = currentTick;
    rtcData.zeitGueltig = true;
  }

  bool hasValidUnixTime() const {
    return rtcData.zeitGueltig;
  }

  bool getUnixTime(uint32_t currentTick, uint32_t& unixTimeOut) const {
    if (!rtcData.zeitGueltig) {
      return false;
    }

    unixTimeOut = rtcData.unixBaseTime + (currentTick - rtcData.unixBaseTick);
    return true;
  }
  
  void setHeatingActive(bool active, uint32_t startTime) {
    rtcData.heizungAktiv = active;
    if (active) {
      rtcData.heizungStartZeit = startTime;
      rtcData.displayAktiv = true;
    } else {
      rtcData.heizungStartZeit = 0;
      rtcData.displayAktiv = false;
    }
  }
  
  uint32_t getHeatingStartTime() const { return rtcData.heizungStartZeit; }
  
  uint32_t getHeatingElapsedTime(uint32_t currentTime) const {
    if (!rtcData.heizungAktiv) return 0;
    return currentTime - rtcData.heizungStartZeit;
  }

  uint32_t getRemainingHeatingTime(uint32_t currentTime) const {
    uint32_t elapsedTime = getHeatingElapsedTime(currentTime);
    if (elapsedTime >= HEATING_DURATION_S) {
      return 0;
    }

    return HEATING_DURATION_S - elapsedTime;
  }
  
  void updateLastTemp(float temp) {
    // Store temperature as int8_t (-20 to +80°C range)
    rtcData.lastTemp_minus20 = (uint8_t)(temp + 20);
  }
  
  void updateLastDisplayTime(uint32_t time) {
    rtcData.letzteDisplayAktualisierung = time;
  }
  
  uint32_t getLastDisplayTime() const {
    return rtcData.letzteDisplayAktualisierung;
  }

  void updateLastLogTime(uint32_t time) {
    rtcData.letzteLogZeit = time;
  }

  uint32_t getLastLogTime() const {
    return rtcData.letzteLogZeit;
  }

  void setDisplayActive(bool active) {
    rtcData.displayAktiv = active;
  }

  bool shouldKeepDisplayOn() const {
    return rtcData.displayAktiv && isHeatingActive();
  }

  void setDisplayPage(DisplayPage page) {
    rtcData.displayPage = static_cast<uint8_t>(page);
  }

  DisplayPage getDisplayPage() const {
    if (rtcData.displayPage >= PAGE_COUNT) {
      return PAGE_STATUS;
    }

    return static_cast<DisplayPage>(rtcData.displayPage);
  }

  void setNextSleepDuration(uint16_t sleepSeconds) {
    rtcData.lastSleepDuration_s = sleepSeconds;
  }
  
  void updateEnergy(float wh) {
    rtcData.gesamtEnergie_Wh_x100 = (uint32_t)(wh * 100.0f);
  }
  
  float getEnergy() const {
    return rtcData.gesamtEnergie_Wh_x100 / 100.0f;
  }
  
private:
  uint32_t calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = CRC32_INIT;
    
    while (length--) {
      crc ^= *data++;
      for (int i = 0; i < 8; i++) {
        crc = (crc & 1) ? (crc >> 1) ^ CRC32_POLY : (crc >> 1);
      }
    }
    
    return ~crc;
  }
};

#endif
