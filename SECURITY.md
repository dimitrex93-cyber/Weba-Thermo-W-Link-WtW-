# Sicherheitsrichtlinie

## Eine Schwachstelle melden

**Bitte niemals Sicherheitslücken als öffentliches Issue melden** – dadurch würden
Angreifer auf das Problem aufmerksam, bevor es behoben ist.

Stattdessen:

1. **Privat melden:** GitHub → Repository → **Security** → **Report a vulnerability**
   (Private Vulnerability Reporting). Die Meldung ist nur für den Repository-Besitzer
   sichtbar.
2. **Falls das private Reporting nicht verfügbar ist:** Schreibe eine E-Mail über die
   GitHub-Profile-Seite des Besitzers oder erwähne die Schwachstelle in einem
   regulären Issue NUR mit allgemeiner Beschreibung ohne technische Details.

## Besonderheit eingebetteter Systeme (ESP32/ESP8266)

- Dieses Projekt steuert reale Hardware (Heizungsregler, Fernbedienung).
  Änderungen am Bus-Protokoll oder an den Pin-Definitionen können physische
  Geräte beeinträchtigen – bitte immer im Detail dokumentieren.
- Enthält der Code Geräte-Adressen (MAC-Adressen) oder Netzwerk-Konfiguration,
  gehören diese NICHT in öffentliche Issues.

## Was in der Meldung enthalten sein sollte

- Betroffene Datei/Version und Modul (z. B. W-Bus, ESP-NOW, Display)
- Beschreibung der Schwachstelle und mögliche Auswirkungen
- Reproduktionsschritte (ohne echte Daten/Adressen zu veröffentlichen)
- Vorschlag für einen Fix (optional)

## Verhalten nach der Meldung

- Der Besitzer bestätigt den Eingang der Meldung innerhalb von ca. 5 Werktagen.
- Die Schwachstelle wird behoben und als Sicherheits-Update im Changelog/Release
  dokumentiert (Details erst nach dem Fix).
- Meldende Personen werden im Security Advisory als Reporter genannt (falls gewünscht).
