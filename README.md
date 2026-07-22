# NebenUhrNTP

ESP32-basierter Taktgeber für eine elektromechanische Nebenuhr. Das Projekt synchronisiert die interne Sollzeit per NTP und steuert eine Nebenuhr über zwei abwechselnd geschaltete Relaisausgänge.

## Funktionen

- NTP-Zeitsynchronisation über Ethernet
- Ansteuerung einer Nebenuhr mit wechselnder Relaispolarität
- konfigurierbare Impulsdauer von 100 bis 400 ms
- automatische Nachführung der Nebenuhr
- schneller Nachlauf mit etwa vier Impulsen pro Sekunde
- Weboberfläche für Status, Konfiguration und Zeitabgleich
- DHCP oder statische IPv4-Konfiguration
- auswählbare Zeitzonen inklusive Sommerzeitregeln
- HTTP Basic Authentication
- persistente Konfiguration und Protokollierung auf SD-Karte
- Loganzeige im Browser und als Textdatei
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

Die Relaisausgänge dürfen die Nebenuhrleitung nicht direkt treiben. Für die im Projekt vorgesehene 60-V-Impulsleitung ist eine geeignete, galvanisch getrennte und ausreichend dimensionierte Leistungsschaltung erforderlich.

## Voraussetzungen

- ESP32 Arduino Core 3.x
- ESP32-EVB beziehungsweise kompatible LAN8720-Hardware
- SD-Karte im SD-MMC-Modus
- Ethernet-Verbindung mit erreichbarem NTP-Server
- geeignete Relais- oder Treiberstufe für die Nebenuhr

Es werden ausschließlich Bibliotheken aus dem ESP32-Arduino-Core verwendet:

- `WiFi`
- `ETH`
- `WebServer`
- `SD_MMC`
- `esp_sntp`

## Installation

1. Repository herunterladen oder klonen.
2. Den Ordner `NebenUhrNTP` in der Arduino IDE öffnen.
3. Ein kompatibles ESP32-Board und den ESP32 Arduino Core 3.x auswählen.
4. SD-Karte einsetzen und Ethernet anschließen.
5. Sketch kompilieren und auf den ESP32 übertragen.
6. Serielle Ausgabe mit 115200 Baud öffnen.
7. Die vergebene IP-Adresse im Browser aufrufen.

Der Sketch ist in mehrere eingebundene Dateien unter `src/` aufgeteilt. Diese Dateien bilden zusammen unverändert den vollständigen Originalsketch aus dem Projektarchiv.

## Erster Start

Eine SD-Karte ist zwingend erforderlich. Kann sie nicht eingebunden werden, hält die Firmware den Start an.

Beim ersten Start werden Standardwerte in `/nebenuhr/config.ini` geschrieben. Die Weboberfläche verwendet standardmäßig:

- Benutzer: `admin`
- Passwort: `admin`
- Hostname: `Nebenuhr`
- NTP-Server: `rb-time.de.bosch.com`
- Zeitzone: Deutschland mit MEZ/MESZ
- Impulsdauer: 200 ms
- NTP-Synchronisationsintervall: 60 Minuten

Diese Zugangsdaten sollten direkt nach der Inbetriebnahme geändert werden.

## Weboberfläche

Die Firmware startet einen Webserver auf Port `443`. Der Sketch verwendet dabei normalen HTTP-Verkehr und implementiert keine TLS-Verschlüsselung. Der Aufruf erfolgt daher je nach Browser und Netzwerk typischerweise über:

```text
http://<IP-Adresse>:443/
```

Verfügbare Bereiche:

- `/` – Systemstatus und Zeitabgleich
- `/config` – Netzwerk-, NTP-, Zeitzonen- und Impulseinstellungen
- `/logs` – laufende Protokollanzeige
- `/logs.txt` – Protokoll als Text
- `/st` – FastStep-Status als JSON
- `/abort` – laufenden FastStep abbrechen

## Zeitabgleich

Die Nebenuhr wird intern als Minutenindex von `0` bis `719` geführt. Nach dem ersten NTP-Sync muss die aktuell angezeigte Zeit der realen Nebenuhr in der Weboberfläche eingetragen werden.

Hinkt die Nebenuhr hinterher, erzeugt die Firmware zusätzliche Impulse. Ist sie nur wenige Minuten voraus, wartet die Firmware, bis die Sollzeit sie wieder erreicht. Bei einer größeren Abweichung nach vorn wird die Uhr mechanisch sicher vorwärts bis zur Sollzeit weitergeführt.

## Persistenz und Logs

Die Firmware legt auf der SD-Karte folgende Dateien an:

```text
/nebenuhr/config.ini
/nebenuhr/logs.txt
```

Gespeichert werden unter anderem Netzwerkeinstellungen, NTP-Konfiguration, Impulsdauer, interner Minutenindex und zuletzt verwendete Relaispolarität.

## Werksreset

Der Taster an GPIO 34 muss ungefähr fünf Sekunden gedrückt gehalten werden. Danach werden die Standardwerte gespeichert und der ESP32 neu gestartet.

## Sicherheitshinweise

- Die Nebenuhrleitung kann mit gefährlicher Spannung betrieben werden.
- Aufbau und Inbetriebnahme dürfen nur mit geeigneter Schutzbeschaltung erfolgen.
- Vor Arbeiten an der Verdrahtung muss die Versorgung vollständig getrennt werden.
- Die Weboberfläche nutzt Basic Authentication ohne TLS. Sie sollte nur in einem vertrauenswürdigen lokalen Netzwerk verwendet werden.
- Standardzugangsdaten nach dem ersten Start ändern.

## Projektstruktur

```text
NebenUhrNTP.ino
src/
  part01.inc
  part02.inc
  part03.inc
  part04.inc
  part05a.inc
  part05b.inc
  part06.inc
  part07.inc
README.md
.gitignore
```

## Lizenz

Für dieses Projekt ist derzeit keine ausdrückliche Lizenz hinterlegt. Ohne Lizenz bleiben alle Rechte beim Urheber.
