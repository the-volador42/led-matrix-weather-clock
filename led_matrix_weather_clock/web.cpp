#include "web.h"
#include "web_page.h"
#include "modes.h"
#include "alarm.h"
#include "weather.h"
#include "network.h"
#include "settings.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>

static ESP8266WebServer server(80);
static ESP8266HTTPUpdateServer httpUpdater;
bool webInterfaceEnabled = false;
static unsigned long restartAt = 0;   // перезагрузка после ответа браузеру

void startWebInterface() {
  if (webInterfaceEnabled) return;
  server.begin();
  webInterfaceEnabled = true;
  Serial.println("Web interface started.");
}

void handleWebClient() {
  server.handleClient();
  if (restartAt && millis() > restartAt) ESP.restart();
}

// ---------------------------------------------------------------------
//  Вспомогательное
// ---------------------------------------------------------------------
static int parseHHMM(const String& s) {   // "07:30" -> 450, ошибка -> -1
  int colon = s.indexOf(':');
  if (colon < 1) return -1;
  int h = s.substring(0, colon).toInt(), m = s.substring(colon + 1).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

static void sendJson(JsonDocument& d) {
  String out;
  serializeJson(d, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

static String localTimeText() {
  time_t now = time(nullptr);
  if (now < 100000) return "";
  int sec = (int)(((int64_t)now + tzOffsetSeconds()) % 86400);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", sec / 3600, sec % 3600 / 60);
  return String(buf);
}

// ---------------------------------------------------------------------
//  API
// ---------------------------------------------------------------------
static void handleState() {
  JsonDocument d;
  d["fw"] = FW_VERSION;
  d["uptime"] = millis() / 1000;
  d["heap"] = ESP.getFreeHeap();
  d["time"] = localTimeText();
  d["power"] = powerOn;
  d["brightness"] = globalBrightness;
  d["nightActive"] = nightDimActive();
  d["mode"] = (int)currentMode;
  d["effect"] = selectedEffectIndex;
  d["text"] = displayText;
  d["alarmDawn"] = ALARM_DAWN_MINUTES;
  d["season"] = settings.season;          // выбор: 0 — по дате
  d["seasonNow"] = (int)currentSeason;    // какой сезон сейчас на экране (0 весна … 3 зима)

  JsonObject a = d["alarm"].to<JsonObject>();
  a["h"] = alarmHour; a["m"] = alarmMinute; a["on"] = alarmEnabled; a["ringing"] = alarmRinging;

  JsonObject n = d["nightCfg"].to<JsonObject>();
  n["on"] = settings.nightEnabled; n["start"] = settings.nightStart;
  n["end"] = settings.nightEnd;    n["bright"] = settings.nightBrightness;

  JsonObject w = d["weather"].to<JsonObject>();
  if (lastTemp == -100) w["temp"] = nullptr; else w["temp"] = lastTemp;
  w["id"] = g_weatherId;
  w["effect"] = EFFECTS[weatherEffect].name;
  if (weatherUpdatedAt) w["ago"] = (millis() - weatherUpdatedAt) / 1000; else w["ago"] = nullptr;
  w["error"] = weatherError;
  w["city"] = settings.city;
  w["tz"] = settings.tzHours;
  w["hasKey"] = settings.owmKey.length() > 0;

  JsonObject f = d["wifi"].to<JsonObject>();
  bool connected = WiFi.status() == WL_CONNECTED;
  f["connected"] = connected;
  f["ssid"] = settings.wifiSsid;
  f["ip"] = connected ? WiFi.localIP().toString() : String("");
  f["rssi"] = connected ? WiFi.RSSI() : 0;
  f["portal"] = portalActive();
  f["ap"] = apName();
  sendJson(d);
}

static void handleMeta() {
  JsonDocument d;
  JsonArray modes = d["modes"].to<JsonArray>();
  for (int i = 0; i < MODE_COUNT; i++) {
    if (i == MODE_ALARM || i == MODE_EFFECT) continue;   // включаются будильником и списком эффектов
    JsonObject m = modes.add<JsonObject>();
    m["id"] = i;
    m["name"] = MODE_NAMES[i];
  }
  JsonArray fx = d["effects"].to<JsonArray>();
  for (int i = 0; i < FX_COUNT; i++) fx.add(EFFECTS[i].name);
  sendJson(d);
}

static void handleSet() {
  bool remember = false;

  if (server.hasArg("power")) { powerOn = server.arg("power").toInt() != 0; }
  if (server.hasArg("brightness")) {
    globalBrightness = constrain(server.arg("brightness").toInt(), 1, 255);
    remember = true;
  }
  if (server.hasArg("text")) {
    displayText = server.arg("text");
    resetText();
    remember = true;
  }
  if (server.hasArg("mode")) {
    stopAlarm();
    int m = constrain(server.arg("mode").toInt(), 0, MODE_COUNT - 1);
    if (m != MODE_ALARM) {
      currentMode = (Mode)m;
      if (currentMode == MODE_DEMO) demoEffectStart = 0;
      if (currentMode == MODE_TEXT) resetText();
      powerOn = true;
    }
    remember = true;
  }
  if (server.hasArg("effect")) {
    stopAlarm();
    int id = constrain(server.arg("effect").toInt(), 0, FX_COUNT - 1);
    if (id != lastSelectedEffect || currentMode != MODE_EFFECT) {
      resetEffect(id);
      lastSelectedEffect = id;
    }
    selectedEffectIndex = id;
    currentMode = MODE_EFFECT;
    powerOn = true;
    remember = true;
  }
  if (server.hasArg("alarmTime")) {
    int t = parseHHMM(server.arg("alarmTime"));
    if (t >= 0) setAlarmTime(t / 60, t % 60);
  }
  if (server.hasArg("alarmOn")) setAlarmEnabled(server.arg("alarmOn").toInt() != 0);

  if (server.hasArg("nightOn")) { settings.nightEnabled = server.arg("nightOn").toInt() != 0; markSettingsDirty(); }
  if (server.hasArg("nightStart")) { int t = parseHHMM(server.arg("nightStart")); if (t >= 0) { settings.nightStart = t; markSettingsDirty(); } }
  if (server.hasArg("nightEnd"))   { int t = parseHHMM(server.arg("nightEnd"));   if (t >= 0) { settings.nightEnd = t;   markSettingsDirty(); } }
  if (server.hasArg("nightBright")) { settings.nightBrightness = constrain(server.arg("nightBright").toInt(), 1, 255); markSettingsDirty(); }

  bool weatherChanged = false;
  if (server.hasArg("city")) {
    String c = server.arg("city"); c.trim();
    if (c.length() && c != settings.city) { settings.city = c; weatherChanged = true; }
  }
  if (server.hasArg("owmKey")) {
    String k = server.arg("owmKey"); k.trim();
    if (k.length()) { settings.owmKey = k; weatherChanged = true; }
  }
  if (server.hasArg("tz")) {
    int tz = constrain(server.arg("tz").toInt(), -12, 14);
    if (tz != settings.tzHours) { settings.tzHours = tz; applyTimezone(); markSettingsDirty(); }
  }
  if (weatherChanged) { markSettingsDirty(); requestWeatherUpdate(); }
  if (server.hasArg("weatherRefresh")) requestWeatherUpdate();
  if (server.hasArg("season")) {
    settings.season = constrain(server.arg("season").toInt(), 0, 4);
    markSettingsDirty();
  }
  if (server.hasArg("otaPass")) {
    String p = server.arg("otaPass");
    if (p.length()) { settings.otaPassword = p; markSettingsDirty(); }
  }

  if (remember) rememberState();
  handleState();
}

// Текущий кадр матрицы: 32x8 точек по 3 байта (R, G, B), строки сверху вниз.
// Страница рисует его размытым фоном и маленьким превью.
static void handleFrame() {
  static uint8_t frame[MATRIX_W * MATRIX_H * 3];
  bool on = powerOn;
  for (int y = 0; y < MATRIX_H; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      uint8_t* p = &frame[(y * MATRIX_W + x) * 3];
      int idx = XY(x, y);
      if (on && idx >= 0) { p[0] = leds[idx].r; p[1] = leds[idx].g; p[2] = leds[idx].b; }
      else { p[0] = p[1] = p[2] = 0; }
    }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/octet-stream", frame, sizeof(frame));
}

static void handleScan() {
  JsonDocument d;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    d["scanning"] = true;
  } else if (n < 0) {               // ещё не запускали — запускаем в фоне
    WiFi.scanNetworks(true);
    d["scanning"] = true;
  } else {
    d["scanning"] = false;
    JsonArray arr = d["networks"].to<JsonArray>();
    // Сети по убыванию сигнала, без повторов имён.
    bool used[64] = {false};
    int count = n > 64 ? 64 : n;
    for (int k = 0; k < count && arr.size() < 20; k++) {
      int best = -1;
      for (int i = 0; i < count; i++)
        if (!used[i] && (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))) best = i;
      used[best] = true;
      String ssid = WiFi.SSID(best);
      if (!ssid.length()) continue;
      bool dup = false;
      for (JsonObject o : arr) if (ssid == o["ssid"].as<const char*>()) { dup = true; break; }
      if (dup) continue;
      JsonObject o = arr.add<JsonObject>();
      o["ssid"] = ssid;
      o["rssi"] = WiFi.RSSI(best);
      o["secure"] = WiFi.encryptionType(best) != ENC_TYPE_NONE;
    }
    WiFi.scanDelete();              // следующий запрос начнёт новый поиск
  }
  sendJson(d);
}

static void handleWifi() {
  String ssid = server.arg("ssid"); ssid.trim();
  if (!ssid.length()) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  server.send(200, "application/json", "{\"ok\":true}");
  connectToWifi(ssid, server.arg("pass"));
}

static void handleRestart() {
  saveSettingsNow();
  server.send(200, "application/json", "{\"ok\":true}");
  restartAt = millis() + 500;
}

static void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

// Всё неизвестное в режиме настройки ведём на главную страницу —
// так телефон сам показывает окно «Войдите в сеть».
static void handleNotFound() {
  if (portalActive()) {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "Not found");
}

void setupWebRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/meta", HTTP_GET, handleMeta);
  server.on("/api/set", HTTP_POST, handleSet);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/frame", HTTP_GET, handleFrame);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.onNotFound(handleNotFound);
  // Загрузка прошивки файлом: http://ledclock.local/update
  httpUpdater.setup(&server, "/update", "admin", settings.otaPassword.c_str());
}
