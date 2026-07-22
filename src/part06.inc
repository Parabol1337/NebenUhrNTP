    uint16_t idx = (head + LOG_CAP - cnt + i) % LOG_CAP;
    s += logbuf[idx]; s += "\r\n";
  }
  portEXIT_CRITICAL(&logMux);
  server.send(200, "text/plain", s);
}

// ---------------- Web: Actions ----------------
unsigned toMinFromStr(const String &hhmm, bool &ok){
  ok=false; int c=hhmm.indexOf(':'); if (c<1) return 0;
  int hh = hhmm.substring(0,c).toInt();
  int mm = hhmm.substring(c+1).toInt();
  if (hh<0||hh>23||mm<0||mm>59) return 0;
  ok=true; return (unsigned)(hh*60+mm) % (12*60);
}

void handleSetClock(){
  if (!checkAuth()) return;
  time_t tnow = time(nullptr);
  if (tnow <= 1000){ server.send(200, "text/plain", "Keine NTP-Zeit."); return; }

  String clk = server.hasArg("clock")?server.arg("clock"):"";
  bool ok=false; 
  uint16_t virt = toMinFromStr(clk, ok);   // 0..719
  if (!ok){ server.send(200, "text/plain", "Eingabeformat HH:MM ungueltig."); return; }

  uint16_t old = L.min_index;
  L.min_index = virt;
  needClockSet = false;     // <-- NEU: Freigabe nach erstmaligem Setzen
  saveConfig(); 

  uint16_t ntp_idx = toMinutes12h(tnow);
  uint16_t delta   = (ntp_idx + 720 - L.min_index) % 720;

  LOGI("SetClock: virt %02u:%02u -> idx=%u (old=%u), NTP idx=%u, delta=%u",
       virt/60, virt%60, L.min_index, old, ntp_idx, delta);

  if (!waitForNtp && delta > 0){
    pauseTick   = true;
    step_abort  = false;
    step_done   = 0;
    step_target = delta;
    step_running= true;
  }

  server.sendHeader("Location","/");
  server.send(303);
}

void handleAutoAdjust(){ handleSetClock(); }

void handleSave(){
  if (!checkAuth()) return;
  auto val = [&](const char* k){ return server.hasArg(k)?server.arg(k):String(""); };

  String host = val("host"); if (host.length()) strlcpy(cfg.hostname, host.c_str(), sizeof(cfg.hostname));
  cfg.dhcp = (val("dhcp")=="1");
  String ntp = val("ntp");  if (ntp.length())  strlcpy(cfg.ntp, ntp.c_str(), sizeof(cfg.ntp));
  uint16_t nmin = val("ntpmin").toInt(); if (nmin>=1 && nmin<=1440) cfg.ntp_sync_min = nmin;
  uint16_t pul  = val("pulse").toInt();  if (pul>=100 && pul<=400)  cfg.pulse_ms = pul;
  String usr = val("user"); if (usr.length()) strlcpy(cfg.user, usr.c_str(), sizeof(cfg.user));
  String pwd = val("pass"); if (pwd.length()) strlcpy(cfg.pass, pwd.c_str(), sizeof(cfg.pass));

  String tzs = server.hasArg("tz_sel") ? server.arg("tz_sel") : "";
  if (tzs.length()) strlcpy(cfg.tz, tzs.c_str(), sizeof(cfg.tz));

  IPAddress nip = parseIp(val("ip"),   ipFrom(cfg.ip));
  IPAddress ngw = parseIp(val("gw"),   ipFrom(cfg.gw));
  IPAddress nmk = parseIp(val("mask"), ipFrom(cfg.mask));
  IPAddress nd1 = parseIp(val("dns1"), ipFrom(cfg.dns1));
  IPAddress nd2 = parseIp(val("dns2"), ipFrom(cfg.dns2));
  ipToBytes(nip, cfg.ip); ipToBytes(ngw, cfg.gw); ipToBytes(nmk, cfg.mask);
  ipToBytes(nd1, cfg.dns1); ipToBytes(nd2, cfg.dns2);

  saveConfig();

  LOGI("Save + reboot in 10s: dhcp=%d ip=%s gw=%s mask=%s dns1=%s dns2=%s host=%s ntp=%s tz=%s pulse=%u ntpmin=%u",
       cfg.dhcp, nip.toString().c_str(), ngw.toString().c_str(), nmk.toString().c_str(),
       nd1.toString().c_str(), nd2.toString().c_str(), cfg.hostname, cfg.ntp, cfg.tz,
       cfg.pulse_ms, cfg.ntp_sync_min);

  const int SECS = 10;
  String page = F("<!doctype html><meta charset='utf-8'>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>Gespeichert</title>"
                  "<style>body{font-family:system-ui;margin:2rem}progress{width:100%}</style>"
                  "<h1>Einstellungen gespeichert</h1>"
                  "<p>Das Geraet startet jetzt neu. Du wirst in <b id='s'></b> Sekunden zur Startseite weitergeleitet…</p>"
                  "<progress id='p' max='");
  page += String(SECS);
  page += F("' value='0'></progress>"
            "<script>let s="); page += String(SECS);
  page += F(";let p=document.getElementById('p'),t=document.getElementById('s');"
            "t.textContent=s;let iv=setInterval(()=>{s--;p.value=");
  page += String(SECS);
  page += F("-s;t.textContent=s;if(s<=0){clearInterval(iv);location.href='/'}} ,1000);</script>");
  server.send(200, "text/html; charset=utf-8", page);

  delay(SECS * 1000UL);
  ESP.restart();
}

// ---------------- Setup / Loop ----------------
void handleLogs();     // fwd
void handleLogsTxt();  // fwd

void setup(){
  Serial.begin(115200);
  sdInitOrDie();   // ← blockiert, wenn SD fehlt (kein Start)
  LOGI("Boot");
  pinMode(RELAY_A, OUTPUT);
  pinMode(RELAY_B, OUTPUT);
  pinMode(RESET_BTN, INPUT);
  digitalWrite(RELAY_A, LOW);
  digitalWrite(RELAY_B, LOW);

  loadConfig();

  WiFi.onEvent([](arduino_event_id_t e, arduino_event_info_t info){ EthEvent(e, info); });
  bool ok = ETH.begin(ETH_PHY_LAN8720, 0, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  if (!ok) LOGE("ETH.begin failed");
  if (!cfg.dhcp) {
    ETH.config(ipFrom(cfg.ip), ipFrom(cfg.gw), ipFrom(cfg.mask), ipFrom(cfg.dns1), ipFrom(cfg.dns2));
  }
  ETH.setHostname(cfg.hostname);
  waitForNtp = true;
