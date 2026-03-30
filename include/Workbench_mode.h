#ifndef WERKBANK_H
#define WERKBANK_H

#include <Arduino.h>
#include <cmath>
#include "types.h"

// ============================================================
// DER HAUPTSCHALTER
// 1 = Liegt auf'm Schreibtisch (Fake-Werte AN)
// 0 = Ist im Auto eingebaut (Fake-Werte AUS)
// ============================================================
#define WORKBENCH_MODE 1

struct WerkbankState {
  bool heizungAktiv;
  uint32_t heizungStartMs;
};

inline WerkbankState& getWerkbankState() {
  static WerkbankState state = {false, 0};
  return state;
}

inline float clampWerkbank(float value, float minValue, float maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

inline void werkbankSetHeizung(bool aktiv) {
  #if WORKBENCH_MODE
  WerkbankState& wb = getWerkbankState();
  wb.heizungAktiv = aktiv;
  if (aktiv) {
    wb.heizungStartMs = millis();
  }
  #else
  (void)aktiv;
  #endif
}

inline bool werkbankIstHeizungAktiv() {
  #if WORKBENCH_MODE
  return getWerkbankState().heizungAktiv;
  #else
  return false;
  #endif
}

// Simuliert alle Sensorwerte robust und konsistent.
inline void ladeWerkbankWerte(RuntimeState& state) {
  #if WORKBENCH_MODE
  const float t = millis() / 1000.0f;
  const bool heizungAn = werkbankIstHeizungAktiv();
  float heizFaktor = 0.0f;

  if (heizungAn) {
    const float heizZeitS = (millis() - getWerkbankState().heizungStartMs) / 1000.0f;
    heizFaktor = clampWerkbank(heizZeitS / 90.0f, 0.0f, 1.0f);
  }

  state.innenTemperatur = 18.5f + (4.5f * heizFaktor) + 0.4f * sinf(t * 0.10f);
  state.innenLuftfeuchtigkeit = 55.0f - (10.0f * heizFaktor) + 1.8f * sinf(t * 0.07f);
  state.luftdruck = 1013.0f + 2.5f * sinf(t * 0.02f);

  if (heizungAn) {
    state.batterieSpannung = 12.4f - (0.3f * heizFaktor) + 0.05f * sinf(t * 0.35f);
    state.batterieStrom = 3.8f + 0.8f * sinf(t * 0.45f);
  } else {
    state.batterieSpannung = 12.8f + 0.04f * sinf(t * 0.20f);
    state.batterieStrom = 0.18f + 0.05f * sinf(t * 0.30f);
  }

  state.batterieSpannung = clampWerkbank(state.batterieSpannung, 11.8f, 13.2f);
  state.batterieStrom = clampWerkbank(state.batterieStrom, 0.05f, 6.0f);
  state.batterieLeistung = state.batterieSpannung * state.batterieStrom;

  state.aht20Status = SENSOR_OK;
  state.bmp280Status = SENSOR_OK;
  state.ina226Status = SENSOR_OK;
  #endif
}

// Diese Funktion lügt dem W-Bus vor, dass die Heizung geantwortet hat
inline bool fakeWebastoAntwort() {
  #if WORKBENCH_MODE
  return true;
  #else
  return false;
  #endif
}

#endif