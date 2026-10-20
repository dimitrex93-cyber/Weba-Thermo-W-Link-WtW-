# Fix: INA226 meldete die Busspannung um Faktor 8 zu klein

**Status:** behoben am 20.10.2026 · **Betroffen:** `include/ina226_optimized.h` · **Auswirkung:** jede Spannungsmessung, damit auch die Akku-Schwellen

## Symptom (per E-Mail gemeldet)

Ein Nutzer baut das Projekt als Steuergerät für die Standheizung seines Land Rover nach
(Testjig: „Kraftwerk" und „Cockpit" zusammengesteckt, ohne RJ45; der INA226 versorgt und
misst dabei nur die Controller-Schaltung selbst). Sein Befund:

- Netzteil liefert **12 V**, der INA226 meldet **1,5 V**
- Strom und Leistung sind dagegen **plausibel**: ca. 40 mA / 0,5 W, passend zur Anzeige des Netzteils
- serielle Ausgabe: `INA226 calibration: 0x0347 (839)` und `RTC data invalid, resetting`

## Ursache

`include/ina226_optimized.h` rechnete die Busspannung mit

```cpp
voltage = (busVoltageRaw >> 3) * BUS_VOLTAGE_LSB;   // falsch für INA226
```

Das `>> 3` ist das **Registerformat des INA219** (dort liegen die Nutzbits in 15…3, LSB 4 mV).
Beim **INA226** ist Register `0x02` ein **volles 16-Bit-Wort mit 1,25 mV je LSB** (Bereich
0…40,96 V) — ein Shift ist nicht nötig und teilt den Wert durch 8:

**12 V / 8 = 1,5 V** — exakt der gemeldete Wert.

Dass die Ursache im Treiber und nicht in der Verdrahtung liegt, zeigt die Kalibrierung: der
gemeldete Wert `0x0347` (839) entspricht genau den Projektwerten
`INA226_SHUNT_RESISTOR 0.01 Ω` und `INA226_MAX_CURRENT 20 A`
(`CAL = 0.00512 / (currentLSB · R_shunt)` mit `currentLSB = 20 A / 32768 = 610 µA` → 839).
I²C-Verbindung, Shunt und Kalibrierung waren also in Ordnung. Strom (`0x04`) und Leistung
(`0x03`) kommen aus eigenen Registern ohne Shift — deshalb waren sie korrekt.

## Warum das mehr als kosmetisch war

`include/config.h` definiert `BATTERY_LOW_VOLTAGE 10.5` und `BATTERY_CRITICAL_VOLTAGE 9.5`.
Mit einem Messwert von 1,5 V hielt die Firmware den Akku **dauerhaft für tiefentladen**;
alle daran hängenden Entscheidungen (Akkugesundheit, Schwellenlogik) arbeiteten mit falschen
Daten.

## Änderung

`include/ina226_optimized.h`:

1. `(raw >> 3)` in `readAll()` und `readBusVoltage()` entfernt.
2. Busspannung und Leistung werden als **`uint16_t`** gelesen (beide Register sind laut
   Datenblatt unsigned; nur Strom und Shuntspannung sind vorzeichenbehaftet). Neuer Helfer
   `readRegisterU16()`.
3. Plausibilitätsgrenze: über `BUS_VOLTAGE_MAX` (40,96 V = ADC-Vollausschlag) gilt die Messung
   als ungültig → `readAll()` liefert `false`, es läuft kein Fantasiewert in die Schwellen.
4. Kommentar korrigiert und Datenblattbezug (TI SBOS547, Bus-Voltage-Register) ergänzt, damit
   der INA219-Shift nicht per Copy-Paste zurückkehrt.

## Verifikation

Register-/Umrechnungswerte (`BUS_VOLTAGE_LSB = 1,25 mV`):

| Soll | Registerwert | alt (`>> 3`) | neu |
|---|---|---|---|
| 12,0 V | 9600 (`0x2580`) | 1,500 V | **12,000 V** |
| 5,0 V | 4000 (`0x0FA0`) | 0,625 V | **5,000 V** |
| 3,3 V | 2640 (`0x0A50`) | 0,412 V | **3,300 V** |
| 13,8 V | 11040 (`0x2B20`) | 1,725 V | **13,800 V** |

Am Testjig zu prüfen: 12 V anlegen, `readAll()` liefert 12,0 V ±0,1 V, Strom und Leistung
bleiben unverändert (ca. 40 mA / 0,5 W). Register `0x02` muss roh `0x2580` lesen.

## Zur zweiten Meldung: `RTC data invalid, resetting`

Die Zeile stammt aus `include/data_manager.h` und betrifft den **ESP8266-RTC-User-Memory**
(CRC32-geprüft), nicht eine externe RTC. Nach einem echten Stromausfall ist dieser Speicher
leer, die Prüfsumme passt nicht und die Meldung ist erwartbar — kein Fehler.

## Schaltplan des Melde-Aufbaus

`schaltplan-melde-aufbau.pdf` und `schaltplan-melde-aufbau.png` zeigen den Aufbau, mit dem der
Fehler gemeldet wurde (KiCad, Stand 15.10.2026). Die Datei enthält **keine personenbezogenen
Daten** — der Titelblock führt nur Projektname, Datum und Revision; Name und Anschrift des
Melders werden bewusst nicht veröffentlicht.
