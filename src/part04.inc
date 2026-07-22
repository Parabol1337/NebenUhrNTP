  L.min_index = (L.min_index + 1) % 720;
  LOGI("MinuteTick: RELAY=%s idx=%u/720 pulse=%ums", relayName(pin), L.min_index, cfg.pulse_ms);
  saveConfig(); 

}

// ---------------- Minute Boundary / Reconcile ----------------
void tickOnBoundary(){

  if (needClockSet)  return;
  if (waitForNtp) return;   
  
  time_t now = time(nullptr);
  if (pauseTick || now<=1000 || (now % 60) != 0 || now == lastAligned) return;
  lastAligned = now;

  // 1) Immer zuerst den regulären Minuten-Tick fahren (TOGGLE + idx++)
  minuteTick();

  // 2) Wenn NTP frisch ist, Rest-Delta nach dem Tick prüfen
  bool ntpFresh = (lastNtpSync > 0) && ((uint32_t)(now - lastNtpSync) <= (cfg.ntp_sync_min * 120UL));
  if (!ntpFresh) {
    LOGW("Reconcile skipped: NTP stale (last=%s)", lastNtpSync ? fmtTime(lastNtpSync).c_str() : "—");
    return;
  }

  uint16_t ntp_idx    = toMinutes12h(now);
  uint16_t deltaAfter = (ntp_idx + 720 - L.min_index) % 720;
  uint16_t deltaBack  = (L.min_index + 720 - ntp_idx) % 720;
  
  // Wenn Uhr leicht voraus (<5 min) → einfach abwarten
  if (deltaBack > 0 && deltaBack <= 5) {
    LOGW("Reconcile skipped: clock ahead by %u min -> wait", deltaBack);
    return;
  }
  
  // Wenn Uhr deutlich voraus (>5 min) → einmal rumdrehen (mechanisch sicher)
  if (deltaBack > 5 && deltaBack < 715) {
    LOGW("Reconcile: clock ahead by %u min -> full rotation (720 min)", deltaBack);
    pauseTick   = true;
    step_abort  = false;
    step_done   = 0;
    step_target = 720-deltaBack;   // komplette Runde
    step_running= true;
    return;
  }
  
  // Standard: Uhr hinkt nach -> normal nachholen
  if (deltaAfter > 0) {
    LOGI("Reconcile after tick: delta=%u -> start FastStep", deltaAfter);
    pauseTick   = true;
    step_abort  = false;
    step_done   = 0;
    step_target = deltaAfter;
    step_running= true;
  }

}

// ---------------- Async FastStep Task ----------------
void stepTask(void*){
  const uint32_t interval_ms = 1000 / STEP_HZ;
  bool ab = false;
  for(;;){
    if (step_running){
      static bool announced=false;
      if (!announced){
        LOGI("FastStep start: target=%u at %u Hz", step_target, STEP_HZ);
        announced=true;
      }
      if (step_abort){
        LOGW("FastStep aborted at step=%u/%u idx=%u/720", step_done, step_target, L.min_index);
        step_running = false;
        step_abort = false;
        pauseTick = false;
        bothOff();
        announced=false;
      } else if (step_done < step_target){
        int pin = L.lastPulseWasA ? RELAY_B : RELAY_A;
        pulseRelay(pin, cfg.pulse_ms);
        int rest = (int)interval_ms - (int)cfg.pulse_ms;
        if (rest > 0) vTaskDelay(pdMS_TO_TICKS(rest));
        L.min_index = (L.min_index + 1) % 720;
        step_done++;
        LOGI("FastStep: step=%u/%u RELAY=%s idx=%u/720 pulse=%ums", step_done, step_target, relayName(pin), L.min_index, cfg.pulse_ms);
      } else {
        step_running = false;
        pauseTick = false;
        bothOff();
        saveConfig(); 
        announced=false;
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

// ---------------- Web Helpers ----------------
String inputIp(const char* name, IPAddress ip){
  String s; s.reserve(64);
  s += "<input name='"; s+=name; s+="' value='"; s+=ip.toString(); s+="' placeholder='x.x.x.x'>";
  return s;
}
IPAddress parseIp(const String &v, IPAddress def){
  int a[4]={0,0,0,0}, idx=0; String n="";
  for (size_t i=0;i<v.length() && idx<4;i++){
    char c=v[i];
    if (c=='.'){ a[idx++]=n.toInt(); n=""; }
    else if (isDigit((unsigned char)c)) n+=c;
  }
  if (idx<4) a[idx]=n.toInt();
  for (int i=0;i<4;i++){ if (a[i]<0||a[i]>255) return def; }
  return IPAddress(a[0],a[1],a[2],a[3]);
}
String getUptime()
{
  uint32_t ms = millis();
  uint32_t sec = ms / 1000;
  uint32_t days = sec / 86400;
  sec %= 86400;
  uint8_t hours = sec / 3600;
  sec %= 3600;
  uint8_t minutes = sec / 60;
  uint8_t seconds = sec % 60;

  char buf[32];
  if (days > 0) {
    snprintf(buf, sizeof(buf), "%lu d %02u:%02u:%02u",
             (unsigned long)days, hours, minutes, seconds);
  } else {
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
             hours, minutes, seconds);
  }
  return String(buf);
}

// ---------------- Web: Seiten ----------------
void handleRoot(){
  if (!checkAuth()) return;
  String s = htmlHead("Status");

  uint8_t mac[6]; ETH.macAddress(mac);
  String macStr = macToStr(mac);

  s += F("<p><b>Ethernet:</b> "); s += ethConnected?F("<span class='ok'>verbunden</span>"):F("<span class='bad'>getrennt</span>");
  s += F("<br><b>IP:</b> "); s += ETH.localIP().toString();
  s += F("<br><b>MAC:</b> "); s += macStr;
  s += F("<br><b>Hostname:</b> "); s += cfg.hostname;
  s += F("<br><b>Uptime:</b> "); s += getUptime();
  s += F("<br><b>NTP-Server:</b> "); s += cfg.ntp;

  time_t now = time(nullptr);
  bool ntpFresh = (lastNtpSync > 0) && ((uint32_t)(now - lastNtpSync) <= (cfg.ntp_sync_min * 120UL));
  s += F("<br><b>NTP-Status:</b> ");
