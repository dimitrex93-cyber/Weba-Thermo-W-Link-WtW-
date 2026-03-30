#include <Wire.h>
#include <user_interface.h>
#include <Arduino.h>
#include <time.h>

#include "config.h"
#include "types.h"
#include "ina226_optimized.h"
#include "sensors.h"
#include "display_manager.h"
#include "wbus_controller.h"
#include "power_manager.h"
#include "data_manager.h"
#include "log_manager.h"
#include "espnow_bridge.h"

// ============================================================
// DEVICE OBJECTS
// ============================================================
SensorManager sensors;
DisplayManager display;
WBUSController wbus;
PowerManager power;
DataManager data;
LogManager logger;
EspNowBridge espNow;

RuntimeState state = {0};
bool runStartupCarousel = false;
bool latchedError = false;
uint32_t nextCycleDueMs = 0;
uint32_t nextHeartbeatDueTick = 0;
uint32_t nextDisplayFrameDueMs = 0;

// Forward declarations
void handleWakeReason(rst_info* resetInfo);
void handleFunkSignal();
void handleEspNowCommand(EspNowCommandType cmd);
void startHeatingFromRemote(const char* triggerLine1);
void stopHeatingFromRemote(const char* triggerLine1);
void handleTimerWake();
void processHeatingStatus(bool forcedStop);
void goToSleep();
void printDebugInfo();
void handleSerialTimeSync();
void formatTimestamp(uint32_t logicalTime, char* out, size_t outSize);

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  Serial.println("\n\n>>> Webasto Thermo W-Link started");

  rst_info *resetInfo = ESP.getResetInfoPtr();

  // Initialize RJ45 LEDs
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  digitalWrite(LED_GREEN,  HIGH);  
  digitalWrite(LED_YELLOW, LOW);   

  // Initialize subsystems
  Wire.begin(I2C_SDA, I2C_SCL);   
  Wire.setClock(I2C_CLOCK);
  
  if (!sensors.initialize()) {
    Serial.println("! Sensor initialization failed");
  }

  if (!data.initialize()) {
    Serial.println("! RTC data initialization failed");
  }
  data.recordWake(resetInfo->reason);
  
  if (!display.initialize(data.shouldKeepDisplayOn())) {
    Serial.println("! Display initialization failed");
  }

  if (resetInfo->reason == REASON_EXT_SYS_RST) {
    display.showStartupLogo(1500);
  }
  
  wbus.initialize();
  power.initialize();
  if (!logger.initialize()) {
    Serial.println("! Log manager initialization failed");
  } else {
    Serial.println(">> Log manager initialized (/logs/)");
  }

  handleSerialTimeSync();

#if ENABLE_ESPNOW_LINK
  if (!espNow.initialize()) {
    Serial.println("! ESP-NOW initialization failed");
  }
#else
  Serial.println(">> ESP-NOW disabled (ENABLE_ESPNOW_LINK=0)");
#endif
  
  // Handle wake reason (Hier wird geguckt: Knöppken gedrückt? Funk empfangen?)
  handleWakeReason(resetInfo);

  if (runStartupCarousel) {
    // Only on heater start via Funk wake: two carousel rounds
    Serial.println("Starte Seiten-Karussell...");
    display.turnOn();
    display.zeigeSeitenKarussell(state, 2);
    display.zeigeHeizFeedback(state, 3);
  }

  if (latchedError) {
    Serial.println("Latched error active, waiting for manual reset");
    while (true) {
      delay(250);
    }
  }

  // Kontinuierlicher Betrieb: loop() uebernimmt den zyklischen Ablauf.
  if (data.isHeatingActive()) {
    nextHeartbeatDueTick = data.getCurrentTime() + HEATER_HEARTBEAT_INTERVAL_S;
  } else {
    nextHeartbeatDueTick = 0;
  }
  data.save();
  nextCycleDueMs = millis();
  nextDisplayFrameDueMs = millis();
  Serial.println("Setup beendet, starte Dauerbetrieb in loop().");
}

void loop() {
  if (latchedError) {
    delay(250);
    return;
  }

  uint32_t nowMs = millis();
  state.aktuelleZeit = data.getCurrentTime();

  espNow.updateStatusSnapshot(state, data.isHeatingActive(), state.restzeit, state.aktuelleZeit);
  espNow.loop(state.aktuelleZeit);
  display.setEspNowLinkAlive(espNow.isLinkAlive(state.aktuelleZeit, 20));

  EspNowCommandType pendingCmd = ESPNOW_CMD_NONE;
  if (espNow.popPendingCommand(pendingCmd)) {
    handleEspNowCommand(pendingCmd);
  }

  // Keep the heating countdown and icon animation responsive between full cycles.
  if (data.isHeatingActive() && (int32_t)(nowMs - nextDisplayFrameDueMs) >= 0) {
    state.aktuelleZeit = data.getCurrentTime();
    state.restzeit = data.getRemainingHeatingTime(state.aktuelleZeit);
    if (state.restzeit > 0) {
      display.turnOn();
      display.showHeatingTimeFrame(state);
      data.updateLastDisplayTime(state.aktuelleZeit);
      data.setDisplayActive(true);
    }
    nextDisplayFrameDueMs = nowMs + 120;
  }

  if ((int32_t)(nowMs - nextCycleDueMs) < 0) {
    delay(10);
    return;
  }

  // Entspricht dem frueheren Wake-Intervall (aktiv/inaktiv).
  uint16_t cycleSeconds = power.calculateSleepTime(data.isHeatingActive());
  handleTimerWake();

  if (data.isHeatingActive()) {
    uint32_t nowTick = data.getCurrentTime();
    if (nextHeartbeatDueTick == 0) {
      nextHeartbeatDueTick = nowTick + HEATER_HEARTBEAT_INTERVAL_S;
    }

    if ((int32_t)(nowTick - nextHeartbeatDueTick) >= 0) {
      digitalWrite(LED_YELLOW, HIGH);
      bool heartbeatOk = wbus.isHeaterReady();
      digitalWrite(LED_YELLOW, LOW);

      #if ENABLE_SERIAL_DEBUG
      if (heartbeatOk) {
        Serial.printf(">> Heartbeat: Heizung antwortet (t=%lu s)\n", (unsigned long)nowTick);
      } else {
        Serial.printf("! Heartbeat: Keine Antwort von Heizung (t=%lu s)\n", (unsigned long)nowTick);
      }
      #endif

      nextHeartbeatDueTick += HEATER_HEARTBEAT_INTERVAL_S;
      if ((int32_t)(nowTick - nextHeartbeatDueTick) >= 0) {
        nextHeartbeatDueTick = nowTick + HEATER_HEARTBEAT_INTERVAL_S;
      }
    }
  } else {
    nextHeartbeatDueTick = 0;
  }

  data.updateEnergy(power.getEnergyConsumption());
  data.updateLastTemp(state.innenTemperatur);
  data.setNextSleepDuration(cycleSeconds);
  data.save();

  nextCycleDueMs = nowMs + ((uint32_t)cycleSeconds * 1000UL);
}

// ============================================================
// WAKE REASON HANDLING
// ============================================================
void handleWakeReason(rst_info* resetInfo) {
  // REASON_EXT_SYS_RST (6): RST pulled low by the 433 MHz optocoupler.
  //   Only this specific reason activates the heater.
  // REASON_DEEP_SLEEP_AWAKE (5): normal timer wake – run sensor/heating check.
  // All other reasons (power-on, WDT, exception, soft restart, …):
  //   treat as a timer wake so no heating is accidentally triggered.
  if (resetInfo->reason == REASON_EXT_SYS_RST) {
    handleFunkSignal();
  } else {
    handleTimerWake();
  }
}

void handleFunkSignal() {
  #if ENABLE_SERIAL_DEBUG
  Serial.println(">> 433 MHz signal received (RST wake via optocoupler)");
  #endif

  if (data.isHeatingActive()) {
    stopHeatingFromRemote("Funk-Signal");
    return;
  }

  startHeatingFromRemote("Funk-Signal");
}

void handleEspNowCommand(EspNowCommandType cmd) {
  switch (cmd) {
    case ESPNOW_CMD_START_HEATER:
      if (!data.isHeatingActive()) {
        startHeatingFromRemote("ESP-NOW");
      }
      break;
    case ESPNOW_CMD_STOP_HEATER:
      if (data.isHeatingActive()) {
        stopHeatingFromRemote("ESP-NOW");
      }
      break;
    case ESPNOW_CMD_REQUEST_STATUS:
      espNow.requestImmediateStatus();
      break;
    case ESPNOW_CMD_NONE:
    default:
      break;
  }
}

void startHeatingFromRemote(const char* triggerLine1) {
  display.showMessage(triggerLine1, "Pruefe Heizung");

  digitalWrite(LED_YELLOW, HIGH);  // Yellow ON – W-Bus active
  wbus.begin();
  if (!wbus.isHeaterReady()) {
    char ts[24];
    formatTimestamp(data.getCurrentTime(), ts, sizeof(ts));
    logger.appendErrorLog(ts, LogManager::ERR_HEATER_NOT_READY,
                          "Heizung nicht bereit",
                          state.batterieSpannung, state.innenTemperatur);
    data.setDisplayActive(true);
    display.turnOn();
    display.showPersistentStatus("Fehler", "Heizung nicht rdy");
    latchedError = true;
  } else if (wbus.startHeater()) {
    runStartupCarousel = true;
    data.setHeatingActive(true, data.getCurrentTime());
    data.updateLastLogTime(data.getCurrentTime());
    data.setDisplayActive(true);
    data.setDisplayPage(PAGE_STATUS);
    data.updateLastDisplayTime(0);
    power.startHeating();
    state.aktuelleZeit = data.getCurrentTime();
    state.restzeit = HEATING_DURATION_S;
    sensors.readAll(state, true);
    display.turnOn();
    display.setCurrentPage(data.getDisplayPage());
    display.update(state);
    data.setDisplayPage(display.getCurrentPage());
    data.updateLastDisplayTime(state.aktuelleZeit);
    espNow.requestImmediateStatus();
  } else {
    char ts[24];
    formatTimestamp(data.getCurrentTime(), ts, sizeof(ts));
    logger.appendErrorLog(ts, LogManager::ERR_HEATER_OFFLINE,
                          "Heater start fehlgeschlagen",
                          state.batterieSpannung, state.innenTemperatur);
    data.setDisplayActive(false);
    display.turnOn();
    display.showPersistentStatus("Fehler", "Heater offline");
    latchedError = true;
  }
  wbus.end();
  digitalWrite(LED_YELLOW, LOW);   // Yellow OFF – W-Bus done
}

void stopHeatingFromRemote(const char* triggerLine1) {
  display.showMessage(triggerLine1, "Stoppe Heizung");
  digitalWrite(LED_YELLOW, HIGH);
  wbus.begin();
  if (wbus.stopHeater()) {
    data.setHeatingActive(false, 0);
    data.setDisplayActive(false);
    data.setDisplayPage(PAGE_STATUS);
    power.stopHeating();
    state.restzeit = 0;
    display.showHeaterOffFeedback();
    espNow.requestImmediateStatus();
  } else {
    char ts[24];
    formatTimestamp(data.getCurrentTime(), ts, sizeof(ts));
    logger.appendErrorLog(ts, LogManager::ERR_STOP_FAILED,
                          "Stop fehlgeschlagen",
                          state.batterieSpannung, state.innenTemperatur);
    display.showPersistentStatus("Stop fehlgeschl.", "Pruefen");
    latchedError = true;
  }
  wbus.end();
  digitalWrite(LED_YELLOW, LOW);
}

void handleTimerWake() {
  state.aktuelleZeit = data.getCurrentTime();

  if (data.isHeatingActive()) {
    state.restzeit = data.getRemainingHeatingTime(state.aktuelleZeit);
  } else {
    state.restzeit = 0;
  }
  
  // Read all sensors
  sensors.readAll(state);
  
  // Check battery health
  bool shouldStopHeating = false;
  power.checkBatteryHealth(state.batterieSpannung, shouldStopHeating);
  
  if (data.isHeatingActive()) {
    processHeatingStatus(shouldStopHeating);
  }
}

void processHeatingStatus(bool forcedStop) {
  uint32_t elapsedTime = data.getHeatingElapsedTime(state.aktuelleZeit);
  
  if (forcedStop || elapsedTime >= HEATING_DURATION_S) {
    // Stop heater
    if (forcedStop) {
      char ts[24];
      formatTimestamp(state.aktuelleZeit, ts, sizeof(ts));
      logger.appendErrorLog(ts, LogManager::ERR_BATTERY_LOW,
                            "Zwangsstopp Batterie",
                            state.batterieSpannung, state.innenTemperatur);
    }
    digitalWrite(LED_YELLOW, HIGH);  // Yellow ON – W-Bus active
    wbus.begin();
    if (wbus.stopHeater()) {
      display.showHeaterOffFeedback();
    } else {
      display.showMessage("Stop fehlgeschl.", "Pruefen");
    }
    wbus.end();
    digitalWrite(LED_YELLOW, LOW);   // Yellow OFF – W-Bus done
    
    data.setHeatingActive(false, 0);
    data.setDisplayActive(false);
    data.setDisplayPage(PAGE_STATUS);
    power.stopHeating();
    state.restzeit = 0;
  } else {
    // Still heating
    state.restzeit = HEATING_DURATION_S - elapsedTime;
    
    // Update energy tracking
    power.updateEnergy(state.batterieLeistung, state.aktuelleZeit);

    // Write one runtime log entry every ENERGY_LOG_INTERVAL_S seconds.
    uint32_t lastLogTime = data.getLastLogTime();
    if ((state.aktuelleZeit - lastLogTime) >= ENERGY_LOG_INTERVAL_S) {
      char ts[24];
      formatTimestamp(state.aktuelleZeit, ts, sizeof(ts));

      #if ENABLE_SERIAL_DEBUG
      Serial.printf("LOG,ts=%s,t=%lu,rest=%lu,temp=%.1f,batt=%.2fV,p=%.1fW\n",
                    ts,
                    (unsigned long)state.aktuelleZeit,
                    (unsigned long)state.restzeit,
                    state.innenTemperatur,
                    state.batterieSpannung,
                    state.batterieLeistung);
      #endif

      if (!logger.appendRuntimeLog(state.aktuelleZeit, ts, state, true, state.restzeit)) {
        #if ENABLE_SERIAL_DEBUG
        Serial.println("! Failed writing /logs/runtime.csv");
        #endif
      }

      data.updateLastLogTime(state.aktuelleZeit);
    }
    
    // Update display if needed
    uint32_t timeSinceLastUpdate = state.aktuelleZeit - data.getLastDisplayTime();
    if (timeSinceLastUpdate >= ACTIVE_SLEEP_S || !data.shouldKeepDisplayOn()) {
      display.turnOn();
      display.showHeatingTimeOnly(state);
      data.updateLastDisplayTime(state.aktuelleZeit);
      data.setDisplayActive(true);
    }
  }
}

// ============================================================
// SLEEP MANAGEMENT
// ============================================================
void goToSleep() {
  uint16_t sleepSeconds = power.calculateSleepTime(data.isHeatingActive());

  // Update data before sleep
  data.updateEnergy(power.getEnergyConsumption());
  data.updateLastTemp(state.innenTemperatur);
  data.setNextSleepDuration(sleepSeconds);
  data.save();
  
  // Turn off all peripherals
  if (!data.isHeatingActive()) {
    display.turnOff();
  }
  wbus.end();

  // Turn off LEDs to minimise current draw during deep sleep
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_YELLOW, LOW);
  
  Serial.flush();
  
  #if ENABLE_SERIAL_DEBUG
  Serial.printf(">> Deep sleep deaktiviert, waere %u Sekunden\n", sleepSeconds);
  #endif

  // Deep sleep absichtlich deaktiviert, da der ESP sich aufhaengt.
  // delay(1000);
  // uint64_t sleepTimeUs = (uint64_t)sleepSeconds * 1000000ULL;
  // ESP.deepSleep(sleepTimeUs);
}

// ============================================================
// UTILITY FUNCTIONS
// ============================================================
void printDebugInfo() {
  #if ENABLE_SERIAL_DEBUG
  Serial.println("\n=== DEBUG INFO ===");
  Serial.printf("Temperature: %.1f°C\n", state.innenTemperatur);
  Serial.printf("Humidity: %.0f%%\n", state.innenLuftfeuchtigkeit);
  Serial.printf("Pressure: %.0f hPa\n", state.luftdruck);
  Serial.printf("Battery: %.2fV | %.1fmA | %.1fmW\n", 
                state.batterieSpannung, state.batterieStrom * 1000, state.batterieLeistung * 1000);
  Serial.printf("Heating: %s (%u s remaining)\n", 
                data.isHeatingActive() ? "YES" : "NO", state.restzeit);
  Serial.printf("Energy: %.1f Wh\n", power.getEnergyConsumption());
  Serial.println("==================\n");
  #endif
}

void formatTimestamp(uint32_t logicalTime, char* out, size_t outSize) {
  uint32_t unixTime = 0;
  if (!data.getUnixTime(logicalTime, unixTime)) {
    snprintf(out, outSize, "UNSYNC");
    return;
  }

  time_t t = (time_t)unixTime;
  struct tm tmVal;
  gmtime_r(&t, &tmVal);
  snprintf(out, outSize, "%02d.%02d.%04d_%02d:%02d:%02d",
           tmVal.tm_mday,
           tmVal.tm_mon + 1,
           tmVal.tm_year + 1900,
           tmVal.tm_hour,
           tmVal.tm_min,
           tmVal.tm_sec);
}

void handleSerialTimeSync() {
  // Optional: set time from monitor with "SETUNIX=1743240000"
  // Wait briefly on boot so user can send command.
  const uint32_t waitMs = 1200;
  uint32_t start = millis();

  while (millis() - start < waitMs) {
    if (!Serial.available()) {
      delay(10);
      continue;
    }

    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("SETUNIX=")) {
      uint32_t unixVal = (uint32_t)line.substring(8).toInt();
      if (unixVal > 1000000000UL) {
        uint32_t nowTick = data.getCurrentTime();
        data.setUnixTime(unixVal, nowTick);
        data.save();
        Serial.printf("TIME OK: unix=%lu\n", (unsigned long)unixVal);
      } else {
        Serial.println("TIME ERR: invalid unix");
      }
      break;
    }

    if (line == "TIME?") {
      char ts[24];
      formatTimestamp(data.getCurrentTime(), ts, sizeof(ts));
      Serial.printf("TIME=%s\n", ts);
      break;
    }
  }
}
