#include "network.h"
#include "config.h"
#include "settings.h"
#include "modes.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <time.h>

WifiState wifiState = WIFI_IDLE;
static unsigned long stateStart = 0;    // когда вошли в текущее состояние
static bool timeConfigured = false;
static bool everConnected = false;      // было ли подключение с момента включения
static unsigned long connectedAt = 0;
static bool otaStarted = false;

static DNSServer dns;
static bool portalOn = false;

static void enter(WifiState s) {
  wifiState = s;
  stateStart = millis();
}

bool networkReady() { return wifiState == WIFI_READY; }
bool portalActive() { return portalOn; }

String apName() {
  String id = String(ESP.getChipId() & 0xFFFF, HEX);
  id.toUpperCase();
  return "LED-Clock-" + id;
}

// ---------------------------------------------------------------------
//  Точка доступа для настройки
// ---------------------------------------------------------------------
static void startPortal() {
  if (portalOn) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName());
  // Любой адрес -> часы: телефон сам откроет страницу настройки.
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  portalOn = true;
  showNotice("WI-FI " + apName() + "   192.168.4.1", 0, CRGB(0, 140, 255));
  Serial.print("Portal started: ");
  Serial.println(apName());
}

static void stopPortal() {
  if (!portalOn) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  portalOn = false;
  Serial.println("Portal stopped.");
}

// ---------------------------------------------------------------------
//  Прошивка по Wi-Fi из Arduino IDE (порт «ledclock at …»)
// ---------------------------------------------------------------------
static void drawOtaProgress(unsigned int done, unsigned int total, CRGB color) {
  FastLED.clear();
  int w = total ? (int)((uint32_t)done * MATRIX_W / total) : 0;
  for (int x = 0; x < w; x++)
    for (int y = 3; y <= 4; y++) setPixel(x, y, color);
  FastLED.setBrightness(30);
  FastLED.show();
}

static void startOta() {
  if (otaStarted) return;
  ArduinoOTA.setHostname(HOSTNAME);
  if (settings.otaPassword.length()) ArduinoOTA.setPassword(settings.otaPassword.c_str());
  ArduinoOTA.onStart([]() {
    if (ArduinoOTA.getCommand() == U_FS) LittleFS.end();
    clearNotice();
    Serial.println("OTA: start");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) { drawOtaProgress(done, total, CRGB(0, 80, 255)); });
  ArduinoOTA.onEnd([]() { drawOtaProgress(1, 1, CRGB(0, 200, 0)); });
  ArduinoOTA.onError([](ota_error_t e) { drawOtaProgress(1, 1, CRGB(200, 0, 0)); Serial.printf("OTA error %u\r\n", e); });
  ArduinoOTA.begin();                      // заодно запускает mDNS: ledclock.local
  MDNS.addService("http", "tcp", 80);
  otaStarted = true;
}

// ---------------------------------------------------------------------
void applyTimezone() {
  configTime(tzOffsetSeconds(), 0, "ntp1.ntp-servers.net", "ntp2.vniiftri.ru", "pool.ntp.org");
  timeConfigured = true;
}

void connectToWifi(const String& ssid, const String& pass) {
  settings.wifiSsid = ssid;
  settings.wifiPass = pass;
  saveSettingsNow();
  WiFi.disconnect();
  enter(WIFI_IDLE);   // на следующем шаге подключимся с новыми данными
}

static void onConnected() {
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
  if (!timeConfigured) applyTimezone();
  startOta();
  String ip = WiFi.localIP().toString();
  if (portalOn) showNotice("ПОДКЛЮЧЕНО  IP " + ip, 3, CRGB(0, 200, 60));
  else if (!everConnected) showNotice("IP " + ip, 2, CRGB(0, 200, 60));
  everConnected = true;
  connectedAt = millis();
}

void handleWiFiConnection() {
  if (portalOn) {
    dns.processNextRequest();
    // Подключились к домашней сети — через 2 минуты убираем свою сеть
    // (если к ней никто не подключён), чтобы она не висела в эфире.
    if (WiFi.status() == WL_CONNECTED && everConnected &&
        millis() - connectedAt > 120000UL && WiFi.softAPgetStationNum() == 0) {
      stopPortal();
    }
  }
  if (otaStarted) ArduinoOTA.handle();

  switch (wifiState) {
    case WIFI_IDLE:
      WiFi.persistent(false);   // пароль храним сами, флеш SDK не трогаем
      if (settings.wifiSsid.length() == 0) {
        startPortal();          // сеть не задана — сразу режим настройки
        return;
      }
      WiFi.mode(portalOn ? WIFI_AP_STA : WIFI_STA);
      WiFi.setAutoReconnect(true);
      WiFi.hostname(HOSTNAME);
      WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
      Serial.print("WiFi connecting to ");
      Serial.println(settings.wifiSsid);
      enter(WIFI_CONNECTING);
      break;

    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        onConnected();
        enter(WIFI_GET_TIME);
      } else {
        if (!everConnected && !portalOn && millis() > PORTAL_AFTER_MS) startPortal();
        if (millis() - stateStart > WIFI_TIMEOUT) {
          Serial.println("WiFi timeout, retrying...");
          WiFi.disconnect();
          enter(WIFI_IDLE);
        }
      }
      break;

    case WIFI_GET_TIME:
      if (time(nullptr) > 100000) {
        Serial.println("Time synchronized.");
        enter(WIFI_READY);
      } else if (WiFi.status() != WL_CONNECTED) {
        enter(WIFI_CONNECTING);
      } else if (millis() - stateStart > 30000) {
        Serial.println("NTP timeout, retrying...");
        enter(WIFI_IDLE);
      }
      break;

    case WIFI_READY:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost, reconnecting...");
        enter(WIFI_CONNECTING);
      }
      break;
  }
}
