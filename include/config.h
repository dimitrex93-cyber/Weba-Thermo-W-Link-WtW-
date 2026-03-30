#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// PIN DEFINITIONS
// ============================================================
// 433 MHz module wakes ESP8266 via optocoupler on RST pin (energy efficient deep sleep)
// No separate GPIO needed – wake is detected via reset reason (REASON_EXT_SYS_RST)
#define W_BUS_TX D8                  // SoftwareSerial TX to SI9241A (GPIO15)
#define W_BUS_RX D7                  // SoftwareSerial RX from SI9241A (GPIO13)

// ============================================================
// ESP-NOW LINK (ESP8266 <-> future ESP32-C3 remote)
// ============================================================
// Disabled by default until the ESP32-C3 remote is ready.
#define ENABLE_ESPNOW_LINK 1
#define ESPNOW_WIFI_CHANNEL 13
#define ESPNOW_STATUS_INTERVAL_S 5

// Set the ESP32-C3 MAC here when available.
#define ESPNOW_REMOTE_PEER_MAC_0 0xE0
#define ESPNOW_REMOTE_PEER_MAC_1 0x72
#define ESPNOW_REMOTE_PEER_MAC_2 0xA1
#define ESPNOW_REMOTE_PEER_MAC_3 0x6E
#define ESPNOW_REMOTE_PEER_MAC_4 0x24
#define ESPNOW_REMOTE_PEER_MAC_5 0x5C

// ============================================================
// I2C CONFIGURATION (SSD1306 OLED + sensors)
// ============================================================
#define I2C_SDA 14                   // SDA – GPIO12 (D6)
#define I2C_SCL 12                   // SCL – GPIO14 (D5)
#define OLED_ADDR 0x3C               // SSD1306 I2C address
// Die I2C adresse wird nicht genutzt, die standard adresse im OLED wird verwendet
// ============================================================
// RJ45 LED PINS
// ============================================================
#define LED_GREEN  4                 // Green  LED – power indicator (GPIO4 / D2)
#define LED_YELLOW 5                 // Yellow LED – data bus activity  (GPIO5 / D1)

// ============================================================
// DEEP SLEEP / WAKE HARDWARE NOTES
// ============================================================
// Timer-based wake:  GPIO16 (D0) MUST be connected to RST on the PCB.
//                    ESP.deepSleep() uses this path to auto-wake after the timer.
// 433 MHz wake:      The 433 MHz receiver module drives an optocoupler whose
//                    output pulls RST low.  The firmware detects this as
//                    REASON_EXT_SYS_RST and activates the heater.

// ============================================================
// TIMING CONFIGURATION (seconds)
// ============================================================
#define NORMAL_SLEEP_S 60            // Deep sleep when inactive
#define ACTIVE_SLEEP_S 15            // Deep sleep when heating active (Variant 2: short wakeups)
#define HEATING_DURATION_S 1800      // 30 minutes total heating time
#define HEATER_HEARTBEAT_INTERVAL_S 300 // Every 5 minutes: alive ping while heating
#define DISPLAY_UPDATE_INTERVAL_S 10 // Update display every 10s when active
#define DISPLAY_TIMEOUT_S 30         // Auto-off display after 30s
#define WBUS_RESPONSE_TIMEOUT_MS 1000
#define I2C_INIT_DELAY_MS 10
#define WBUS_INIT_DELAY_MS 50

// ============================================================
// SERIAL & I2C CONFIGURATION
// ============================================================
#define WBUS_BAUD 2400
#define I2C_CLOCK 100000
#define SERIAL_BAUD 115200

// ============================================================
// SENSOR CALIBRATION
// ============================================================
#define INA226_ADDR 0x40
#define INA226_SHUNT_RESISTOR 0.01f  // 0.01 Ohm shunt
#define INA226_MAX_CURRENT 20.0f      // Max 20A for calibration

// ============================================================
// BATTERY THRESHOLDS
// ============================================================
#define BATTERY_LOW_VOLTAGE 10.5f
#define BATTERY_CRITICAL_VOLTAGE 9.5f

// ============================================================
// SENSOR UPDATE INTERVALS
// ============================================================
#define TEMP_READ_INTERVAL_S 10      // Read temp every 10s when inactive
#define TEMP_READ_INTERVAL_ACTIVE_S 5 // Read every 5s when heating
#define BATTERY_READ_INTERVAL_S 5    // Always read battery every 5s

// ============================================================
// DISPLAY BUFFER SIZE
// ============================================================
#define DISPLAY_BUFFER_SIZE 24

// ============================================================
// CRC & DATA VALIDATION
// ============================================================
#define CRC32_POLY 0xEDB88320
#define CRC32_INIT 0xffffffff

// ============================================================
// ENERGY TRACKING
// ============================================================
#define ENERGY_TRACKING_ENABLED 1
#define ENERGY_LOG_INTERVAL_S 60

// ============================================================
// DEBUG FLAGS  (defaults – overridable via platformio.ini build_flags)
// ============================================================
#ifndef ENABLE_SERIAL_DEBUG
  #define ENABLE_SERIAL_DEBUG 1  // default: on (overridden to 0 in release)
#endif
#ifndef DEBUG_INA226
  #define DEBUG_INA226 1         // default: on (overridden to 0 in release)
#endif
#ifndef DEBUG_SENSORS
  #define DEBUG_SENSORS 0        // default: off
#endif

#endif
