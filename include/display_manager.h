#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include "config.h"
#include "types.h"
#include "Bitmaps.h"


// ============================================================
// DISPLAY MANAGER CLASS
// ============================================================
class DisplayManager {
private:
  U8G2_SSD1306_128X64_NONAME_F_SW_I2C display;
  DisplayPage currentPage = PAGE_STATUS;
  uint32_t displayOnTime = 0;
  bool displayActive = false;
  bool espNowLinkAlive = false;
  uint8_t fanFrame = 0;
  uint8_t fireFrame = 0;
  static constexpr uint8_t FAN_FRAMES = 8;
  static constexpr uint8_t HEAT_ICON_SIZE = 14;
  static constexpr uint8_t HEAT_ICON_GAP = 2;
  
  static constexpr uint8_t FONT_HEIGHT = 12;
  
public:
  void showStartupLogo(uint16_t durationMs = 1500) {
    turnOn();
    display.clearBuffer();
    drawMonochromeMSB(display, 0, 0, start_image_width, start_image_height, start_image_data);
    display.sendBuffer();
    delay(durationMs);
  }


  // Zeigt erst das Logo und startet dann die Diashow
  void starteAnfangsShow(RuntimeState& state) {
    turnOn();
    
    // 1. Logo für 3 Sekunden anzeigen
    display.clearBuffer();
    drawMonochromeMSB(display, 0, 0, start_image_width, start_image_height, start_image_data);
    display.sendBuffer();
    delay(3000); 
    if (state.restzeit >= 3) state.restzeit -= 3;

    // 2. Jetzt das normale Karussell (2 Runden)
    zeigeSeitenKarussell(state, 2);
  }
  // SW_I2C constructor: (rotation, reset, clock, data)
  DisplayManager() : display(U8G2_R0, I2C_SCL, I2C_SDA, U8X8_PIN_NONE) {}
  
  bool initialize(bool keepOn = false) {
    if (!display.begin()) {
      Serial.println("! OLED initialization failed");
      return false;
    }
    if (keepOn) {
      display.setPowerSave(0);
      displayActive = true;
      displayOnTime = millis() / 1000;
    } else {
      display.setPowerSave(1);  // Off by default
      displayActive = false;
    }
    return true;
  }
  
  void turnOn() {
    if (!displayActive) {
      display.setPowerSave(0);
      displayActive = true;
      displayOnTime = millis() / 1000;
    }
  }
  
  void turnOff() {
    if (displayActive) {
      display.setPowerSave(1);
      displayActive = false;
    }
  }
  
  bool isActive() const { return displayActive; }
  void setEspNowLinkAlive(bool alive) { espNowLinkAlive = alive; }
  void setCurrentPage(DisplayPage page) { currentPage = page; }
  DisplayPage getCurrentPage() const { return currentPage; }
  
  void updateAutoOff() {
    if (!displayActive) return;
    uint32_t now = millis() / 1000;
    if (now - displayOnTime > DISPLAY_TIMEOUT_S) {
      turnOff();
    }
  }
  
  void showMessage(const char* line1, const char* line2) {
    turnOn();
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(0, 25, line1);
    display.drawStr(0, 45, line2);
    display.sendBuffer();
    delay(2000);
    turnOff();
  }

  void showPersistentStatus(const char* line1, const char* line2) {
    turnOn();
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(0, 24, line1);
    display.drawStr(0, 44, line2);
    display.sendBuffer();
  }
  
  void update(const RuntimeState& state) {
    if (!displayActive) return;
    
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB08_tr);
    
    switch (currentPage) {
      case PAGE_STATUS: drawPageStatus(state); break;
      case PAGE_ENVIRONMENT: drawPageEnvironment(state); break;
      case PAGE_BATTERY: drawPageBattery(state); break;
      default: currentPage = PAGE_STATUS; drawPageStatus(state); break;
    }
    
    drawPageIndicator();
    drawHeatingVisualFeedback(state);
    display.sendBuffer();
    currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
  }
  
private:

  void drawPageStatus(const RuntimeState& state) {
    display.drawStr(0, 12, "Weba Thermo W-Link");
    char buffer[DISPLAY_BUFFER_SIZE];
    if (state.restzeit > 0) {
      display.drawStr(0, 28, "Heizung AN");
      uint16_t minutes = state.restzeit / 60;
      uint16_t seconds = state.restzeit % 60;
      snprintf(buffer, sizeof(buffer), "%02u:%02u", minutes, seconds);
      display.drawStr(85, 28, buffer);
    } else {
      display.drawStr(0, 28, "Heizung AUS");
    }
    snprintf(buffer, sizeof(buffer), "Batt: %.1fV", state.batterieSpannung);
    display.drawStr(0, 55, buffer);
  }
  
  void drawPageEnvironment(const RuntimeState& state) {
    display.drawStr(0, 12, "Umgebung");
    char buffer[DISPLAY_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "T: %.1fC", state.innenTemperatur);
    display.drawStr(0, 28, buffer);
    snprintf(buffer, sizeof(buffer), "H: %.0f%%", state.innenLuftfeuchtigkeit);
    display.drawStr(0, 44, buffer);
    snprintf(buffer, sizeof(buffer), "P: %.0f hPa", state.luftdruck);
    display.drawStr(0, 60, buffer);
  }
  
  void drawPageBattery(const RuntimeState& state) {
    display.drawStr(0, 12, "Batterie");
    char buffer[DISPLAY_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "U: %.2fV", state.batterieSpannung);
    display.drawStr(0, 28, buffer);
    float currentMa = state.batterieStrom * 1000.0f;
    snprintf(buffer, sizeof(buffer), "I: %.0fmA", currentMa);
    display.drawStr(0, 44, buffer);
    snprintf(buffer, sizeof(buffer), "P: %.1fW", state.batterieLeistung);
    display.drawStr(0, 60, buffer);
  }
  
  void drawPageIndicator() {
    uint8_t dotX = 60;
    for (uint8_t i = 0; i < (uint8_t)PAGE_COUNT; i++) {
      if (i == (uint8_t)currentPage) {
        display.drawBox(dotX, 62, 3, 2);
      } else {
        display.drawFrame(dotX, 62, 3, 2);
      }
      dotX += 6;
    }

    drawEspNowLinkIcon10x10(display, 117, 1, espNowLinkAlive);
  }

  void drawHeatingVisualFeedback(const RuntimeState& state, bool animate = true) {
    if (state.restzeit == 0) {
      return;
    }

    const uint8_t y = 64 - 2 - HEAT_ICON_SIZE;
    const uint8_t xFan = 128 - 2 - HEAT_ICON_SIZE;
    const uint8_t xFire = xFan - HEAT_ICON_GAP - HEAT_ICON_SIZE;

    drawFireIcon14x14(display, xFire, y, fireFrame);
    drawFanIcon14x14(display, xFan, y, fanFrame);

    if (animate) {
      fanFrame = (uint8_t)((fanFrame + 1) % FAN_FRAMES);
      fireFrame = (uint8_t)((fireFrame + 1) % 4);
    }
  }

  void drawRemainingTimeOnly(const RuntimeState& state) {
    char buffer[DISPLAY_BUFFER_SIZE];
    uint16_t minutes = state.restzeit / 60;
    uint16_t seconds = state.restzeit % 60;
    snprintf(buffer, sizeof(buffer), "%02u:%02u", minutes, seconds);
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(0, 18, "Restzeit");
    drawEspNowLinkIcon10x10(display, 117, 1, espNowLinkAlive);
    display.setFont(u8g2_font_logisoso20_tn);
    display.drawStr(0, 56, buffer);
  }

public:
  void zeigeSeitenKarussell(RuntimeState& state, int runden) {
    turnOn(); // Sicherstellen, dass das Display Saft hat
    const uint16_t pageDurationMs = 3000;
    const uint16_t frameDelayMs = 35;

    for (int i = 0; i < runden; i++) {
      uint32_t pageStartRest = state.restzeit;
      uint32_t startMs = millis();
      while ((millis() - startMs) < pageDurationMs) {
        uint32_t elapsedSeconds = (millis() - startMs) / 1000UL;
        state.restzeit = (pageStartRest > elapsedSeconds) ? (pageStartRest - elapsedSeconds) : 0;
        display.clearBuffer();
        display.setFont(u8g2_font_ncenB08_tr);
        drawPageStatus(state);
        drawHeatingVisualFeedback(state, true);
        display.sendBuffer();
        delay(frameDelayMs);
      }
      uint32_t elapsedSeconds = (millis() - startMs) / 1000UL;
      state.restzeit = (pageStartRest > elapsedSeconds) ? (pageStartRest - elapsedSeconds) : 0;

      pageStartRest = state.restzeit;
      startMs = millis();
      while ((millis() - startMs) < pageDurationMs) {
        uint32_t elapsedSecondsPage = (millis() - startMs) / 1000UL;
        state.restzeit = (pageStartRest > elapsedSecondsPage) ? (pageStartRest - elapsedSecondsPage) : 0;
        display.clearBuffer();
        display.setFont(u8g2_font_ncenB08_tr);
        drawPageEnvironment(state);
        drawHeatingVisualFeedback(state, true);
        display.sendBuffer();
        delay(frameDelayMs);
      }
      elapsedSeconds = (millis() - startMs) / 1000UL;
      state.restzeit = (pageStartRest > elapsedSeconds) ? (pageStartRest - elapsedSeconds) : 0;

      pageStartRest = state.restzeit;
      startMs = millis();
      while ((millis() - startMs) < pageDurationMs) {
        uint32_t elapsedSecondsPage = (millis() - startMs) / 1000UL;
        state.restzeit = (pageStartRest > elapsedSecondsPage) ? (pageStartRest - elapsedSecondsPage) : 0;
        display.clearBuffer();
        display.setFont(u8g2_font_ncenB08_tr);
        drawPageBattery(state);
        drawHeatingVisualFeedback(state, true);
        display.sendBuffer();
        delay(frameDelayMs);
      }
      elapsedSeconds = (millis() - startMs) / 1000UL;
      state.restzeit = (pageStartRest > elapsedSeconds) ? (pageStartRest - elapsedSeconds) : 0;
    }
  }

  void zeigeHeizFeedback(RuntimeState& state, uint16_t sekunden) {
    if (state.restzeit == 0) {
      return;
    }

    turnOn();
    uint32_t startRest = state.restzeit;
    uint32_t startMs = millis();

    while ((millis() - startMs) < ((uint32_t)sekunden * 1000UL)) {
      uint32_t elapsedSeconds = (millis() - startMs) / 1000UL;
      state.restzeit = (startRest > elapsedSeconds) ? (startRest - elapsedSeconds) : 0;
      display.clearBuffer();
      drawRemainingTimeOnly(state);
      drawHeatingVisualFeedback(state, true);
      display.sendBuffer();
      delay(35);
    }

    uint32_t elapsedSeconds = (millis() - startMs) / 1000UL;
    state.restzeit = (startRest > elapsedSeconds) ? (startRest - elapsedSeconds) : 0;
  }

  void showHeatingTimeOnly(const RuntimeState& state) {
    showHeatingTimeAnimated(state, 1000, 35);
  }

  void showHeatingTimeFrame(const RuntimeState& state) {
    if (state.restzeit == 0) {
      return;
    }

    turnOn();
    display.clearBuffer();
    drawRemainingTimeOnly(state);
    drawHeatingVisualFeedback(state, true);
    display.sendBuffer();
  }

  void showHeatingTimeAnimated(const RuntimeState& state, uint16_t durationMs, uint16_t frameDelayMs) {
    if (state.restzeit == 0) {
      return;
    }

    turnOn();
    uint32_t startMs = millis();
    while ((millis() - startMs) < durationMs) {
      showHeatingTimeFrame(state);
      delay(frameDelayMs);
    }
  }

  void showHeaterOffFeedback() {
    turnOn();
    uint32_t startMs = millis();
    const uint16_t durationMs = 1800;
    const uint16_t frameDelayMs = 80;
    while ((millis() - startMs) < durationMs) {
      display.clearBuffer();
      display.setFont(u8g2_font_ncenB08_tr);
      display.drawStr(0, 18, "Heizung AUS");
      display.drawStr(0, 34, "Luefter stoppt");
      drawFanIcon14x14(display, 128 - 2 - HEAT_ICON_SIZE, 64 - 2 - HEAT_ICON_SIZE, fanFrame);
      if ((millis() - startMs) < 900) {
        fanFrame = (uint8_t)((fanFrame + 1) % FAN_FRAMES);
      }
      display.sendBuffer();
      delay(frameDelayMs);
    }
    turnOff();
  }
};

#endif