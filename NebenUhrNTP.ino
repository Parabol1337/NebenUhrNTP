/*
 * Nebenuhr-Taktgeber (ESP32-EVB, ESP32 Core 3.x)
 * - 1 Linie (Relais A/B), 60V Impuls TOGGLE
 * - Zwei Uhren: NTP (lokal) & Nebenuhr (virtuell 0..719)
 * - Reconcile: delta=(NTP-Nebenuhr) mod 720 -> bei delta>0 FastStep (~4 Hz)
 * - WebUI responsiv (FastStep in FreeRTOS-Task, Abbruch möglich)
 * - NVS: virtueller Index + Polarität werden regelmäßig gespeichert
 * - Ethernet (LAN8720), Basic-Auth, Konfig-UI
 * - Long-Press (GPIO34 ~5s) => Werkseinstellungen + Neustart
 * - NTP-Ausfall: Uhr tickt lokal weiter; UI zeigt Status & letzten NTP-Sync
 * - Erweiterte Serial-Logs + /logs (Ringpuffer) + /logs.txt
 */

#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <time.h>
#include <esp_sntp.h>
#include <stdarg.h>

#define RELAY_A      32
#define RELAY_B      33
#define RESET_BTN    34

WebServer server(443);
#define USE_SD_MMC 1

#if USE_SD_MMC
  #include <SD_MMC.h>
  #define SD_FS SD_MMC
  const char* SD_MOUNT = "/sdcard";
#else
  #include <SD.h>
  #define SD_FS SD
  const int SD_CS = 5;
  const char* SD_MOUNT = "/sd";
#endif

static bool sdReady = false;
static const char* CFG_DIR  = "/nebenuhr";
static const char* CFG_FILE = "/nebenuhr/config.ini";
static const char* LOG_FILE = "/nebenuhr/logs.txt";

/* Vollständiger Sketch aus dem bereitgestellten Projektarchiv. */
