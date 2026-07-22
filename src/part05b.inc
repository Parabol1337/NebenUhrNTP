  s += F("</div><div><label>DNS 2</label>");                       s += inputIp("dns2", ipFrom(cfg.dns2));
  s += F("</div></div>");

  s += F("<h3>Zeit</h3>"
         "<label>NTP-Server</label><input name='ntp' value='"); s += cfg.ntp; s += F("'>"
         "<label>Zeitzone</label><select name='tz_sel'>");
  for (auto &it : TZ_LIST) {
    s += "<option value='"; s += it.posix; s += "'";
    if (String(it.posix) == String(cfg.tz)) s += " selected";
    s += ">"; s += it.label; s += "</option>";
  }
  s += F("</select>"
         "<label>NTP Sync-Intervall (Minuten)</label>"
         "<input type='number' name='ntpmin' min='1' max='1440' value='"); s += String(cfg.ntp_sync_min); s += F("'>");

  s += F("<h3>Impuls</h3>"
         "<label>Impulsdauer (ms)</label><input type='number' name='pulse' min='100' max='400' value='"); s += String(cfg.pulse_ms); s += F("'>");

  s += F("<h3>Passwortschutz</h3>"
         "<div class='row'><div><label>Benutzer</label><input name='user' value='"); s += cfg.user;
  s += F("'></div><div><label>Passwort</label><input name='pass' type='password' value='"); s += cfg.pass; s += F("'></div></div>"
         "<div class='btns'><button type='submit'>Speichern</button></div>"
         "</form><script>"
         "document.querySelector('select[name=dhcp]').value='"); s += (cfg.dhcp ? "1":"0");
  s += F("';</script>");
  s += htmlFoot();
  server.send(200, "text/html; charset=utf-8", s);
}

// ---------------- /logs (HTML) & /logs.txt (Text) ----------------
void handleLogs(){
  if (!checkAuth()) return;

  // Clear?
  if (server.hasArg("clear")){
    portENTER_CRITICAL(&logMux);
    log_head = 0; log_count = 0;
    portEXIT_CRITICAL(&logMux);
    server.sendHeader("Location","/logs");
    server.send(303);
    return;
  }

  String s = htmlHead("Logs");
  s += F("<meta http-equiv='refresh' content='2'>");
  s += F("<pre>");

  // Ausgabe von ältestem zum neuesten
  portENTER_CRITICAL(&logMux);
  uint16_t cnt = log_count;
  uint16_t head = log_head;
  for (uint16_t i = 0; i < cnt; i++){
    uint16_t idx = (head + LOG_CAP - cnt + i) % LOG_CAP;
    s += logbuf[idx];
    s += "\n";
  }
  portEXIT_CRITICAL(&logMux);

  s += F("</pre>");
  s += htmlFoot();
  server.send(200, "text/html; charset=utf-8", s);
}

void handleLogsTxt(){
  if (!checkAuth()) return;
  String s; s.reserve(LOG_CAP * (LOG_LEN/2));
  portENTER_CRITICAL(&logMux);
  uint16_t cnt = log_count;
  uint16_t head = log_head;
  for (uint16_t i = 0; i < cnt; i++){
