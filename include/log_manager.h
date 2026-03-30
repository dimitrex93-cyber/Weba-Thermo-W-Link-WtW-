#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include "types.h"

class LogManager {
public:
  // Error codes for appendErrorLog()
  static constexpr uint8_t ERR_HEATER_NOT_READY = 0x01;  // isHeaterReady() failed
  static constexpr uint8_t ERR_HEATER_OFFLINE    = 0x02;  // startHeater() failed
  static constexpr uint8_t ERR_STOP_FAILED       = 0x03;  // stopHeater() failed
  static constexpr uint8_t ERR_BATTERY_LOW       = 0x04;  // forced stop: battery under threshold

private:
  static constexpr const char* LOG_DIR = "/logs";
  bool initialized = false;

  // Build  "/logs/<prefix>_YYYY-MM.csv"  from a "DD.MM.YYYY_HH:MM:SS" timestamp.
  // Falls back to "/logs/<prefix>_unsync.csv" when timestamp is "UNSYNC".
  void buildMonthlyPath(const char* prefix,
                        const char* timestamp,
                        char* out, size_t outSize) {
    if (timestamp[0] >= '0' && timestamp[0] <= '9' && strlen(timestamp) >= 10) {
      // DD.MM.YYYY_HH:MM:SS
      //   0123456789
      char month[3] = { timestamp[3], timestamp[4], '\0' };
      char year[5]  = { timestamp[6], timestamp[7], timestamp[8], timestamp[9], '\0' };
      snprintf(out, outSize, "/logs/%s_%s-%s.csv", prefix, year, month);
    } else {
      snprintf(out, outSize, "/logs/%s_unsync.csv", prefix);
    }
  }

  // Create file with header row if it does not exist yet.
  bool ensureFileWithHeader(const char* path, const char* header) {
    if (!LittleFS.exists(path)) {
      File f = LittleFS.open(path, "w");
      if (!f) return false;
      f.println(header);
      f.close();
    }
    return true;
  }

  bool ensureLogDir() {
    if (!LittleFS.exists(LOG_DIR)) {
      return LittleFS.mkdir(LOG_DIR);
    }
    return true;
  }

public:
  bool initialize() {
    if (!LittleFS.begin()) {
      initialized = false;
      return false;
    }
    initialized = ensureLogDir();
    return initialized;
  }

  // Append one row to the monthly runtime CSV (e.g. /logs/runtime_2026-03.csv).
  bool appendRuntimeLog(uint32_t logicalTime,
                        const char* timestamp,
                        const RuntimeState& state,
                        bool heatingActive,
                        uint32_t remainingSeconds) {
    if (!initialized) return false;

    char path[48];
    buildMonthlyPath("runtime", timestamp, path, sizeof(path));

    if (!ensureFileWithHeader(path,
          "logical_time_s,timestamp,heating_active,remaining_s,"
          "temp_c,humidity_pct,pressure_hpa,batt_v,batt_a,batt_w")) {
      return false;
    }

    File f = LittleFS.open(path, "a");
    if (!f) return false;

    f.printf("%lu,%s,%u,%lu,%.1f,%.1f,%.1f,%.2f,%.3f,%.2f\n",
             (unsigned long)logicalTime,
             timestamp,
             heatingActive ? 1U : 0U,
             (unsigned long)remainingSeconds,
             state.innenTemperatur,
             state.innenLuftfeuchtigkeit,
             state.luftdruck,
             state.batterieSpannung,
             state.batterieStrom,
             state.batterieLeistung);
    f.close();
    return true;
  }

  // Append one row to the monthly error CSV (e.g. /logs/errors_2026-03.csv).
  bool appendErrorLog(const char* timestamp,
                      uint8_t errorCode,
                      const char* errorText,
                      float battV,
                      float tempC) {
    if (!initialized) return false;

    char path[48];
    buildMonthlyPath("errors", timestamp, path, sizeof(path));

    if (!ensureFileWithHeader(path,
          "timestamp,error_code,error_text,batt_v,temp_c")) {
      return false;
    }

    File f = LittleFS.open(path, "a");
    if (!f) return false;

    f.printf("%s,0x%02X,%s,%.2f,%.1f\n",
             timestamp,
             (unsigned)errorCode,
             errorText,
             battV,
             tempC);
    f.close();
    return true;
  }
};

#endif
