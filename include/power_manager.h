#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "config.h"
#include "types.h"
#include <cmath>

// ============================================================
// POWER MANAGER CLASS
// ============================================================
class PowerManager {
private:
  EnergyData energyData = {0};
  uint32_t lastHeatingStartTime = 0;
  bool heatingActive = false;
  
public:
  // ACHTUNG (Review 04.08.2026): 
  // - currentTime auf LOGISCHER Zeitbasis (data.getCurrentTime()), sonst
  //   verfälscht startTick>0 die Energie-Berechnung (Zeitbasen-Mix).
  // - startEnergyWh aus dem RTC-Speicher (data.getEnergy()) laden, sonst
  //   geht die kumulierte Energie bei jedem Reset verloren.
  void initialize(uint32_t currentTime, float startEnergyWh) {
    energyData.lastUpdateTime = currentTime;
    energyData.totalEnergy_Wh = startEnergyWh;
    energyData.peakPower_W = 0.0f;
    energyData.heatingCycles = 0;
  }
  
  void startHeating() {
    heatingActive = true;
    lastHeatingStartTime = millis() / 1000;
    energyData.heatingCycles++;
    
    #if ENABLE_SERIAL_DEBUG
    Serial.printf(">> Heating started (cycle %u)\n", energyData.heatingCycles);
    #endif
  }
  
  void stopHeating() {
    heatingActive = false;
    
    #if ENABLE_SERIAL_DEBUG
    Serial.printf(">> Heating stopped (total: %.1f Wh)\n", energyData.totalEnergy_Wh);
    #endif
  }
  
  void updateEnergy(float powerW, uint32_t currentTime) {
    if (!heatingActive) return;
    
    uint32_t deltaTime = currentTime - energyData.lastUpdateTime;
    if (deltaTime == 0) return;
    
    // Energy = Power * Time (W * s / 3600 = Wh)
    float deltaEnergy_Wh = (powerW * deltaTime) / 3600.0f;
    energyData.totalEnergy_Wh += deltaEnergy_Wh;
    
    // Track peak power
    if (powerW > energyData.peakPower_W) {
      energyData.peakPower_W = powerW;
    }
    
    energyData.lastUpdateTime = currentTime;
    
    #if ENABLE_SERIAL_DEBUG
    static uint32_t lastLog = 0;
    if (currentTime - lastLog > ENERGY_LOG_INTERVAL_S) {
      Serial.printf("Energy: %.1f Wh | Peak: %.1f W\n", 
                    energyData.totalEnergy_Wh, energyData.peakPower_W);
      lastLog = currentTime;
    }
    #endif
  }
  
  void checkBatteryHealth(float voltage, bool& shouldStopHeating) {
    shouldStopHeating = false;
    
    if (voltage < BATTERY_CRITICAL_VOLTAGE) {
      #if ENABLE_SERIAL_DEBUG
      Serial.println("! CRITICAL: Battery voltage too low!");
      #endif
      shouldStopHeating = true;
    } else if (voltage < BATTERY_LOW_VOLTAGE) {
      #if ENABLE_SERIAL_DEBUG
      Serial.println("! WARNING: Battery low voltage");
      #endif
    }
  }
  
  uint16_t calculateSleepTime(bool heatingActive) const {
    return heatingActive ? ACTIVE_SLEEP_S : NORMAL_SLEEP_S;
  }
  
  float getEnergyConsumption() const {
    return energyData.totalEnergy_Wh;
  }
  
  float getPeakPower() const {
    return energyData.peakPower_W;
  }
  
  uint16_t getHeatingCycles() const {
    return energyData.heatingCycles;
  }
};

#endif
