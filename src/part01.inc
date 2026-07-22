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

// -------- Hardware-Pins --------
#define RELAY_A      32
#define RELAY_B      33
#define RESET_BTN    34   // input-only

// -------- Web/SD --------
WebServer   server(443);
#define USE_SD_MMC 1              // 1 = SD_MMC (onboard), 0 = SPI

#if USE_SD_MMC
  #include <SD_MMC.h>
  #define SD_FS SD_MMC
  const char* SD_MOUNT = "/sdcard";
#else
  #include <SD.h>
  #define SD_FS SD
  const int SD_CS = 5;            // bei SPI anpassen
  const char* SD_MOUNT = "/sd";
#endif

static bool sdReady = false;
static const char* CFG_DIR  = "/nebenuhr";
static const char* CFG_FILE = "/nebenuhr/config.ini";
static const char* LOG_FILE = "/nebenuhr/logs.txt";


static bool ensureDir(const char* path){
  if (SD_FS.exists(path)) return true;
  return SD_FS.mkdir(path);
}

static void sdAppendLine(const char* path, const char* line){
  if (!sdReady) return;
  File f = SD_FS.open(path, FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
}

static void sdWriteText(const char* path, const String& content){
  if (!sdReady) return;
  File f = SD_FS.open(path, FILE_WRITE);
  if (!f) return;
  f.print(content);
  f.close();
}

static bool sdReadAll(const char* path, String& out){
  if (!sdReady || !SD_FS.exists(path)) return false;
  File f = SD_FS.open(path, FILE_READ);
  if (!f) return false;
  out = f.readString();
  f.close();
  return true;
}

static String ipToStr4(const uint8_t b[4]){
  char tmp[24];
  snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
  return String(tmp);
}

static void strToIp4(const String& s, uint8_t b[4]){
  int a[4]={0,0,0,0}, idx=0; String n="";
  for (size_t i=0;i<s.length() && idx<4;i++){
    char c=s[i];
    if (c=='.'){ a[idx++]=n.toInt(); n=""; }
    else if (isDigit((unsigned char)c)) n+=c;
  }
  if (idx<4) a[idx]=n.toInt();
  for (int i=0;i<4;i++){ b[i] = (uint8_t) constrain(a[i], 0, 255); }
}

// SD ist Pflicht: ohne SD kein Start
static void sdInitOrDie(){                 // <-- NEU
#if USE_SD_MMC
  sdReady = SD_MMC.begin("/sdcard", true); // 1-bit: true, 4-bit: false
#else
  sdReady = SD.begin(SD_CS, SPI, 40000000);
#endif
  if (!sdReady){
    Serial.println("[ERR] SD mount failed. System halted (no SD card).");
    while (true){ delay(1000); }
  }
  ensureDir(CFG_DIR);
  sdAppendLine(LOG_FILE, "---------------- Boot ----------------");
}


// -------- Konfiguration --------
struct Config {
  bool     dhcp;
  char     hostname[32];
  char     ntp[64];
  char     tz[64];              // POSIX TZ
  uint16_t pulse_ms;            // 100..400 ms
  uint16_t ntp_sync_min;        // 1..1440 Minuten
  char     user[16];
  char     pass[32];
  // statische IP
  uint8_t  ip[4], gw[4], mask[4], dns1[4], dns2[4];
} cfg;

IPAddress ipFrom(const uint8_t b[4]) { return IPAddress(b[0], b[1], b[2], b[3]); }
void ipToBytes(IPAddress ip, uint8_t b[4]) { for (int i=0;i<4;i++) b[i]=ip[i]; }

// -------- Status / Nebenuhr-State --------
volatile bool ethConnected = false;

struct LineState {
  uint16_t min_index;       // 0..719
  bool     lastPulseWasA;   // TOGGLE-Status
  uint8_t  save_div;        // alle N Minuten sichern
} L;

// -------- Async Stepper (FreeRTOS) --------
TaskHandle_t stepTaskHandle = nullptr;
volatile uint32_t step_target = 0;     // Ziel-Minuten (FastStep)
volatile uint32_t step_done   = 0;     // gelaufene Minuten
volatile bool     step_running = false;
volatile bool     step_abort   = false;
const uint32_t    STEP_HZ = 4;         // ~4 Impulse/Sek bei FastStep

// -------- Steuerflags/Timing --------
volatile bool pauseTick = false;       // kein Minutentick während FastStep
time_t lastAligned = 0;                // letzte verarbeitete volle Minute

// -------- NTP-Tracking --------
volatile time_t lastNtpSync = 0;       // letzter erfolgreicher NTP-Sync
uint32_t ntpSyncIntervalMs = 0;        // Intervall in ms

// -------- Zeitzonen-Dropdown --------
struct TZItem { const char* label; const char* posix; };
const TZItem TZ_LIST[] = {
  {"UTC (GMT0)",                  "GMT0"},
  {"Deutschland / MEZ/MESZ",     "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"UK / GMT mit BST",           "GMT0BST,M3.5.0/1,M10.5.0/2"},
  {"Osteuropa (EET/EEST)",       "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"USA Eastern",                "EST5EDT,M3.2.0,M11.1.0"},
