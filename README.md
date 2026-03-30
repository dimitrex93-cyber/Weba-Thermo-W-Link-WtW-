# weba-thermo-w-link
Intelligente W-Bus Steuerung für Webasto Thermo Top V im VW Touran 1T. 433 MHz Funkstart, 30-Minuten-Timer, OLED mit Temperatur &amp; Batterie-Status, DeepSleep-Modus. 3-Modul-Architektur (Motorraum/Innenraum), originale Fahrzeugverkabelung bleibt erhalten. Kostengünstige Telestart-Alternative zum Selberbauen. #DIY #Webasto #VW


# Weba Thermo W-Link – Intelligente W-Bus Steuerung
**Für Webasto Thermo Top V (z.B. VW Touran 1T)**

Eine selbstgebaute, kostengünstige und hochintelligente Steuerung für Webasto Standheizungen als Alternative zum teuren Telestart. Das System besteht aus einer 2-Modul-Architektur (Kraftwerk & Cockpit), die über ein standardmäßiges RJ45-Netzwerkkabel (Patchkabel) verbunden wird. 

## 1. Systemarchitektur & Hardware

Um Störungen durch den Fahrzeugmotor und die Wasserpumpe zu vermeiden, ist das System in zwei Gehäuse aufgeteilt:

### Gehäuse 1: "Das Kraftwerk" (Motorraum / Batterienähe)
Hier passiert die eigentliche Arbeit. Dieses Gehäuse wird versteckt verbaut.
* **Gehirn:** ESP8266 (NodeMCU).
* **Strommessung:** INA226-Modul (misst Batteriespannung und Stromverbrauch exakt).
* **W-Bus Kommunikation:** L9637D oder SI9241A Chip (übersetzt die Signale für die Webasto).
* **Stromversorgung:** Step-Down-Wandler (12V auf 5V/3.3V).
* **Sicherheit:** Abgesichert durch 5x20mm Keramik-/Glas-Feinsicherungen (2A) direkt am 12V-Abgriff und vor dem Step-Down-Wandler.

### Gehäuse 2: "Das Cockpit" (Innenraum)
Das reine Anzeige- und Bedienmodul für den Wohnraum/Fahrgastraum.
* **Anzeige:** 128x64 OLED-Display für Status, Countdown und Sensordaten.
* **Umweltsensoren:** AHT20 (Temperatur & Luftfeuchtigkeit) und BMP280 (Luftdruck).
* **Bedienung:** Taster zum manuellen Starten und Status-LEDs.

### Die Verbindung: Der RJ45 "Twisted Pair" Trick
Um den I2C-Bus (Display und Sensoren) über mehrere Meter stabil zu übertragen, wird ein 8-adriges RJ45-Kabel genutzt. Die verdrillten Adernpaare schirmen Störungen ab:
* **Paar 1 (Orange):** I2C Daten (SDA) + Masse (GND)
* **Paar 2 (Grün):** I2C Takt (SCL) + Masse (GND)
* **Paar 3 (Blau):** Stromversorgung (3.3V/5V) + Masse (GND)
* **Paar 4 (Braun):** Taster-Signal (Wake/Start) + Status-LED

---

## 2. Software-Logik & Sicherheit

Der Code läuft **nicht** im DeepSleep, sondern in einer durchgehenden Endlosschleife (`loop`). Das hat entscheidende Vorteile für die Stabilität des W-Bus:

1. **Der 30-Minuten-Timer:** Die Heizung läuft maximal 30 Minuten. Die Zeit wird präzise über die interne Uhr des ESP (`millis()`) heruntergezählt, ohne das System zu blockieren.
2. **Der "Heartbeat":** Die Webasto erwartet regelmäßige Signale. Der ESP sendet im Hintergrund kontinuierlich den "Heizen"-Befehl über den W-Bus. Bleibt das Signal aus, schaltet die Heizung sicherheitshalber ab.
3. **Der Hard-Stop:** Nach exakt 30 Minuten (oder bei manueller Abschaltung / Unterspannung) feuert der ESP einen harten W-Bus Stopp-Befehl und legt das Display schlafen.
4. **Batterieschutz:** Sinkt die Spannung der Autobatterie unter einen kritischen Wert (z.B. 11.5V), wird die Heizung sofort beendet, damit der Motor noch gestartet werden kann.

---

## 3. Die Blackbox (Log-Manager)

Das System verfügt über einen eingebauten Datenlogger. Der ESP8266 nutzt seinen internen Flash-Speicher (LittleFS) wie eine kleine Festplatte. 

* **Telemetrie:** Alle wichtigen Daten (Spannung, Strom, Temperatur, Restzeit) werden regelmäßig in einer CSV-Datei (z.B. `/logs/telemetry_2026-03.csv`) gespeichert.
* **Fehlerspeicher:** Startabbrüche, Kommunikationsfehler mit dem Brenner oder Unterspannung werden separat im Fehler-Log gesammelt. 
* **Vorteil:** Auch wenn die Autobatterie abgeklemmt wird, bleiben die Daten auf dem Chip erhalten. Sie können später zur Diagnose von Verschleiß (z.B. defekter Glühstift oder schwächelnde Batterie) ausgewertet werden.

---

## 4. Bedienung

* **Einschalten:** Taster drücken oder per 433 MHz Funkfernbedienung (simuliert einen Tastendruck).
* **Startsequenz:** Das System bootet, das Webasto-Logo erscheint auf dem OLED, die Sensoren werden initialisiert.
* **Heizbetrieb:** Das Display zeigt im Wechsel die Restzeit animiert an oder rotiert durch die Status-Seiten (Temperatur, Batterie, Status).
* **Abschalten:** Vorzeitiges Abschalten jederzeit durch erneuten Tastendruck möglich.

---

## 5. ESP-NOW Migrationsvorbereitung (ESP8266 <-> ESP32-C3)

Die Firmware enthaelt bereits eine vorbereitete ESP-NOW-Bruecke fuer eine spaetere smarte Fernbedienung mit ESP32-C3 und 0.96" SSD1306.

* Feature-Flag: `ENABLE_ESPNOW_LINK` in `include/config.h` (standardmaessig `0`, also aus).
* Geplante Fernbedienungsdaten: Innenraumtemperatur, Batteriespannung, Batterieleistung, Restlaufzeit, Heizstatus.
* Geplante Befehle von der Fernbedienung: Start, Stop, Status anfordern.

Sobald der ESP32-C3 fertig ist, kann das Flag aktiviert und die Peer-MAC in `include/config.h` eingetragen werden.

---

## 6. Pinout (ESP8266 NodeMCU)

* `D8` (`GPIO15`): W-Bus TX
* `D7` (`GPIO13`): W-Bus RX
* `D6` (`GPIO12`): I2C SDA (RJ45 / Display / Sensoren)
* `D5` (`GPIO14`): I2C SCL (RJ45 / Display / Sensoren)
* `D2` (`GPIO4`): LED Green
* `D1` (`GPIO5`): LED Yellow
* `RST`: 433 MHz Wake (Optokoppler)

## 7. Build & Flash

### Build

```bash
pio run -e esp8266
```

### Upload

```bash
pio run -t upload -e esp8266 --upload-port /dev/ttyUSB0
```

### Serial Monitor

```bash
pio device monitor -b 115200 --port /dev/ttyUSB0
```

## 8. ESP-NOW Kopplung (mit Weba-Remote)

In `include/config.h` muessen folgende Werte passen:

* `ENABLE_ESPNOW_LINK 1`
* `ESPNOW_WIFI_CHANNEL` identisch auf beiden Geraeten
* `ESPNOW_REMOTE_PEER_MAC_*` = MAC des ESP32-C3

## 9. Release

Dieses Repository ist fuer den initialen gemeinsamen Stand mit ESP32-C3-Remote als `v1.0.0` vorgesehen.
