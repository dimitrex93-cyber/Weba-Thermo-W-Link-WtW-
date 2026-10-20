#ifndef INA226_OPTIMIZED_H
#define INA226_OPTIMIZED_H

#include <Arduino.h>
#include <Wire.h>
#include <cstdint>
#include "config.h"

// ============================================================
// INA226 BATTERY MONITOR CLASS (Optimized)
// ============================================================
class INA226Battery {
private:
  uint8_t address;
  float shuntResistor;
  float currentLSB;           // Current LSB for conversions
  float powerLSB;             // Power LSB for conversions
  
  static constexpr uint8_t REG_CONFIG = 0x00;
  static constexpr uint8_t REG_SHUNT_VOLTAGE = 0x01;
  static constexpr uint8_t REG_BUS_VOLTAGE = 0x02;
  static constexpr uint8_t REG_POWER = 0x03;
  static constexpr uint8_t REG_CURRENT = 0x04;
  static constexpr uint8_t REG_CALIBRATION = 0x05;
  
  static constexpr float SHUNT_VOLTAGE_LSB = 0.0000025f;  // 2.5 µV per LSB
  // INA226 (TI SBOS547): Busspannung = volles 16-Bit-Register 0x02, LSB 1,25 mV,
  // Messbereich 0…40,96 V. KEIN Shift — (raw >> 3) ist das INA219-Format und
  // teilt den Wert durch 8 (siehe Fix vom 20.10.2026, docs/ina226-busvoltage/).
  static constexpr float BUS_VOLTAGE_LSB = 0.00125f;      // 1.25 mV per LSB
  static constexpr float BUS_VOLTAGE_MAX = 40.96f;        // ADC-Vollausschlag
  static constexpr uint16_t CONFIG_VALUE = 0x4527;        // Continuous mode
  
  bool readRegister(uint8_t reg, int16_t& out) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    
    if (Wire.requestFrom(address, (uint8_t)2) < 2) return false;
    
    out = ((int16_t)Wire.read() << 8) | Wire.read();
    return true;
  }
  
  // Busspannung und Leistung sind laut Datenblatt UNSIGNED 16 Bit
  // (nur Strom/Shuntspannung sind vorzeichenbehaftet).
  bool readRegisterU16(uint8_t reg, uint16_t& out) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    
    if (Wire.requestFrom(address, (uint8_t)2) < 2) return false;
    
    out = ((uint16_t)Wire.read() << 8) | Wire.read();
    return true;
  }
  
  void writeRegister(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)value);
    Wire.endTransmission();
  }
  
public:
  INA226Battery(uint8_t addr, float shunt) 
    : address(addr), shuntResistor(shunt) {
    // Calculate LSBs for maximum accuracy
    currentLSB = INA226_MAX_CURRENT / 32768.0f;
    powerLSB = currentLSB * 25.0f;
  }
  
  bool begin() {
    // Initialize configuration register
    writeRegister(REG_CONFIG, CONFIG_VALUE);
    delay(I2C_INIT_DELAY_MS);
    
    // Set calibration register for max current range.
    // ACHTUNG (Review 04.08.2026): alte Formel 0.04096/(Imax*Rshunt)
    // ergab (uint16_t)0.2048 = 0 -> Strom/Energie immer 0.
    // Korrekt: CAL = 0.00512 / (currentLSB * Rshunt), currentLSB = Imax/32768.
    // Mit Imax=20A, Rshunt=0.01Ohm: CAL ~= 839.
    float currentLSB = INA226_MAX_CURRENT / 32768.0f;
    uint16_t calibration = (uint16_t)(0.00512f / (currentLSB * shuntResistor) + 0.5f);
    writeRegister(REG_CALIBRATION, calibration);
    
    #if DEBUG_INA226
    Serial.printf(">> INA226 calibration: 0x%04X (%u)\n", calibration, calibration);
    #endif
    
    return true;
  }
  
  // Optimized: Read all values in one burst.
  // Liefert false bei I2C-Fehler (Aufrufer darf dann keine 0-Werte
  // als echte Messung verwenden – sonst droht ein falscher Heiz-Stopp).
  bool readAll(float& voltage, float& current, float& power) {
    uint16_t busVoltageRaw, powerRaw;
    int16_t currentRaw;
    if (!readRegisterU16(REG_BUS_VOLTAGE, busVoltageRaw)) return false;
    if (!readRegister(REG_CURRENT, currentRaw)) return false;
    if (!readRegisterU16(REG_POWER, powerRaw)) return false;
    
    // Busspannung: 16-Bit-Register, 1,25 mV je LSB, kein Shift (INA226).
    voltage = busVoltageRaw * BUS_VOLTAGE_LSB;
    
    // Current: 16-bit signed value
    current = currentRaw * currentLSB;
    
    // Power: 16-bit unsigned value
    power = powerRaw * powerLSB;
    
    // Plausibilitaet: ueber dem ADC-Vollausschlag ist der Messwert unbrauchbar
    // (verhindert, dass ein Fantasiewert in die Akku-Schwellen laeuft).
    if (voltage > BUS_VOLTAGE_MAX) return false;
    return true;
  }
  
  float readBusVoltage() {
    uint16_t raw;
    if (!readRegisterU16(REG_BUS_VOLTAGE, raw)) return 0.0f;
    return raw * BUS_VOLTAGE_LSB;
  }
  
  float readCurrent() {
    int16_t raw;
    if (!readRegister(REG_CURRENT, raw)) return 0.0f;
    return raw * currentLSB;
  }
  
  float readPower() {
    uint16_t raw;
    if (!readRegisterU16(REG_POWER, raw)) return 0.0f;
    return raw * powerLSB;
  }
};

#endif
