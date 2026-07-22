# NebenUhrNTP

ESP32-basierter Taktgeber für eine elektromechanische Nebenuhr. Das Projekt synchronisiert die Sollzeit per NTP und steuert die Uhr über zwei abwechselnd geschaltete Relaisausgänge.

## Funktionen

- NTP-Zeitsynchronisation über Ethernet
- wechselnde Relaispolarität für Minutenimpulse
- konfigurierbare Impulsdauer von 100 bis 400 ms
- automatische Nachführung und schneller Nachlauf
- Weboberfläche für Status, Konfiguration, Logs und Zeitabgleich
- DHCP oder statische IPv4-Konfiguration
- Zeitzonen inklusive Sommerzeitregeln
- HTTP Basic Authentication
- Konfiguration und Logs auf SD-Karte
- Werksreset über langen Tastendruck
- lokaler Weiterlauf bei vorübergehendem NTP-Ausfall

## Hardware

Das Projekt ist für ein ESP32-EVB mit LAN8720-Ethernet ausgelegt.

| Funktion | GPIO |
|---|---:|
| Relais A | 32 |
| Relais B | 33 |
| Reset-Taster | 34 |
| Ethernet MDC | 23 |
| Ethernet MDIO | 18 |
| Ethernet-Clock | GPIO0 Eingang |

Die Relaisausgänge dürfen die Nebenuhrleitung nicht direkt treiben. Für die vorgesehene 60-V-Impulsleitung ist eine geeignete, galvanisch getrennte und ausreichend dimensionierte Leistungsschaltung erforderlich.

## Voraussetzungen

- ESP32 Arduino Core 3.x
- ESP32-EVB oder kompatible LAN8720-Hardware
- SD-Karte im SD-MMC-Modus
- Ethernet-Verbindung mit erreichbarem NTP-Server
- geeignete Relais- oder Treiberstufe

Verwendete Core-Bibliotheken:

- `WiFi`
- `ETH`
- `WebServer`
- `SD_MMC`
- `esp_sntp`

## Installation

1. Repository klonen oder herunterladen.
2. `NebenUhrNTP.ino` in der Arduino IDE öffnen.
3. Ein kompatibles ESP32-Board mit ESP32 Arduino Core 3.x auswählen.
4. SD-Karte einsetzen und Ethernet anschließen.
5. Sketch kompilieren und übertragen.
6. Serielle Ausgabe mit 115200 Baud öffnen.
7. Die ausgegebene IP-Adresse im Browser aufrufen.

## Code-Struktur

Der Einstiegssketch enthält nur noch die vier Hauptmodule:

```cpp
#include "src/CoreAndConfiguration.h"
#include "src/ClockController.h"
#include "src/WebInterface.h"
#include "src/Application.h"
```

Die Verantwortlichkeiten sind damit klar getrennt:

| Modul | Aufgabe |
|---|---|
| `CoreAndConfiguration.h` | Hardwaredefinitionen, SD-Karte, Konfiguration, Status, Logging, Ethernet und NTP |
| `ClockController.h` | Relaisimpulse, Minutentakt, Zeitabgleich und FastStep |
| `WebInterface.h` | HTML-Helfer, Statusseite, Konfiguration, Logs und Web-Aktionen |
| `Application.h` | `setup()`, `loop()`, Routenregistrierung und Reset-Taster |

Die vorhandenen `part*.inc`-Dateien bleiben als unveränderte Implementierungsfragmente eingebunden. Dadurch wurde die öffentliche Struktur lesbarer, ohne die getestete Programmlogik oder Reihenfolge zu verändern.

## Erster Start

Eine SD-Karte ist zwingend erforderlich. Kann sie nicht eingebunden werden, hält die Firmware den Start an.

Beim ersten Start werden Standardwerte in `/nebenuhr/config.ini` geschrieben:

- Benutzer: `admin`
- Passwort: `admin`
- Hostname: `Nebenuhr`
- NTP-Server: `rb-time.de.bosch.com`
- Zeitzone: Deutschland mit MEZ/MESZ
- Impulsdauer: 200 ms
- NTP-Synchronisationsintervall: 60 Minuten

Die Standardzugangsdaten sollten direkt nach der Inbetriebnahme geändert werden.

## Weboberfläche

Die Firmware startet einen normalen HTTP-Webserver auf Port `443`; TLS ist nicht implementiert.

```text
http://<IP-Adresse>:443/
```

Routen:

- `/` – Systemstatus und Zeitabgleich
- `/config` – Netzwerk-, NTP-, Zeitzonen- und Impulseinstellungen
- `/logs` – Protokollanzeige
- `/logs.txt` – Protokoll als Text
- `/st` – FastStep-Status als JSON
- `/abort` – FastStep abbrechen

## Zeitabgleich

Die Nebenuhr wird intern als Minutenindex von `0` bis `719` geführt. Nach dem ersten NTP-Sync wird die aktuell angezeigte Zeit der realen Nebenuhr in der Weboberfläche eingetragen.

Hinkt die Nebenuhr hinterher, erzeugt die Firmware zusätzliche Impulse. Bei kleiner Abweichung nach vorn wartet sie auf die Sollzeit; bei größerer Abweichung wird die Uhr mechanisch sicher vorwärts nachgeführt.

## Persistenz und Logs

Auf der SD-Karte werden folgende Dateien angelegt:

```text
/nebenuhr/config.ini
/nebenuhr/logs.txt
```

Gespeichert werden unter anderem Netzwerkeinstellungen, NTP-Konfiguration, Impulsdauer, interner Minutenindex und Relaispolarität.

## Werksreset

GPIO 34 ungefähr fünf Sekunden gedrückt halten. Anschließend werden die Standardwerte gespeichert und der ESP32 neu gestartet.

## Projektstruktur

```text
NebenUhrNTP.ino
src/
  CoreAndConfiguration.h
  ClockController.h
  WebInterface.h
  Application.h
  part01.inc ... part07.inc
README.md
.gitignore
```

## Sicherheitshinweise

- Die Nebenuhrleitung kann mit gefährlicher Spannung betrieben werden.
- Nur geeignete Schutz- und Treiberschaltungen verwenden.
- Vor Arbeiten an der Verdrahtung die Versorgung vollständig trennen.
- Basic Authentication wird ohne TLS verwendet; die Oberfläche nur in einem vertrauenswürdigen lokalen Netzwerk betreiben.
- Standardzugangsdaten nach dem ersten Start ändern.

## Lizenz

Für dieses Projekt ist derzeit keine ausdrückliche Lizenz hinterlegt. Ohne Lizenz bleiben alle Rechte beim Urheber.
