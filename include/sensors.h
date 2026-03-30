#ifndef SENSORS_H
#define SENSORS_H

#include <AHT20.h>
#include <Adafruit_BMP280.h>
#include "config.h"
#include "types.h"
#include "ina226_optimized.h"
#include "Workbench_mode.h"

class SensorManager {
private:
  AHT20 aht20;
  Adafruit_BMP280 bmp280;
  INA226Battery ina226;
  
  uint32_t lastAHT20Read = 0;
  uint32_t lastBMP280Read = 0;
  uint32_t lastINA226Read = 0;
  
  uint8_t failureCount = 0;
  static constexpr uint8_t MAX_FAILURES = 3;
  
public:
  SensorManager() : ina226(INA226_ADDR, INA226_SHUNT_RESISTOR) {}
  
  bool initialize() {
    #if WORKBENCH_MODE
      Serial.println(">>> WERKBANK MODUS AKTIV: Sensoren werden simuliert");
      return true; 
    #else
      bool allOk = true;
      if (!aht20.begin()) { Serial.println("! AHT20 failed"); allOk = false; }
      if (!bmp280.begin()) { 
        Serial.println("! BMP280 failed"); allOk = false; 
      } else {
        bmp280.setSampling(Adafruit_BMP280::MODE_FORCED, Adafruit_BMP280::SAMPLING_X2, Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::FILTER_OFF, Adafruit_BMP280::STANDBY_MS_1);
      }
      if (!ina226.begin()) { Serial.println("! INA226 failed"); allOk = false; }
      return allOk;
    #endif
  }
  
  void readAll(RuntimeState& state, bool forceRead = false) {
    #if WORKBENCH_MODE
    (void)forceRead;
    ladeWerkbankWerte(state);
    return;
    #endif

    uint32_t now = millis() / 1000;
    uint16_t tempInterval = state.restzeit > 0 ? TEMP_READ_INTERVAL_ACTIVE_S : TEMP_READ_INTERVAL_S;
    
    if (forceRead || (now - lastAHT20Read >= tempInterval)) {
      readAHT20(state);
      lastAHT20Read = now;
    }
    
    if (forceRead || (now - lastBMP280Read >= 30)) {
      readBMP280(state);
      lastBMP280Read = now;
    }
    
    if (forceRead || (now - lastINA226Read >= BATTERY_READ_INTERVAL_S)) {
      readINA226(state);
      lastINA226Read = now;
    } 
  }

private:
  void readAHT20(RuntimeState& state) {
    float temp = aht20.getTemperature();
    float hum  = aht20.getHumidity();
    if (isnan(temp) || isnan(hum)) {
      state.aht20Status = SENSOR_FAILED;
      return;
    }
    state.innenTemperatur = temp;
    state.innenLuftfeuchtigkeit = hum;
    state.aht20Status = SENSOR_OK;
  }
  
  void readBMP280(RuntimeState& state) {
    state.luftdruck = bmp280.readPressure() / 100.0f;
    state.bmp280Status = SENSOR_OK;
  }
  
  void readINA226(RuntimeState& state) {
    float voltage, current, power;
    ina226.readAll(voltage, current, power);
    state.batterieSpannung = voltage;
    state.batterieStrom = current;
    state.batterieLeistung = power;
    state.ina226Status = SENSOR_OK;
  }
};

#endif