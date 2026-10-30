#include "settings.h"
#include "config.h"
#include "credentials.h"
#include "modes.h"
#include "alarm.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

Settings settings;

static const char* SETTINGS_FILE = "/settings.json";
static const char* SETTINGS_TMP  = "/settings.tmp";
static bool fsReady = false;
static bool dirty = false;
static unsigned long dirtySince = 0;
static String lastSavedJson;

static void setDefaults() {
  settings.wifiSsid        = WIFI_SSID;
  settings.wifiPass        = WIFI_PASSWORD;
  settings.city            = OWM_CITY;
  settings.owmKey          = OWM_API_KEY;
  settings.tzHours         = GMT_OFFSET_SEC / 3600;
  settings.brightness      = MIN_BRIGHTNESS;
  settings.mode            = MODE_NORMAL;
  settings.effect          = 0;
  settings.text            = "ПРИВЕТ";
  settings.alarmHour       = 7;
  settings.alarmMinute     = 0;
  settings.alarmEnabled    = false;
  settings.nightEnabled    = false;
  settings.nightStart      = NIGHT_DEFAULT_START;
  settings.nightEnd        = NIGHT_DEFAULT_END;
  settings.nightBrightness = NIGHT_DEFAULT_BRIGHTNESS;
  settings.otaPassword     = OTA_PASSWORD;
  settings.season          = 0;
}

static String toJson() {
  JsonDocument d;
  d["wifiSsid"] = settings.wifiSsid;   d["wifiPass"] = settings.wifiPass;
  d["city"] = settings.city;           d["owmKey"] = settings.owmKey;
  d["tzHours"] = settings.tzHours;
  d["brightness"] = settings.brightness;
  d["mode"] = settings.mode;           d["effect"] = settings.effect;
  d["text"] = settings.text;
  d["alarmHour"] = settings.alarmHour; d["alarmMinute"] = settings.alarmMinute;
  d["alarmEnabled"] = settings.alarmEnabled;
  d["nightEnabled"] = settings.nightEnabled;
  d["nightStart"] = settings.nightStart; d["nightEnd"] = settings.nightEnd;
  d["nightBrightness"] = settings.nightBrightness;
  d["otaPassword"] = settings.otaPassword;
  d["season"] = settings.season;
  String out;
  serializeJson(d, out);
  return out;
}

// Читаем только то, что есть в файле: новые поля в будущих версиях
// просто останутся со значениями по умолчанию.
static void readStr(JsonDocument& d, const char* k, String& v) { if (d[k].is<const char*>()) v = d[k].as<const char*>(); }
static void readBool(JsonDocument& d, const char* k, bool& v)  { if (d[k].is<bool>()) v = d[k].as<bool>(); }
template <typename T>
static void readInt(JsonDocument& d, const char* k, T& v, long lo, long hi) {
  if (d[k].is<long>()) v = (T)constrain(d[k].as<long>(), lo, hi);
}

void loadSettings() {
  setDefaults();
  fsReady = LittleFS.begin();   // при первом запуске сама размечает флеш
  if (!fsReady) { Serial.println("LittleFS: not available, settings will not be saved"); return; }

  File f = LittleFS.open(SETTINGS_FILE, "r");
  if (!f) { Serial.println("Settings: defaults"); lastSavedJson = ""; return; }
  JsonDocument d;
  DeserializationError err = deserializeJson(d, f);
  f.close();
  if (err) { Serial.print("Settings: bad file, defaults. "); Serial.println(err.c_str()); return; }

  readStr(d, "wifiSsid", settings.wifiSsid);   readStr(d, "wifiPass", settings.wifiPass);
  readStr(d, "city", settings.city);           readStr(d, "owmKey", settings.owmKey);
  readInt(d, "tzHours", settings.tzHours, -12, 14);
  readInt(d, "brightness", settings.brightness, 1, 255);
  readInt(d, "mode", settings.mode, 0, MODE_COUNT - 1);
  readInt(d, "effect", settings.effect, 0, FX_COUNT - 1);
  readStr(d, "text", settings.text);
  readInt(d, "alarmHour", settings.alarmHour, 0, 23);
  readInt(d, "alarmMinute", settings.alarmMinute, 0, 59);
  readBool(d, "alarmEnabled", settings.alarmEnabled);
  readBool(d, "nightEnabled", settings.nightEnabled);
  readInt(d, "nightStart", settings.nightStart, 0, 1439);
  readInt(d, "nightEnd", settings.nightEnd, 0, 1439);
  readInt(d, "nightBrightness", settings.nightBrightness, 1, 255);
  readStr(d, "otaPassword", settings.otaPassword);
  readInt(d, "season", settings.season, 0, 4);
  lastSavedJson = toJson();
  Serial.println("Settings: loaded");
}

void applySettings() {
  globalBrightness = settings.brightness;
  currentMode = (settings.mode == MODE_ALARM) ? MODE_NORMAL : (Mode)settings.mode;
  selectedEffectIndex = settings.effect;
  displayText = settings.text;
  resetText();
  alarmHour = settings.alarmHour;
  alarmMinute = settings.alarmMinute;
  alarmEnabled = settings.alarmEnabled;
}

void markSettingsDirty() {
  dirty = true;
  dirtySince = millis();
}

void rememberState() {
  // Во время рассвета будильника яркость и режим временные — их не запоминаем.
  if (!alarmRinging) {
    settings.brightness = constrain(globalBrightness, 1, 255);
    if (currentMode != MODE_ALARM) settings.mode = currentMode;
  }
  settings.effect = constrain(selectedEffectIndex, 0, FX_COUNT - 1);
  settings.text = displayText;
  settings.alarmHour = alarmHour;
  settings.alarmMinute = alarmMinute;
  settings.alarmEnabled = alarmEnabled;
  markSettingsDirty();
}

void saveSettingsNow() {
  dirty = false;
  if (!fsReady) return;
  String json = toJson();
  if (json == lastSavedJson) return;   // ничего не поменялось — флеш не трогаем
  File f = LittleFS.open(SETTINGS_TMP, "w");
  if (!f) { Serial.println("Settings: cannot write"); return; }
  size_t written = f.print(json);
  f.close();
  if (written != json.length()) { Serial.println("Settings: write failed"); return; }
  LittleFS.remove(SETTINGS_FILE);
  LittleFS.rename(SETTINGS_TMP, SETTINGS_FILE);
  lastSavedJson = json;
  Serial.println("Settings: saved");
}

void settingsLoop() {
  if (dirty && millis() - dirtySince > 3000) saveSettingsNow();
}

long tzOffsetSeconds() {
  return (long)settings.tzHours * 3600L;
}
