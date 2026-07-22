  memset(&cfg, 0, sizeof(cfg));
  cfg.dhcp = true;
  strlcpy(cfg.hostname, "Nebenuhr", sizeof(cfg.hostname));
  strlcpy(cfg.ntp, "rb-time.de.bosch.com", sizeof(cfg.ntp));
  strlcpy(cfg.tz,  "CET-1CEST,M3.5.0,M10.5.0/3", sizeof(cfg.tz));
  cfg.pulse_ms = 200;
  cfg.ntp_sync_min = 60;
  strlcpy(cfg.user, "admin", sizeof(cfg.user));
  strlcpy(cfg.pass, "admin", sizeof(cfg.pass));
  ipToBytes(IPAddress(192,168,1,50), cfg.ip);
  ipToBytes(IPAddress(192,168,1,1),  cfg.gw);
  ipToBytes(IPAddress(255,255,255,0),cfg.mask);
  ipToBytes(IPAddress(9,9,9,9),      cfg.dns1);
  ipToBytes(IPAddress(1,1,1,1),      cfg.dns2);

  L.min_index = 0;
  L.lastPulseWasA = false;
  needClockSet = true;   
}

void loadConfig(){ // <-- ERSETZEN
  loadDefaults();
  if (!sdReady){
    LOGE("SD not ready in loadConfig()");
    return;
  }
  String all;
  if (!sdReadAll(CFG_FILE, all)){
    // Erststart: Defaults auf SD schreiben
    saveConfig();
    LOGW("No SD config found. Wrote defaults to SD.");
    return;
  }

  // Zeilenweise parsen
  int start = 0;
  while (start < (int)all.length()){
    int end = all.indexOf('\n', start);
    if (end < 0) end = all.length();
    String line = all.substring(start, end); line.trim();
    start = end + 1;
    if (line.length()==0 || line.startsWith("#")) continue;
    int eq = line.indexOf('=');
    if (eq < 1) continue;
    String k = line.substring(0,eq); k.trim();
    String v = line.substring(eq+1); v.trim();

    if      (k=="dhcp")         cfg.dhcp = (v=="1");
    else if (k=="host")         strlcpy(cfg.hostname, v.c_str(), sizeof(cfg.hostname));
    else if (k=="ntp")          strlcpy(cfg.ntp, v.c_str(), sizeof(cfg.ntp));
    else if (k=="tz")           strlcpy(cfg.tz, v.c_str(), sizeof(cfg.tz));
    else if (k=="pulse_ms")     cfg.pulse_ms = (uint16_t)v.toInt();
    else if (k=="ntp_sync_min") cfg.ntp_sync_min = (uint16_t)v.toInt();
    else if (k=="user")         strlcpy(cfg.user, v.c_str(), sizeof(cfg.user));
    else if (k=="pass")         strlcpy(cfg.pass, v.c_str(), sizeof(cfg.pass));
    else if (k=="ip")           strToIp4(v, cfg.ip);
    else if (k=="gw")           strToIp4(v, cfg.gw);
    else if (k=="mask")         strToIp4(v, cfg.mask);
    else if (k=="dns1")         strToIp4(v, cfg.dns1);
    else if (k=="dns2")         strToIp4(v, cfg.dns2);
    else if (k=="minidx")       L.min_index = (uint16_t)v.toInt();
    else if (k=="lastA")        L.lastPulseWasA = (v=="1");
    else if (k=="need_clock_set")  needClockSet = (v=="1");
  }

  LOGI("Config loaded: host=%s ntp=%s tz=%s pulse=%ums ntpmin=%u idx=%u",
       cfg.hostname, cfg.ntp, cfg.tz, cfg.pulse_ms, cfg.ntp_sync_min, L.min_index);
}




// ---------------- Ethernet Events ----------------
void EthEvent(arduino_event_id_t event, arduino_event_info_t info){
  switch (event){
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname(cfg.hostname);
      LOGI("ETH start");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      LOGI("ETH link up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP: {
      ethConnected = true;
      uint8_t mac[6]; ETH.macAddress(mac);
      LOGI("ETH got IP: ip=%s gw=%s mask=%s dns1=%s dns2=%s mac=%s",
           ETH.localIP().toString().c_str(),
           ETH.gatewayIP().toString().c_str(),
           ETH.subnetMask().toString().c_str(),
           ETH.dnsIP(0).toString().c_str(),
           ETH.dnsIP(1).toString().c_str(),
           macToStr(mac).c_str());
    } break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_LOST_IP:
    case ARDUINO_EVENT_ETH_STOP:
      ethConnected = false;
      LOGW("ETH down");
      break;
    default: break;
  }
}

// ---------------- NTP ----------------
void onNtpSync(struct timeval* tv) {
  lastNtpSync = time(nullptr);
  waitForNtp = false;                     // ← Tick freigeben
  LOGI("NTP sync successfull – tick released");
}

void setupSNTP(){
  ntpSyncIntervalMs = (uint32_t)max(1, (int)cfg.ntp_sync_min) * 60UL * 1000UL;
  sntp_set_sync_interval(ntpSyncIntervalMs);
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTime(0, 0, cfg.ntp);
  setenv("TZ", cfg.tz, 1);
  tzset();
  LOGI("SNTP set: server=%s interval=%umin tz=%s", cfg.ntp, cfg.ntp_sync_min, cfg.tz);
}

unsigned toMinutes12h(time_t t){
  struct tm tm; localtime_r(&t, &tm);
  return (tm.tm_hour * 60 + tm.tm_min) % (12*60);
}

// ---------------- Relais ----------------
static inline const char* relayName(int gpio){ return (gpio==RELAY_A) ? "A" : "B"; }

void bothOff(){ digitalWrite(RELAY_A, LOW); digitalWrite(RELAY_B, LOW); }

// kooperativ (Tasks/loop)
void pulseRelay(int gpio, uint16_t ms){
  bothOff();
  digitalWrite(gpio, HIGH);
  vTaskDelay(pdMS_TO_TICKS(ms));
  digitalWrite(gpio, LOW);
  L.lastPulseWasA = (gpio == RELAY_A);
}

// regulärer Minuten-Impuls
void minuteTick(){
  if (needClockSet) return;   // <-- NEU: Sicherheit, falls doch aufgerufen
  if (waitForNtp) return;
  int pin = L.lastPulseWasA ? RELAY_B : RELAY_A;
  pulseRelay(pin, cfg.pulse_ms);
