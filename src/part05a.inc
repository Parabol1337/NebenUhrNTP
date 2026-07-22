  s += ntpFresh ? F("<span class='ok'>OK</span>") : F("<span class='bad'>alt/keine Sync</span>");
  s += F("<br><b>NTP-Sync-Zeitpunkt:</b> ");
  s += (lastNtpSync ? fmtTime(lastNtpSync) : String("—"));
  s += F("<br><b>NTP-Sync-Intervall:</b> ");
  s += String(cfg.ntp_sync_min); s += F(" min");

  s += F("<br><b>NTP-Zeit:</b> "); s += fmtTime(now);

  uint16_t idx = L.min_index;
  uint16_t hh = (idx / 60) % 12; if (hh==0) hh=12;
  uint16_t mm = idx % 60;
  char buf[6]; snprintf(buf, sizeof(buf), "%02d:%02d", hh%24, mm);
  s += F("<br><b>Interne Nebenuhr:</b> "); s += String(buf);

  s += F("<br><b>Impulsrelais:</b> "); s += nextImpulseText();
  s += F("<br><b>Impulsdauer:</b> "); s += String(cfg.pulse_ms); s += F(" ms");

  if (step_running){
    s += F("<p><b>FastStep:</b> l&auml;uft – ");
    s += String(step_done); s += "/"; s += String(step_target);
    s += F(" <a href='/abort'>Abbrechen</a></p>");
  }

  if (needClockSet && !waitForNtp){
    s += F("<p class='bad'><b>Hinweis:</b>Takt gesperrt – "
           "Bitte die interne Nebenuhrzeit setzen.</p>");
  }
  
  if (waitForNtp){
    s += F("<p class='bad'><b>Hinweis:</b> Takt gesperrt – warte auf ersten NTP-Sync.</p>");
  }

  if (!waitForNtp){
  s += F("<form method='POST' action='/setclock'><h3>Interne Nebenuhr korrigieren</h3>"
        "<label>Aktuelle Zeit der externen Nebenuhr (Format: 12h oder 24h)</label>"
        "<input name='clock' placeholder='HH:MM'>"
        "<p>Setzt die Zeit der internen Nebenuhr auf die Zeit der externen Nebenuhr</p>"
        "<div class='btns'><button>Setzen & Abgleichen</button></div></form>");
  }
  s += htmlFoot();
  server.send(200, "text/html; charset=utf-8", s);
}

void handleConfig(){
  if (!checkAuth()) return;
  String s = htmlHead("Konfiguration");
  s += F("<form method='POST' action='/save'>"
         "<h3>Netzwerk</h3>"
         "<label>Hostname</label><input name='host' value='"); s += cfg.hostname; s += F("'>"
         "<label>DHCP</label><select name='dhcp'><option value='1'>Ja</option><option value='0'>Nein (statisch)</option></select>");

  s += F("<div class='row'><div><label>IP</label>");   s += inputIp("ip",   ipFrom(cfg.ip));
  s += F("</div><div><label>Gateway</label>");         s += inputIp("gw",   ipFrom(cfg.gw));
  s += F("</div><div><label>Subnetz</label>");         s += inputIp("mask", ipFrom(cfg.mask));
  s += F("</div></div><div class='row'><div><label>DNS 1</label>"); s += inputIp("dns1", ipFrom(cfg.dns1));
