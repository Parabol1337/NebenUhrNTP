  LOGI("Wait for NTP sync");
  setupSNTP();

  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/config",     HTTP_GET,  handleConfig);
  server.on("/save",       HTTP_POST, handleSave);
  server.on("/autoadjust", HTTP_POST, handleAutoAdjust);
  server.on("/setclock",   HTTP_POST, handleSetClock);

  // Logs
  server.on("/logs",    HTTP_GET, handleLogs);
  server.on("/logs.txt",HTTP_GET, handleLogsTxt);

  // Fortschritt + Abbruch
  server.on("/st", HTTP_GET, [](){
    if (!checkAuth()) return;
    String j = "{\"running\":";
    j += (step_running ? "true" : "false");
    j += ",\"done\":";   j += String(step_done);
    j += ",\"target\":"; j += String(step_target);
    j += ",\"minidx\":"; j += String(L.min_index);
    j += "}";
    server.send(200, "application/json", j);
  });
  server.on("/abort", HTTP_GET, [](){
    if (!checkAuth()) return;
    step_abort = true;
    LOGW("Abort requested by user");
    server.sendHeader("Location","/");
    server.send(303);
  });

  server.begin();
  xTaskCreatePinnedToCore(stepTask, "stepTask", 4096, nullptr, 1, &stepTaskHandle, 1);

  // Boot-Reconcile: nur wenn NTP frisch
  time_t tnow = time(nullptr);
  bool ntpFreshBoot = (lastNtpSync > 0) && ((uint32_t)(tnow - lastNtpSync) <= (cfg.ntp_sync_min * 120UL));
  if (tnow > 1000 && ntpFreshBoot){
    uint16_t ntp_idx = toMinutes12h(tnow);
    uint16_t delta   = (ntp_idx + 720 - L.min_index) % 720;
    if (delta > 0){
      LOGI("Boot reconcile: delta=%u -> start FastStep", delta);
      pauseTick   = true;
      step_abort  = false;
      step_done   = 0;
      step_target = delta;
      step_running= true;
    }
  }

  LOGI("System Ready");
}

void loop(){
  server.handleClient();

  // Minutentakt oder FastStep-Start auf voller Minute
  tickOnBoundary();

  // Long-Press Reset (~5s)
  static unsigned long pressStart = 0;
  static int lastCountdown = -1;
  bool pressed = (digitalRead(RESET_BTN) == LOW);

  if (pressed) {
    if (pressStart == 0) {
      pressStart = millis();
      lastCountdown = -1;
      LOGW("Reset button pressed – hold 5s to factory reset");
    } else {
      unsigned long held = millis() - pressStart;
      int sec = held / 1000;
      if (sec != lastCountdown && sec < 5) {
        lastCountdown = sec;
        LOGW("... held %ds", sec);
      }
      if (held > 5000) {
        LOGW("Factory defaults + restart");
        loadDefaults();
        saveConfig();
        delay(500);
        ESP.restart();
      }
    }
  } else {
    if (pressStart != 0) {
      LOGI("Reset canceled");
      pressStart = 0;
    }
  }
}
