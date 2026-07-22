  {"USA Central",                "CST6CDT,M3.2.0,M11.1.0"},
  {"USA Mountain",               "MST7MDT,M3.2.0,M11.1.0"},
  {"USA Pacific",                "PST8PDT,M3.2.0,M11.1.0"},
  {"China (keine DST)",          "CST-8"},
  {"Japan (keine DST)",          "JST-9"},
  {"Australien (Sydney)",        "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"GMT+1 ohne DST",             "GMT-1"},
  {"GMT+2 ohne DST",             "GMT-2"},
  {"GMT-3 ohne DST",             "GMT+3"},
};


static volatile bool needClockSet = false;
static volatile bool waitForNtp = true;

// ---------------- Logging (Serial + Ringpuffer) ----------------
#define LOG_CAP 200
#define LOG_LEN 160
static char logbuf[LOG_CAP][LOG_LEN];
static volatile uint16_t log_head = 0;    // zeigt auf nächste freie Zelle
static volatile uint16_t log_count = 0;   // max LOG_CAP
static portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;

static void tsNow(char* out, size_t len){
  time_t t = time(nullptr);
  if (t > 1000) {
    struct tm tm; localtime_r(&t, &tm);
    strftime(out, len, "%Y-%m-%d %H:%M:%S", &tm);
  } else {
    snprintf(out, len, "t+%lus", (unsigned long)(millis()/1000));
  }
}
static void logAppend(const char* line){
  portENTER_CRITICAL(&logMux);
  strlcpy(logbuf[log_head], line, LOG_LEN);
  log_head = (log_head + 1) % LOG_CAP;
  if (log_count < LOG_CAP) log_count++;   // sonst überschreiben wir älteste
  portEXIT_CRITICAL(&logMux);
}
static void logf(const char* level, const char* fmt, ...){
  char ts[24]; tsNow(ts, sizeof(ts));
  char core[192];
  va_list ap; va_start(ap, fmt);
  vsnprintf(core, sizeof(core), fmt, ap);
  va_end(ap);

  // Serial
  Serial.printf("[%s][%s] %s\n", ts, level, core);

  // Ringpufferzeile zusammenbauen und ablegen
  char line[LOG_LEN];
  snprintf(line, sizeof(line), "[%s][%s] %s", ts, level, core);
  logAppend(line);
  sdAppendLine(LOG_FILE, line);   // <-- NEU
}
#define LOGI(...) logf("INFO", __VA_ARGS__)
#define LOGW(...) logf("WARN", __VA_ARGS__)
#define LOGE(...) logf("ERR",  __VA_ARGS__)

String macToStr(const uint8_t mac[6]){
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  return String(b);
}

// ---------------- Utils/HTML ----------------
String htmlHead(const char* title){
  String s =
  F("<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>");
  s += "<title>"; s += title; s += F("</title>"
    "<style>body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:2rem auto;max-width:980px;padding:0 1rem}"
    "header{display:flex;justify-content:space-between;align-items:center;gap:.8rem}"
    "nav a{margin-right:.8rem}form{border:1px solid #ccc;padding:1rem;border-radius:.5rem;margin:.8rem 0}"
    "label{display:block;margin:.5rem 0 .2rem}input,select,button{font-size:1rem;padding:.45rem .6rem}"
    ".row{display:flex;gap:.6rem;flex-wrap:wrap}.row>*{flex:1 1 240px}.btns{display:flex;gap:.6rem;margin-top:.6rem}"
    "pre{background:#111;color:#eee;padding:1rem;border-radius:.5rem;overflow:auto;max-height:60vh}"
    ".ok{color:#0a0}.bad{color:#a00}</style></head><body>");
  s += F("<header><div><h1>Nebenuhr-Taktgeber</h1>"
         "<nav><a href='/'>Status</a><a href='/config'>Konfiguration</a><a href='/logs'>Logs</a></nav></div></header>");
  return s;
}
String htmlFoot(){ return F("</body></html>"); }

String fmtTime(time_t t){
  if (t<1000) return String("—");
  struct tm tm; localtime_r(&t, &tm);
  char buf[32]; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
  return String(buf);
}

bool checkAuth(){
  if (strlen(cfg.user)==0) return true;
  if (!server.authenticate(cfg.user, cfg.pass)) {
    server.requestAuthentication(BASIC_AUTH, "ESP32-EVB", "Zugang geschützt");
    return false;
  }
  return true;
}

String nextImpulseText(){
  return L.lastPulseWasA ? "Relais B" : "Relais A";
}

// ---------------- Persistenz ----------------
void saveConfig(){
  if (!sdReady) return;
  String s;
  s.reserve(1024);
  s += "dhcp=";        s += (cfg.dhcp ? "1":"0"); s += "\n";
  s += "host=";        s += cfg.hostname; s += "\n";
  s += "ntp=";         s += cfg.ntp; s += "\n";
  s += "tz=";          s += cfg.tz; s += "\n";
  s += "pulse_ms=";    s += String(cfg.pulse_ms); s += "\n";
  s += "ntp_sync_min=";s += String(cfg.ntp_sync_min); s += "\n";
  s += "user=";        s += cfg.user; s += "\n";
  s += "pass=";        s += cfg.pass; s += "\n";
  s += "ip=";          s += ipToStr4(cfg.ip); s += "\n";
  s += "gw=";          s += ipToStr4(cfg.gw); s += "\n";
  s += "mask=";        s += ipToStr4(cfg.mask); s += "\n";
  s += "dns1=";        s += ipToStr4(cfg.dns1); s += "\n";
  s += "dns2=";        s += ipToStr4(cfg.dns2); s += "\n";
  s += "minidx=";      s += String(L.min_index); s += "\n";
  s += "lastA=";       s += (L.lastPulseWasA ? "1":"0"); s += "\n";
  s += "need_clock_set="; s += (needClockSet ? "1":"0"); s += "\n";
  sdWriteText(CFG_FILE, s);
}

void loadDefaults(){
