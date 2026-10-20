---
name: Fehlerbericht (Bug)
about: Etwas funktioniert nicht wie erwartet
title: "[Bug] Kurze Beschreibung"
labels: bug
assignees: ''
---

<!--
Drei Angaben entscheiden, ob ein Fehler in Minuten oder in Tagen gefunden wird:
Erwartet/Gemessen, die serielle Ausgabe und der Aufbau. Bitte ausfuellen, auch
wenn es nach Kleinkram aussieht. Gutes Beispiel: docs/ina226-busvoltage/README.md
-->

**Was funktioniert nicht?**
Ein bis zwei Sätze: Was sollte passieren, was passiert stattdessen?

**Erwartet / Gemessen** *(Pflichtfeld — Zahlen mit Einheit, nicht nur "falsch")*

| Größe | Erwartet | Gemessen |
|---|---|---|
| z. B. Busspannung | 12,0 V | 1,5 V |
|  |  |  |

Woher kommt der Erwartungswert? (Multimeter, Netzteil-Display, Datenblatt, Vergleichsmessung)

**Serielle Ausgabe** *(Pflichtfeld — Baudrate nicht vergessen)*
```
Baudrate: 115200
Vollständige Ausgabe vom Start bis zum Fehler
```

**Hardware-Aufbau** *(Pflichtfeld)*
- Board / Modul (genaue Bezeichnung, z. B. NodeMCU v3, ESP32-C3 QtPy):
- Verdrahtung: Pin-Belegung, Versorgung, Shunt, Messpunkt (IN+/IN-/VBUS):
- Skizze, Foto oder Schaltplan: (Bild anhängen oder Link — siehe Hinweis unten)

**Firmware-Version**
- Commit oder Release (`git rev-parse --short HEAD` oder Tag):
- Build: PlatformIO-Environment (z. B. `esp8266`, `esp8266-debug`) und geänderte `build_flags`:
- Bibliotheksversionen, falls abweichend (`platformio.ini` / `lib_deps`):

**Reproduktion**
1. ...
2. ...
3. ...

Lässt es sich zuverlässig auslösen (immer / sporadisch / nur nach Kaltstart)?

**Bereits geprüft**
Was hast du schon ausgeschlossen? (anderes Kabel, andere Versorgung, Bauteil getauscht,
älteren Commit getestet, ...)

---

**Bitte beachten**
- **Keine MAC-Adressen, IP-Adressen, Zugangsdaten oder Netzwerkkonfiguration** in
  öffentliche Issues — siehe `SECURITY.md`.
- **Sicherheitslücken nicht als Issue melden**, sondern über *Security → Report a
  vulnerability* (nur für den Repository-Besitzer sichtbar).
- Fragen und Diskussion dürfen gern auch per E-Mail kommen — Fehler aber bitte hier,
  damit sie auffindbar bleiben und andere davon profitieren.
