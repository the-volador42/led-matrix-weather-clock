#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

// ======================== НАСТРОЙКИ ===========================
#define WIFI_SSID       "см. secrets.h"
#define WIFI_PASSWORD   "см. secrets.h"
#define OWM_API_KEY     "см. secrets.h"
#define OWM_CITY        "Moscow,RU"
#define OWM_UNITS       "metric"

#define GMT_OFFSET_SEC   (3 * 3600)
#define DAYLIGHT_OFFSET_SEC  0

#define LED_PIN         5
#define MATRIX_W        32
#define MATRIX_H        8
#define NUM_LEDS        (MATRIX_W * MATRIX_H)
#define BRIGHTNESS      180
#define MIN_BRIGHTNESS  10

#define WEATHER_CHECK_INTERVAL    (10UL * 60UL * 1000UL)
#define TEMP_DISPLAY_TIME   5000
#define LIGHTNING_DURATION  500
#define LIGHTNING_INTERVAL  3000
#define SNOW_GEN_INTERVAL   10000
#define WIND_SPEED_THRESHOLD  7.0f

#define TOUCH_PIN       14
#define DEBOUNCE_TIME   200
#define LONG_PRESS_TIME 1000
#define DOUBLE_CLICK_TIME 400
#define BRIGHTNESS_STEP 5
#define WIFI_TIMEOUT 20000
#define WEB_INTERFACE_DURATION (10UL * 60UL * 1000UL)

// --- Настройки для смены дневных эффектов ---
#define DAY_EFFECT_CHANGE_INTERVAL (2UL * 60UL * 1000UL)
// --- Настройки для смены ночных эффектов ---
#define NIGHT_EFFECT_CHANGE_INTERVAL (3UL * 60UL * 1000UL)
// ==============================================================

bool webInterfaceEnabled = false;
unsigned long webInterfaceStartTime = 0;
bool wifiConnecting = false;
CRGB leds[NUM_LEDS];
ESP8266WebServer server(80);
float moonPhase = 0.5f;

// ========== РЕЖИМЫ ==========
// MODE_EFFECT добавлен, чтобы можно было напрямую выбрать любой эффект из
// общего списка allEffects[] через веб-интерфейс (см. ниже).
enum Mode {
  MODE_NORMAL, MODE_DEMO, MODE_CLOCK, MODE_FIRE,
  MODE_ALARM, MODE_TEXT, MODE_SUNRISE, MODE_SUNSET,
  MODE_RAIN, MODE_SNOW, MODE_NIGHT, MODE_MOON, MODE_RAINBOW,
  MODE_EFFECT
};
Mode currentMode = MODE_NORMAL;
int selectedEffectIndex = 0; // индекс в allEffects[] для MODE_EFFECT

// ========== СОСТОЯНИЯ КНОПКИ ==========
enum ButtonState { BTN_IDLE, BTN_PRESS, BTN_LONG_PRESS, BTN_RELEASE };
ButtonState btnState = BTN_IDLE;
unsigned long btnPressStart = 0, btnLastClick = 0;
int btnClickCount = 0;
bool btnLongPressHandled = false;

// ========== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ==========
int globalBrightness = MIN_BRIGHTNESS; // старт с минимальной яркости
bool powerOn = true;
String displayText = "ПРИВЕТ";
int alarmHour = 7, alarmMinute = 0;
bool alarmActive = false, alarmTriggered = false;

uint8_t textChars[128];
int textLen = 0, scrollPos = MATRIX_W;

unsigned long demoEffectStart = 0, demoEffectDuration = 0;
int demoEffectIndex = 0;
unsigned long lastWeatherCheck = 0;

long  g_sunrise = 0, g_sunset = 0;
float g_windSpeed = 0.0f;
float g_rainVolume = 0.0f;
float g_snowVolume = 0.0f;
uint8_t g_clouds = 0;
int   g_weatherId = 800;

enum WifiState { WIFI_IDLE, WIFI_CONNECTING, WIFI_GET_TIME, WIFI_READY };
WifiState wifiState = WIFI_IDLE;
unsigned long ntpStartTime = 0;
bool weatherUpdateRequested = false;
bool initialSyncDone = false;

struct Star { float x, y; uint8_t phase, speed; };
struct ShootingStar { bool active; float x, y, vx, vy; int trailLen; };

// ========== XY ==========
int XY(int x, int y) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return -1;
  return (x % 2 == 0) ? x * 8 + y : x * 8 + (7 - y);
}

unsigned long lastFrame = 0;
const int FRAME_DELAY = 40;

// =====================================================================
//  ШРИФТ ЦИФР
// =====================================================================
const byte digits[10][7][5] = {
  {{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{0,1,1,1,0}},
  {{0,1,1,0,0},{1,1,1,0,0},{0,1,1,0,0},{0,1,1,0,0},{0,1,1,0,0},{0,1,1,0,0},{1,1,1,1,1}},
  {{1,1,1,1,0},{0,0,0,0,1},{0,0,0,0,1},{0,1,1,1,0},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,1}},
  {{1,1,1,1,0},{0,0,0,0,1},{0,0,0,0,1},{0,1,1,1,0},{0,0,0,0,1},{0,0,0,0,1},{1,1,1,1,0}},
  {{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{1,1,1,1,1},{0,0,0,1,0},{0,0,0,1,0},{0,0,0,1,0}},
  {{1,1,1,1,1},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,0},{0,0,0,0,1},{0,0,0,0,1},{1,1,1,1,0}},
  {{0,1,1,1,0},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{0,1,1,1,0}},
  {{1,1,1,1,1},{0,0,0,0,1},{0,0,0,1,0},{0,0,1,0,0},{0,1,0,0,0},{0,1,0,0,0},{0,1,0,0,0}},
  {{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{0,1,1,1,0}},
  {{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{0,1,1,1,1},{0,0,0,0,1},{0,0,0,0,1},{0,1,1,1,0}}
};

void drawDigit(int digit, int startX, int startY, CRGB color) {
  if (digit < 0 || digit > 9) return;
  for (int y = 0; y < 7; y++)
    for (int x = 0; x < 5; x++)
      if (digits[digit][y][x]) {
        int idx = XY(startX + x, startY + y);
        if (idx >= 0 && idx < NUM_LEDS) leds[idx] = color;
      }
}

void drawNumber(int num, CRGB color) {
  FastLED.clear();
  char str[6];
  snprintf(str, sizeof(str), "%d", num);
  int len = strlen(str);
  int totalWidth = len * 6;
  int startX = (MATRIX_W - totalWidth) / 2;
  for (int i = 0; i < len; i++) {
    char c = str[i];
    if (c == '-') {
      int idx = XY(startX + i * 6 + 2, 3);
      if (idx >= 0) leds[idx] = color;
    } else {
      drawDigit(c - '0', startX + i * 6, 0, color);
    }
  }
}

// =====================================================================
//  РУССКИЙ ШРИФТ 5x7
// =====================================================================
const byte russianLetters[33][7][5] = {
  {{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1}},
  {{1,1,1,1,1},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,0}},
  {{1,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,0}},
  {{1,1,1,1,1},{1,0,0,0,0},{1,0,0,0,0},{1,0,0,0,0},{1,0,0,0,0},{1,0,0,0,0},{1,0,0,0,0}},
  {{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1}},
  {{1,1,1,1,1},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,0},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,1}},
  {{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{0,1,1,1,0},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1}},
  {{0,1,1,1,0},{1,0,0,0,1},{0,0,0,0,1},{0,1,1,1,0},{0,0,0,0,1},{1,0,0,0,1},{0,1,1,1,0}},
  {{1,0,0,0,1},{1,0,0,1,1},{1,0,1,0,1},{1,1,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1}},
  {{1,0,0,1,0},{1,0,0,0,1},{1,0,0,1,1},{1,0,1,0,1},{1,1,0,0,1},{1,0,0,0,1},{1,0,0,0,1}},
  {{1,0,0,1,0},{1,0,1,0,0},{1,1,0,0,0},{1,1,0,0,0},{1,0,1,0,0},{1,0,0,1,0},{1,0,0,0,1}},
  {{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1}},
  {{1,0,0,0,1},{1,1,0,1,1},{1,0,1,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1}},
  {{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1}},
  {{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{0,1,1,1,0}},
  {{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1}},
  {{1,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,0},{1,0,0,0,0},{1,0,0,0,0},{1,0,0,0,0}},
  {{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,0},{1,0,0,0,0},{1,0,0,0,0},{1,0,0,0,1},{0,1,1,1,0}},
  {{1,1,1,1,1},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0}},
  {{1,0,0,0,1},{1,0,0,0,1},{0,1,0,1,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,1,1,1,0}},
  {{0,1,1,1,0},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{0,1,1,1,0},{0,0,1,0,0},{0,0,1,0,0}},
  {{1,0,0,0,1},{1,0,0,0,1},{0,1,0,1,0},{0,0,1,0,0},{0,1,0,1,0},{1,0,0,0,1},{1,0,0,0,1}},
  {{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1}},
  {{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{0,1,1,1,1},{0,0,0,0,1},{0,0,0,0,1},{0,0,0,0,1}},
  {{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,1,1,1,1}},
  {{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,1,1,1,1}},
  {{1,1,0,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,1,1,1,0},{0,0,1,0,1},{0,0,1,0,1},{0,1,1,1,0}},
  {{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1},{1,1,0,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,1,0,0,1}},
  {{1,0,0,0,0},{1,0,0,0,0},{1,0,0,0,0},{1,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,0}},
  {{0,1,1,1,0},{1,0,0,0,1},{0,0,0,0,1},{0,1,1,1,1},{0,0,0,0,1},{1,0,0,0,1},{0,1,1,1,0}},
  {{1,0,0,1,0},{1,0,1,0,1},{1,0,1,0,1},{1,1,0,1,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,0,1,0}},
  {{0,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{0,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1}},
  {{0,1,1,1,0},{1,0,0,0,1},{1,0,0,0,1},{1,1,1,1,1},{1,0,0,0,1},{1,0,0,0,1},{1,0,0,0,1}}
};

int mapUnicodeToIndex(uint16_t unicode) {
  if (unicode >= 0x0410 && unicode <= 0x042F) return unicode - 0x0410;
  if (unicode == 0x0401) return 5;
  return -1;
}

void utf8ToIndices(String input, uint8_t* outBuf, int maxLen, int& outLen) {
  outLen = 0;
  int i = 0;
  while (i < input.length() && outLen < maxLen) {
    uint16_t unicode;
    unsigned char c = input[i];
    if ((c & 0x80) == 0) { unicode = c; i++; }
    else if ((c & 0xE0) == 0xC0) { if (i+1<input.length()) { unicode = ((c&0x1F)<<6)|(input[i+1]&0x3F); i+=2; } else break; }
    else if ((c & 0xF0) == 0xE0) { if (i+2<input.length()) { unicode = ((c&0x0F)<<12)|((input[i+1]&0x3F)<<6)|(input[i+2]&0x3F); i+=3; } else break; }
    else { i++; continue; }
    int idx = mapUnicodeToIndex(unicode);
    if (idx >= 0) outBuf[outLen++] = (uint8_t)idx;
    else if (unicode == ' ') outBuf[outLen++] = 255;
  }
}

void drawLetter(int index, int startX, int startY, CRGB color) {
  if (index >= 33) return;
  for (int y = 0; y < 7; y++)
    for (int x = 0; x < 5; x++)
      if (russianLetters[index][y][x]) {
        int idx = XY(startX + x, startY + y);
        if (idx >= 0) leds[idx] = color;
      }
}

// =====================================================================
//  ДЕРЕВО — ЕДИНАЯ ФУНКЦИЯ ДЛЯ ВСЕХ ЭФФЕКТОВ
//  Раньше в коде было несколько отдельных функций (drawTreeLeft,
//  drawTreeRight, отдельная нарисованная вручную "ёлка" внутри ночного
//  эффекта и внутри эффекта ветра). Теперь всё дерево рисуется одной
//  параметризуемой функцией drawTree(), как это сделано для домика
//  (drawHouse) — а drawTrees() лишь вызывает её дважды (слева и справа).
// =====================================================================
void drawTree(int baseX, int bend, CRGB leafColor, CRGB trunkColor) {
  // baseX — левый край кроны (крона занимает 6 колонок по ширине)
  int t1 = baseX + 1, t2 = baseX + 2;
  for (int y = 5; y < 8; y++) {
    int idx1 = XY(t1, y); if (idx1 >= 0) leds[idx1] = trunkColor;
    int idx2 = XY(t2, y); if (idx2 >= 0) leds[idx2] = trunkColor;
  }
  for (int x = 0; x <= 5; x++) { int idx = XY(baseX + x + bend, 4); if (idx >= 0) leds[idx] = leafColor; }
  for (int x = 1; x <= 4; x++) { int idx = XY(baseX + x + bend, 3); if (idx >= 0) leds[idx] = leafColor; }
  for (int x = 2; x <= 3; x++) { int idx = XY(baseX + x + bend, 2); if (idx >= 0) leds[idx] = leafColor; }
  { int idx = XY(baseX + 2 + bend, 1); if (idx >= 0) leds[idx] = leafColor; }
  { int idx = XY(baseX + 3 + bend, 1); if (idx >= 0) leds[idx] = leafColor; }
}

void drawTrees(CRGB leafColor = CRGB(0, 150, 0)) {
  drawTree(0, 0, leafColor, CRGB(100, 60, 20));
  drawTree(26, 0, leafColor, CRGB(100, 60, 20));
}

// =====================================================================
//  ДОМИК
// =====================================================================
bool isHouse(int x, int y) {
  if (y == 3 && x == 26) return true;
  if (y == 4 && (x >= 25 && x <= 27)) return true;
  if (y == 5 && (x >= 24 && x <= 28)) return true;
  if (y == 6 && (x >= 25 && x <= 27)) return true;
  if (y == 7 && (x >= 24 && x <= 28)) return true;
  return false;
}

void drawHouse() {
  int idx;
  idx = XY(26,3); if(idx>=0) leds[idx] = CRGB(200, 100, 0);
  idx = XY(25,4); if(idx>=0) leds[idx] = CRGB(200, 100, 0);
  idx = XY(27,4); if(idx>=0) leds[idx] = CRGB(200, 100, 0);
  idx = XY(24,5); if(idx>=0) leds[idx] = CRGB(200, 100, 0);
  idx = XY(25,5); if(idx>=0) leds[idx] = CRGB(200, 100, 0);
  idx = XY(26,5); if(idx>=0) leds[idx] = CRGB(200, 100, 0);
  idx = XY(27,5); if(idx>=0) leds[idx] = CRGB(200, 100, 0);
  idx = XY(28,5); if(idx>=0) leds[idx] = CRGB(200, 100, 0);
  idx = XY(26,4); if(idx>=0) leds[idx] = CRGB(80, 40, 10);
  idx = XY(25,6); if(idx>=0) leds[idx] = CRGB(80, 40, 10);
  idx = XY(26,6); if(idx>=0) leds[idx] = CRGB(80, 40, 10);
  idx = XY(27,6); if(idx>=0) leds[idx] = CRGB(80, 40, 10);
  idx = XY(24,7); if(idx>=0) leds[idx] = CRGB(80, 40, 10);
  idx = XY(25,7); if(idx>=0) leds[idx] = CRGB(80, 40, 10);
  idx = XY(26,7); if(idx>=0) leds[idx] = CRGB(80, 40, 10);
  idx = XY(27,7); if(idx>=0) leds[idx] = CRGB(80, 40, 10);
  idx = XY(28,7); if(idx>=0) leds[idx] = CRGB(80, 40, 10);
  idx = XY(26,5); if(idx>=0) leds[idx] = CRGB(100, 150, 255);
}

// =====================================================================
//  ДОЖДЬ, МОЛНИЯ
// =====================================================================
#define RAIN_DROPS 24
float rainY[RAIN_DROPS]; int rainX[RAIN_DROPS]; float rainSpeed[RAIN_DROPS];
bool rainInit1 = false;
void resetRain1() { rainInit1 = false; }
void rainEffect1() {
  if (!rainInit1) {
    for (int i = 0; i < RAIN_DROPS; i++) {
      rainX[i] = random(0, MATRIX_W); rainY[i] = random(0, MATRIX_H - 1);
      rainSpeed[i] = random(20, 60) / 100.0;
    }
    rainInit1 = true;
  }
  fadeToBlackBy(leds, NUM_LEDS, 80);
  for (int x = 0; x < MATRIX_W; x++) {int idx = XY(x, 7); if(idx>=0) leds[idx] = CRGB(0, 160, 0);}
  drawHouse();
  for (int i = 0; i < RAIN_DROPS; i++) {
    rainY[i] += rainSpeed[i];
    if (rainY[i] >= MATRIX_H - 1) {
      int sx = rainX[i];
      if (sx > 0 && !isHouse(sx - 1, 6)) {int idx = XY(sx - 1, 6); if(idx>=0) leds[idx] = CRGB(150, 200, 255);}
      if (sx < MATRIX_W - 1 && !isHouse(sx + 1, 6)) {int idx = XY(sx + 1, 6); if(idx>=0) leds[idx] = CRGB(150, 200, 255);}
      rainY[i] = 0; rainX[i] = random(0, MATRIX_W); rainSpeed[i] = random(20, 60) / 100.0;
    } else {
      int px = rainX[i], py = (int)rainY[i];
      if (!isHouse(px, py) && px >= 0 && px < MATRIX_W) {
        int idx = XY(px, py); if (idx >= 0) leds[idx] = CRGB(40, 60, 140);
      }
    }
  }
}

unsigned long lastLightningCheck = 0;
bool lightningActive = false;
unsigned long lightningStart = 0;
float lightningPoints[5][2];

void generateLightning() {
  float startX = random(5, MATRIX_W - 5);
  lightningPoints[0][0] = startX; lightningPoints[0][1] = 0.0;
  for (int i = 1; i < 4; i++) {
    lightningPoints[i][0] = constrain(lightningPoints[i-1][0] + random(-3, 4), 0.0f, (float)(MATRIX_W-1));
    lightningPoints[i][1] = lightningPoints[i-1][1] + 1.0;
  }
  lightningPoints[4][0] = constrain(lightningPoints[3][0] + random(-2, 3), 0.0f, (float)(MATRIX_W-1));
  lightningPoints[4][1] = 4.0;
}

void drawLightning() {
  for (int i = 0; i < 4; i++) {
    int x0 = lightningPoints[i][0], y0 = lightningPoints[i][1];
    int x1 = lightningPoints[i+1][0], y1 = lightningPoints[i+1][1];
    int dx = abs(x1-x0), dy = -abs(y1-y0);
    int sx = x0<x1 ? 1 : -1, sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2;
    for (;;) {
      if (x0>=0 && x0<MATRIX_W && y0>=0 && y0<5) {
        int idx = XY(x0,y0); if(idx>=0) leds[idx] = CRGB::White;
        if (x0>0) {int idx2 = XY(x0-1,y0); if(idx2>=0) leds[idx2] = CRGB(100,100,100);}
        if (x0<MATRIX_W-1) {int idx2 = XY(x0+1,y0); if(idx2>=0) leds[idx2] = CRGB(100,100,100);}
      }
      if (x0==x1 && y0==y1) break;
      e2 = 2*err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
}

bool rainInit2 = false;
void resetRain2() { rainInit2 = false; lightningActive = false; }
void rainEffect2() {
  if (!rainInit2) {
    for (int i = 0; i < RAIN_DROPS; i++) {
      rainX[i] = random(0, MATRIX_W); rainY[i] = random(0, MATRIX_H - 1);
      rainSpeed[i] = random(40, 90) / 100.0;
    }
    rainInit2 = true;
    lastLightningCheck = millis();
  }
  if (!lightningActive) {
    if (millis() - lastLightningCheck > LIGHTNING_INTERVAL) {
      generateLightning();
      lightningActive = true;
      lightningStart = millis();
      lastLightningCheck = millis();
    }
  } else if (millis() - lightningStart > LIGHTNING_DURATION) lightningActive = false;

  fadeToBlackBy(leds, NUM_LEDS, 110);
  for (int x = 0; x < MATRIX_W; x++) {int idx = XY(x, 7); if(idx>=0) leds[idx] = CRGB(0, 160, 0);}
  drawHouse();
  for (int i = 0; i < RAIN_DROPS; i++) {
    rainY[i] += rainSpeed[i];
    if (rainY[i] >= MATRIX_H - 1) {
      int sx = rainX[i];
      if (sx > 0 && !isHouse(sx - 1, 6)) {int idx = XY(sx - 1, 6); if(idx>=0) leds[idx] = CRGB(150, 200, 255);}
      if (sx < MATRIX_W - 1 && !isHouse(sx + 1, 6)) {int idx = XY(sx + 1, 6); if(idx>=0) leds[idx] = CRGB(150, 200, 255);}
      rainY[i] = 0; rainX[i] = random(0, MATRIX_W); rainSpeed[i] = random(40, 90) / 100.0;
    } else {
      int px = rainX[i], py = (int)rainY[i];
      if (!isHouse(px, py) && px >= 0 && px < MATRIX_W) {
        int idx = XY(px, py); if (idx >= 0) leds[idx] = CRGB(50, 70, 160);
      }
    }
  }
  if (lightningActive) drawLightning();
}

bool rainInit3 = false;
void resetRain3() { rainInit3 = false; }
void rainEffect3() {
  if (!rainInit3) {
    for (int i = 0; i < RAIN_DROPS / 2; i++) {
      rainX[i] = random(0, MATRIX_W); rainY[i] = random(0, MATRIX_H - 1);
      rainSpeed[i] = random(8, 20) / 100.0;
    }
    rainInit3 = true;
  }
  fadeToBlackBy(leds, NUM_LEDS, 50);
  for (int x = 0; x < MATRIX_W; x++) {int idx = XY(x, 7); if(idx>=0) leds[idx] = CRGB(0, 160, 0);}
  drawHouse();
  for (int i = 0; i < RAIN_DROPS / 2; i++) {
    rainY[i] += rainSpeed[i];
    if (rainY[i] >= MATRIX_H - 1) {
      int sx = rainX[i];
      if (sx > 0 && !isHouse(sx - 1, 6)) {int idx = XY(sx - 1, 6); if(idx>=0) leds[idx] = CRGB(100, 160, 220);}
      if (sx < MATRIX_W - 1 && !isHouse(sx + 1, 6)) {int idx = XY(sx + 1, 6); if(idx>=0) leds[idx] = CRGB(100, 160, 220);}
      rainY[i] = 0; rainX[i] = random(0, MATRIX_W); rainSpeed[i] = random(8, 20) / 100.0;
    } else {
      int px = rainX[i], py = (int)rainY[i];
      if (!isHouse(px, py) && px >= 0 && px < MATRIX_W) {
        int idx = XY(px, py); if (idx >= 0) leds[idx] = CRGB(25, 40, 90);
      }
    }
  }
}

#define RAIN_DROPS_HEAVY 30
float rainYheavy[RAIN_DROPS_HEAVY]; int rainXheavy[RAIN_DROPS_HEAVY]; float rainSpeedHeavy[RAIN_DROPS_HEAVY];
bool rainInit4 = false;
void resetRain4() { rainInit4 = false; }
void rainEffect4() {
  if (!rainInit4) {
    for (int i = 0; i < RAIN_DROPS_HEAVY; i++) {
      rainXheavy[i] = random(0, MATRIX_W); rainYheavy[i] = random(0, MATRIX_H - 1);
      rainSpeedHeavy[i] = random(50, 100) / 100.0;
    }
    rainInit4 = true;
  }
  fadeToBlackBy(leds, NUM_LEDS, 130);
  for (int x = 0; x < MATRIX_W; x++) {int idx = XY(x, 7); if(idx>=0) leds[idx] = CRGB(0, 160, 0);}
  drawHouse();
  for (int i = 0; i < RAIN_DROPS_HEAVY; i++) {
    rainYheavy[i] += rainSpeedHeavy[i];
    if (rainYheavy[i] >= MATRIX_H - 1) {
      int sx = rainXheavy[i];
      if (sx > 0 && !isHouse(sx - 1, 6)) {int idx = XY(sx - 1, 6); if(idx>=0) leds[idx] = CRGB(150, 200, 255);}
      if (sx < MATRIX_W - 1 && !isHouse(sx + 1, 6)) {int idx = XY(sx + 1, 6); if(idx>=0) leds[idx] = CRGB(150, 200, 255);}
      rainYheavy[i] = 0; rainXheavy[i] = random(0, MATRIX_W); rainSpeedHeavy[i] = random(50, 100) / 100.0;
    } else {
      int px = rainXheavy[i], py = (int)rainYheavy[i];
      if (!isHouse(px, py) && px >= 0 && px < MATRIX_W) {
        int idx = XY(px, py); if (idx >= 0) leds[idx] = CRGB(60, 80, 180);
      }
    }
  }
}

// =====================================================================
//  СНЕГ
// =====================================================================
#define SNOW_FLAKES 20
float snowY[SNOW_FLAKES], snowX[SNOW_FLAKES], snowSpeed[SNOW_FLAKES], snowDrift[SNOW_FLAKES];
bool snowInit1 = false;
uint8_t snowDrifts1[MATRIX_W] = {0};
unsigned long lastSnowGen1 = 0;
void resetSnow1() { snowInit1 = false; memset(snowDrifts1, 0, sizeof(snowDrifts1)); lastSnowGen1 = 0; }
void snowEffect1() {
  if (!snowInit1) {
    for (int i = 0; i < SNOW_FLAKES; i++) {
      snowX[i] = random(0, MATRIX_W * 10) / 10.0; snowY[i] = random(0, MATRIX_H);
      snowSpeed[i] = random(5, 15) / 100.0; snowDrift[i] = random(-5, 5) / 100.0;
    }
    snowInit1 = true; lastSnowGen1 = millis();
  }
  fadeToBlackBy(leds, NUM_LEDS, 40);
  for (int x = 0; x < MATRIX_W; x++) {int idx = XY(x, 7); if(idx>=0) leds[idx] = CRGB(200, 210, 230);}
  if (millis() - lastSnowGen1 > SNOW_GEN_INTERVAL) {
    lastSnowGen1 = millis();
    int r = random(0, 100), x = random(0, MATRIX_W);
    if (r < 50) snowDrifts1[x] = 1;
    else if (r < 80) snowDrifts1[x] = 2;
    else if (r < 90) snowDrifts1[x] = 3;
    if (random(0, 100) < 20) { int tx = random(0, MATRIX_W); if (snowDrifts1[tx] > 0) snowDrifts1[tx]--; }
  }
  for (int x = 0; x < MATRIX_W; x++) {
    if (snowDrifts1[x] > 0) {
      int idx = XY(x, 6); if(idx>=0) leds[idx] = CRGB(220, 225, 240);
      if (snowDrifts1[x] >= 2) {idx = XY(x, 5); if(idx>=0) leds[idx] = CRGB(220, 225, 240);}
      if (snowDrifts1[x] >= 3) {idx = XY(x, 4); if(idx>=0) leds[idx] = CRGB(220, 225, 240);}
    }
  }
  drawHouse();
  for (int i = 0; i < SNOW_FLAKES; i++) {
    int px = (int)snowX[i], py = (int)snowY[i];
    if (!isHouse(px, py) && px >= 0 && px < MATRIX_W && py >= 0) {
      int idx = XY(px, py); if(idx>=0) leds[idx] = CRGB(200, 210, 230);
    }
    snowY[i] += snowSpeed[i]; snowX[i] += sin(snowY[i] * 0.5) * 0.08 + snowDrift[i];
    if (snowX[i] < 0) snowX[i] = MATRIX_W - 1;
    if (snowX[i] >= MATRIX_W) snowX[i] = 0;
    if (snowY[i] >= MATRIX_H) { snowY[i] = 0; snowX[i] = random(0, MATRIX_W * 10) / 10.0; snowSpeed[i] = random(5, 15) / 100.0; }
  }
}

uint8_t snowDrifts2[MATRIX_W] = {0};
unsigned long lastSnowGen2 = 0;
bool snowInit2 = false;
void resetSnow2() { snowInit2 = false; memset(snowDrifts2, 0, sizeof(snowDrifts2)); lastSnowGen2 = 0; }
void snowEffect2() {
  if (!snowInit2) {
    for (int i = 0; i < SNOW_FLAKES; i++) {
      snowX[i] = random(0, MATRIX_W * 10) / 10.0; snowY[i] = random(0, MATRIX_H);
      snowSpeed[i] = random(15, 30) / 100.0;
    }
    snowInit2 = true; lastSnowGen2 = millis();
  }
  fadeToBlackBy(leds, NUM_LEDS, 60);
  for (int x = 0; x < MATRIX_W; x++) {int idx = XY(x, 7); if(idx>=0) leds[idx] = CRGB(200, 210, 230);}
  if (millis() - lastSnowGen2 > SNOW_GEN_INTERVAL) {
    lastSnowGen2 = millis();
    int r = random(0, 100), x = random(0, MATRIX_W);
    if (r < 50) snowDrifts2[x] = 1;
    else if (r < 80) snowDrifts2[x] = 2;
    else if (r < 90) snowDrifts2[x] = 3;
    if (random(0, 100) < 20) { int tx = random(0, MATRIX_W); if (snowDrifts2[tx] > 0) snowDrifts2[tx]--; }
  }
  for (int x = 0; x < MATRIX_W; x++) {
    if (snowDrifts2[x] > 0) {
      int idx = XY(x, 6); if(idx>=0) leds[idx] = CRGB(220, 225, 240);
      if (snowDrifts2[x] >= 2) {idx = XY(x, 5); if(idx>=0) leds[idx] = CRGB(220, 225, 240);}
      if (snowDrifts2[x] >= 3) {idx = XY(x, 4); if(idx>=0) leds[idx] = CRGB(220, 225, 240);}
    }
  }
  drawHouse();
  for (int i = 0; i < SNOW_FLAKES; i++) {
    int px = (int)snowX[i], py = (int)snowY[i];
    if (!isHouse(px, py) && px >= 0 && px < MATRIX_W && py >= 0) {
      int idx = XY(px, py); if(idx>=0) leds[idx] = CRGB(220, 225, 240);
    }
    snowY[i] += snowSpeed[i]; snowX[i] += 0.2;
    if (snowX[i] >= MATRIX_W) snowX[i] = 0;
    if (snowY[i] >= MATRIX_H) { snowY[i] = 0; snowX[i] = random(0, MATRIX_W * 10) / 10.0; snowSpeed[i] = random(15, 30) / 100.0; }
  }
}

// =====================================================================
//  СОЛНЦЕ, ОБЛАЧНОСТЬ
// =====================================================================
#define NUM_CLOUDS_SUN 3
float sunCloudBaseX[NUM_CLOUDS_SUN] = {2.0, 10.0, 18.0};
float sunCloudBaseY[NUM_CLOUDS_SUN] = {0.8, 0.5, 0.9};
float sunCloudPhaseX[NUM_CLOUDS_SUN], sunCloudPhaseY[NUM_CLOUDS_SUN];
const float sunCloudSpeedX[NUM_CLOUDS_SUN] = {0.015, 0.012, 0.010};
const float sunCloudSpeedY[NUM_CLOUDS_SUN] = {0.020, 0.018, 0.025};
const byte sunCloudBitmap[3][4] = {{0,1,1,0},{1,1,1,1},{1,1,1,1}};
void resetSun2() { for (int i=0;i<NUM_CLOUDS_SUN;i++){ sunCloudPhaseX[i]=random(0,628)/100.0; sunCloudPhaseY[i]=random(0,628)/100.0; } }
void sunEffect2() {
  for (int y = 0; y <= 5; y++) for (int x = 0; x < MATRIX_W; x++) {
    int idx = XY(x,y); if(idx>=0) leds[idx] = CRGB(0,120,220);
  }
  for (int i=0;i<NUM_CLOUDS_SUN;i++) {
    float offX=sin(sunCloudPhaseX[i])*2.0, offY=sin(sunCloudPhaseY[i])*1.2;
    int cx=(int)(sunCloudBaseX[i]+offX), cy=(int)(sunCloudBaseY[i]+offY);
    for (int r=0;r<3;r++) for (int c=0;c<4;c++) if(sunCloudBitmap[r][c]) {
      int dx=cx+c, dy=cy+r;
      if(dx>=0 && dx<MATRIX_W && dy>=0 && dy<6) {
        int idx = XY(dx,dy); if(idx>=0) leds[idx]=CRGB(220,225,240);
      }
    }
    sunCloudPhaseX[i]+=sunCloudSpeedX[i]; sunCloudPhaseY[i]+=sunCloudSpeedY[i];
    if(sunCloudPhaseX[i]>6.283)sunCloudPhaseX[i]-=6.283;
    if(sunCloudPhaseY[i]>6.283)sunCloudPhaseY[i]-=6.283;
  }
  for (int y=0;y<3;y++) for (int x=25;x<29;x++) {
    int idx = XY(x,y); if(idx>=0) leds[idx] = CRGB(255,230,0);
  }
  for (int y=6;y<8;y++) for (int x=0;x<MATRIX_W;x++) {
    int idx = XY(x,y); if(idx>=0) leds[idx] = CRGB(0,180,40);
  }
}

#define NUM_CLOUDS_OVERCAST 5
float overcastCloudBaseX[NUM_CLOUDS_OVERCAST] = {2.0,8.0,14.0,20.0,26.0};
float overcastCloudBaseY[NUM_CLOUDS_OVERCAST] = {0.8,1.0,0.5,1.2,0.9};
float overcastCloudPhaseX[NUM_CLOUDS_OVERCAST], overcastCloudPhaseY[NUM_CLOUDS_OVERCAST];
const float overcastCloudSpeedX[NUM_CLOUDS_OVERCAST] = {0.008,0.010,0.007,0.009,0.006};
const float overcastCloudSpeedY[NUM_CLOUDS_OVERCAST] = {0.012,0.015,0.010,0.013,0.011};
const byte overcastCloudBitmap[3][4] = {{0,1,1,0},{1,1,1,1},{1,1,1,1}};
void resetSunCloudy() { for(int i=0;i<NUM_CLOUDS_OVERCAST;i++){ overcastCloudPhaseX[i]=random(0,628)/100.0; overcastCloudPhaseY[i]=random(0,628)/100.0; } }
void sunCloudyEffect() {
  for(int y=0;y<=5;y++) for(int x=0;x<MATRIX_W;x++) {
    int idx = XY(x,y); if(idx>=0) leds[idx] = CRGB(10,30,90);
  }
  for(int i=0;i<NUM_CLOUDS_OVERCAST-1;i++){
    float offX=sin(overcastCloudPhaseX[i])*1.5, offY=sin(overcastCloudPhaseY[i])*1.2;
    int cx=(int)(overcastCloudBaseX[i]+offX), cy=(int)(overcastCloudBaseY[i]+offY);
    for(int r=0;r<3;r++) for(int c=0;c<4;c++) if(overcastCloudBitmap[r][c]){
      int dx=cx+c, dy=cy+r;
      if(dx>=0 && dx<MATRIX_W && dy>=0 && dy<6) {
        int idx = XY(dx,dy); if(idx>=0) leds[idx] = CRGB(140,150,170);
      }
    }
    overcastCloudPhaseX[i]+=overcastCloudSpeedX[i]; overcastCloudPhaseY[i]+=overcastCloudSpeedY[i];
    if(overcastCloudPhaseX[i]>6.283)overcastCloudPhaseX[i]-=6.283;
    if(overcastCloudPhaseY[i]>6.283)overcastCloudPhaseY[i]-=6.283;
  }
  int i=NUM_CLOUDS_OVERCAST-1;
  float offX=sin(overcastCloudPhaseX[i])*1.5, offY=sin(overcastCloudPhaseY[i])*1.2;
  int cx=(int)(overcastCloudBaseX[i]+offX), cy=(int)(overcastCloudBaseY[i]+offY);
  for(int r=0;r<3;r++) for(int c=0;c<4;c++) if(overcastCloudBitmap[r][c]){
    int dx=cx+c, dy=cy+r;
    if(dx>=0 && dx<MATRIX_W && dy>=0 && dy<6) {
      int idx = XY(dx,dy); if(idx>=0) leds[idx] = CRGB(200,180,100);
    }
  }
  overcastCloudPhaseX[i]+=overcastCloudSpeedX[i]; overcastCloudPhaseY[i]+=overcastCloudSpeedY[i];
  if(overcastCloudPhaseX[i]>6.283)overcastCloudPhaseX[i]-=6.283;
  if(overcastCloudPhaseY[i]>6.283)overcastCloudPhaseY[i]-=6.283;
  for(int y=6;y<8;y++) for(int x=0;x<MATRIX_W;x++) {
    int idx = XY(x,y); if(idx>=0) leds[idx] = CRGB(0,100,20);
  }
}

// =====================================================================
//  ЗВЁЗДЫ
// =====================================================================
#define MAX_STARS 16
Star stars[MAX_STARS]; uint8_t numStars = 0;
ShootingStar shooting = {false,0,0,0,0,0};
unsigned long lastStarEvent = 0;
const unsigned long STAR_EVENT_INTERVAL = 10000;

bool isStarPositionFree(float x, float y) {
  if (y >= 5.0f || y < 0.0f) return false;
  if (x >= 22.0f && x <= 28.0f && y <= 4.5f) return false;
  if (y >= 4.5f && x >= 0.0f && x <= 5.0f) return false;
  if (y >= 3.5f && y < 4.5f && x >= 0.5f && x <= 4.5f) return false;
  if (y >= 2.5f && y < 3.5f && x >= 1.5f && x <= 3.5f) return false;
  for (int i = 0; i < numStars; i++) {
    float dx = x - stars[i].x, dy = y - stars[i].y;
    if (sqrt(dx*dx + dy*dy) < 2.5) return false;
  }
  return true;
}

bool tryPlaceStar(Star &s) {
  for (int attempt = 0; attempt < 100; attempt++) {
    float tx = random(0, MATRIX_W);
    float ty = random(0, 5);
    if (isStarPositionFree(tx, ty)) {
      s.x = tx; s.y = ty; s.phase = random(0,255); s.speed = random(1,4);
      return true;
    }
  }
  return false;
}

void resetStars() {
  numStars = 0;
  int desired = random(3,17);
  for (int i=0;i<desired;i++) {
    if (numStars >= MAX_STARS) break;
    Star newStar;
    if (tryPlaceStar(newStar)) stars[numStars++] = newStar;
  }
  shooting.active = false;
  lastStarEvent = millis();
}

void updateStars() {
  if (millis() - lastStarEvent > STAR_EVENT_INTERVAL) {
    lastStarEvent = millis();
    if (random(0,100) < 30) {
      if (random(2) == 0) {
        if (numStars < MAX_STARS) {
          Star newStar;
          if (tryPlaceStar(newStar)) stars[numStars++] = newStar;
        } else if (numStars > 0 && !shooting.active) {
          int idx = random(0, numStars);
          shooting.active = true;
          shooting.x = stars[idx].x; shooting.y = stars[idx].y;
          shooting.vx = -1.2; shooting.vy = 1.2;
          shooting.trailLen = 4;
          stars[idx] = stars[--numStars];
        }
      } else {
        if (numStars > 0 && !shooting.active) {
          int idx = random(0, numStars);
          shooting.active = true;
          shooting.x = stars[idx].x; shooting.y = stars[idx].y;
          shooting.vx = -1.2; shooting.vy = 1.2;
          shooting.trailLen = 4;
          stars[idx] = stars[--numStars];
        }
      }
    }
  }
  if (shooting.active) {
    shooting.x += shooting.vx; shooting.y += shooting.vy;
    if (shooting.x < 0 || shooting.x >= MATRIX_W || shooting.y >= 5.0 || shooting.y < 0)
      shooting.active = false;
  }
}

void drawStars() {
  for (int i=0;i<numStars;i++) {
    uint8_t brightness = sin8(stars[i].phase);
    brightness = map(brightness, 0, 255, 80, 255);
    CRGB color = CRGB(180,200,255); color.nscale8_video(brightness);
    int px = (int)stars[i].x, py = (int)stars[i].y;
    if (px>=0 && px<MATRIX_W && py>=0 && py<5) {
      int idx = XY(px,py); if(idx>=0) leds[idx] = color;
    }
    stars[i].phase += stars[i].speed;
  }
  if (shooting.active) {
    float sx = shooting.x, sy = shooting.y;
    for (int t = 0; t < shooting.trailLen; t++) {
      int px = (int)(sx - t*shooting.vx*0.7), py = (int)(sy - t*shooting.vy*0.7);
      if (px>=0 && px<MATRIX_W && py>=0 && py<5) {
        int idx = XY(px,py); if(idx>=0) leds[idx] = CRGB(255,230,150).nscale8_video(map(t,0,shooting.trailLen-1,255,40));
      }
    }
  }
}

// =====================================================================
//  ЛУНА
// =====================================================================
const byte moonCrescent[5][5] = {
  {0,1,1,1,0},{1,1,1,1,0},{1,1,1,0,0},{1,1,1,1,0},{0,1,1,1,0}
};
const byte moonFull[5][5] = {
  {0,1,1,1,0},{1,1,1,1,1},{1,1,1,1,1},{1,1,1,1,1},{0,1,1,1,0}
};
const byte moonWaningCrescent[5][5] = {
  {0,1,1,1,0},{0,1,1,1,1},{0,0,1,1,1},{0,1,1,1,1},{0,1,1,1,0}
};

void drawMoonByPhase(int x0, int y0, float phase) {
  if (phase < 0.02f || phase > 0.98f) {
    for (int y = 0; y < 5; y++)
      for (int x = 0; x < 5; x++)
        if (moonCrescent[y][x]) {
          int idx = XY(x0 + x, y0 + y);
          if(idx >= 0) leds[idx] = CRGB(40, 40, 40);
        }
    return;
  }
  const byte (*mask)[5] = nullptr;
  CRGB color;
  if (phase > 0.45f && phase < 0.55f) { mask = moonFull; color = CRGB(255, 240, 170); }
  else if (phase <= 0.45f) { mask = moonCrescent; color = CRGB(255, 220, 120); }
  else { mask = moonWaningCrescent; color = CRGB(255, 220, 120); }
  if (mask) {
    for (int y = 0; y < 5; y++)
      for (int x = 0; x < 5; x++)
        if (mask[y][x]) {
          int idx = XY(x0 + x, y0 + y);
          if(idx >= 0) leds[idx] = color;
        }
  }
}

float getMoonPhase(time_t t) {
  struct tm *gmt = gmtime(&t);
  int y = gmt->tm_year + 1900, m = gmt->tm_mon + 1, d = gmt->tm_mday;
  double jd = (1461 * (y + 4800 + (m - 14) / 12)) / 4 +
              (367 * (m - 2 - 12 * ((m - 14) / 12))) / 12 -
              (3 * ((y + 4900 + (m - 14) / 12) / 100)) / 4 +
              d - 32075 - 0.5 + gmt->tm_hour / 24.0 + gmt->tm_min / 1440.0 + gmt->tm_sec / 86400.0;
  double knownNewMoon = 2451544.5;
  double age = jd - knownNewMoon;
  double phase = fmod(age, 29.53058867) / 29.53058867;
  if (phase < 0) phase += 1;
  return (float)phase;
}

void nightEffectPhase() {
  time_t now = time(nullptr);
  if (now > 100000) {
    moonPhase = getMoonPhase(now);
  }
  FastLED.clear();
  for (int y = 0; y < 6; y++) {
    for (int x = 0; x < 32; x++) {
      CRGB c = CRGB(5, 10, 45);
      int dx = x - 25, dy = y - 2;
      int d = sqrt(dx*dx + dy*dy);
      if (d < 7) { c.r += (7-d)*5; c.g += (7-d); c.b += (7-d)*2; }
      int idx = XY(x,y); if(idx>=0) leds[idx] = c;
    }
  }
  drawMoonByPhase(23, 0, moonPhase);
  for (int y=6; y<8; y++)
    for (int x=0; x<32; x++) {
      int idx = XY(x,y); if(idx>=0) leds[idx] = CRGB(0, 40, 10);
    }
  // единая функция дерева вместо ручной отрисовки — только левое дерево,
  // справа стоит домик
  drawTree(0, 0, CRGB(0, 150, 0), CRGB(100, 60, 20));
  updateStars();
  drawStars();
}

// =====================================================================
//  ДИНАМИЧЕСКИЙ ЗАКАТ / РАССВЕТ / ВЕТЕР
// =====================================================================

float getSunsetProgress() {
  if (g_sunset < 100000) {
    return (millis() % 60000) / 60000.0f;
  }
  long now = (long)time(nullptr);
  long window = 3600;
  long start  = g_sunset - window;
  if (now <= start) return 0.0f;
  if (now >= g_sunset) return 1.0f;
  return (float)(now - start) / (float)window;
}

float getSunriseProgress() {
  if (g_sunrise < 100000) return (millis() % 60000) / 60000.0f;
  long now = (long)time(nullptr);
  long window = 3600;
  long start = g_sunrise - window;
  if (now <= start) return 0.0f;
  if (now >= g_sunrise) return 1.0f;
  return (float)(now - start) / (float)window;
}

void drawSun(int x0, int y0, uint8_t r, uint8_t g, uint8_t b) {
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      int px = x0 + dx, py = y0 + dy;
      if (py < 0 || py > 5) continue;
      int idx = XY(px, py);
      if (idx < 0) continue;
      if (dx == 0 && dy == 0) leds[idx] = CRGB(r, g, b);
      else leds[idx] = CRGB(r/2, g/2, b/2);
    }
  }
}

void sunsetDynamicEffect() {
  float p = getSunsetProgress();
  FastLED.clear();

  for (int y = 0; y < 6; y++) {
    float py = (float)y / 5.0f;
    uint8_t r = (uint8_t)(20  + p * 210 * (1.0f - py));
    uint8_t g = (uint8_t)(100 - p * 70  * (1.0f - py));
    uint8_t b = (uint8_t)(200 - p * 160 * (1.0f - py));
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(r, g, b);
    }
  }

  int sunY = 1 + (int)(p * 4.0f);
  uint8_t sr = 255;
  uint8_t sg = (uint8_t)(230 - p * 130);
  uint8_t sb = (uint8_t)(80  - p * 70);
  drawSun(16, sunY, sr, sg, sb);

  for (int y = 6; y < 8; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) {
        uint8_t gr = (uint8_t)(30  - p * 20);
        uint8_t gg = (uint8_t)(70  - p * 55);
        leds[idx] = CRGB(gr, gg, 8);
      }
    }
  }
  drawTrees();
}

void dawnDynamicEffect() {
  float p = getSunriseProgress();
  FastLED.clear();
  for (int y = 0; y < 6; y++) {
    float py = (float)y / 5.0f;
    uint8_t r = (uint8_t)(10 + p * 200 * (1.0f - py));
    uint8_t g = (uint8_t)(40 + p * 130 * (1.0f - py));
    uint8_t b = (uint8_t)(120 + p * 80);
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(r, g, b);
    }
  }
  int sunY = 5 - (int)(p * 4.0f);
  drawSun(16, sunY, 255, 220, (uint8_t)(60 + p * 60));
  for (int y = 6; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(0, (uint8_t)(90 + p * 50), 20);
    }
  drawTrees();
}

// ---- ВЕТЕР: дерево с раскачивающейся кроной и срывающимися листьями ----
float windPhase = 0;

#define WIND_LEAVES 14
struct WindLeaf { bool active; float x, y, vx, vy, wob; uint8_t life; };
WindLeaf windLeaves[WIND_LEAVES];
unsigned long lastLeafSpawn = 0;

void resetWind() {
  windPhase = 0;
  lastLeafSpawn = millis();
  for (int i = 0; i < WIND_LEAVES; i++) windLeaves[i].active = false;
}

void spawnWindLeaf(int fromX, int fromY) {
  for (int i = 0; i < WIND_LEAVES; i++) {
    if (!windLeaves[i].active) {
      windLeaves[i].active = true;
      windLeaves[i].x = fromX;
      windLeaves[i].y = fromY;
      // лист срывается и летит "назад" по случайной дуге — направление
      // и кривизна траектории каждый раз выбираются случайно
      float dir = (random(0, 2) == 0) ? -1.0f : 1.0f;
      windLeaves[i].vx = dir * (0.25f + random(0, 45) / 100.0f);
      windLeaves[i].vy = -0.25f - random(0, 30) / 100.0f;
      windLeaves[i].wob = random(0, 628) / 100.0f;
      windLeaves[i].life = 255;
      return;
    }
  }
}

void windEffect() {
  float windStrength = constrain(g_windSpeed, 0.0f, 20.0f);
  float speed = 0.06f + windStrength * 0.02f;
  windPhase += speed;

  // небо
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(20, 30, 55);
    }

  // потоки ветра (полосы, летящие по небу)
  int streaks = 6 + (int)constrain(windStrength, 0.0f, 10.0f);
  for (int i = 0; i < streaks; i++) {
    float t = windPhase * (1.0f + i * 0.13f) + i * 2.7f;
    int x = ((int)(t * 8.0f)) % (MATRIX_W + 12) - 6;
    int y = 1 + (i % 4);
    for (int k = 0; k < 4; k++) {
      int px = x - k;
      int idx = XY(px, y);
      if (idx >= 0 && px >= 0) {
        uint8_t br = 220 - k * 50;
        leds[idx] = CRGB(180, 200, 230).nscale8_video(br);
      }
    }
  }

  // земля
  for (int y = 6; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(0, 110, 40);
    }

  // раскачивающаяся крона: обе кроны качаются в противофазе относительно
  // друг друга, амплитуда растёт вместе с силой ветра
  int bend = (int)round(sin(windPhase) * (1.0f + windStrength * 0.25f));
  drawTree(0, bend, CRGB(0, 150, 0), CRGB(100, 60, 20));
  drawTree(26, -bend, CRGB(0, 150, 0), CRGB(100, 60, 20));

  // время от времени срываем зелёные частицы (листья) с кроны
  unsigned long spawnInterval = (unsigned long)constrain(1400 - windStrength * 60, 250, 1400);
  if (millis() - lastLeafSpawn > spawnInterval) {
    lastLeafSpawn = millis();
    if (random(0, 100) < 40 + (int)(windStrength * 3)) {
      bool leftTree = random(0, 2) == 0;
      int fx = leftTree ? (2 + random(-1, 2) + bend) : (28 + random(-1, 2) - bend);
      int fy = 2 + random(0, 3);
      spawnWindLeaf(fx, fy);
    }
  }

  // обновление и отрисовка летящих листьев
  for (int i = 0; i < WIND_LEAVES; i++) {
    if (!windLeaves[i].active) continue;
    windLeaves[i].wob += 0.3f;
    windLeaves[i].x += windLeaves[i].vx + sin(windLeaves[i].wob) * 0.2f;
    windLeaves[i].y += windLeaves[i].vy * 0.4f + 0.03f; // лёгкое проседание
    if (windLeaves[i].life > 4) windLeaves[i].life -= 4; else windLeaves[i].life = 0;

    int px = (int)windLeaves[i].x, py = (int)windLeaves[i].y;
    if (windLeaves[i].life == 0 || px < -2 || px > MATRIX_W + 2 || py < -2 || py > 7) {
      windLeaves[i].active = false;
      continue;
    }
    if (px >= 0 && px < MATRIX_W && py >= 0 && py < MATRIX_H) {
      int idx = XY(px, py);
      if (idx >= 0) leds[idx] = CRGB(60, 180, 40).nscale8_video(windLeaves[i].life);
    }
  }
}

// =====================================================================
//  РАДУГА — классическая дуга из 7 цветных полос
// =====================================================================
void rainbowEffect() {
  static float rainbowPhase = 0;
  rainbowPhase += 0.02f;

  // фон - посветлевшее после дождя небо
  for (int y = 0; y < MATRIX_H; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y); if (idx >= 0) leds[idx] = CRGB(15, 45, 85);
    }

  const CRGB bands[7] = {
    CRGB(255, 0, 0),   CRGB(255, 120, 0), CRGB(255, 220, 0), CRGB(0, 200, 0),
    CRGB(0, 120, 255), CRGB(60, 0, 200),  CRGB(160, 0, 200)
  };

  // центр дуги ниже видимой области — так получается именно дуга, а не круг
  float cx = 16.0f;
  float cy = 9.0f + sin(rainbowPhase) * 0.4f; // лёгкое дыхание дуги
  for (int y = 0; y <= 6; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      float dx = (x - cx) * 0.55f; // компенсация неквадратных пропорций матрицы
      float dy = (y - cy);
      float dist = sqrt(dx * dx + dy * dy);
      int band = (int)(dist - 2.5f);
      if (band >= 0 && band < 7) {
        int idx = XY(x, y);
        if (idx >= 0) leds[idx] = bands[band];
      }
    }
  }
  for (int x = 0; x < MATRIX_W; x++) { int idx = XY(x, 7); if (idx>=0) leds[idx] = CRGB(0, 140, 30); }
}

// =====================================================================
//  ЧАСЫ
// =====================================================================
void clockEffect() {
  time_t now = time(nullptr);
  if (now < 100000) {
    FastLED.clear();
    drawDigit(0, 2, 0, CRGB::Red);
    drawDigit(0, 8, 0, CRGB::Red);
    drawDigit(0, 16, 0, CRGB::Red);
    drawDigit(0, 22, 0, CRGB::Red);
    return;
  }
  struct tm* t = localtime(&now);
  FastLED.clear();
  uint8_t hueOffset = (millis() / 50) % 255;
  drawDigit(t->tm_hour/10, 2, 0, CHSV((hueOffset) % 255, 255, 200));
  drawDigit(t->tm_hour%10, 8, 0, CHSV((hueOffset + 64) % 255, 255, 200));
  if(t->tm_sec%2==0){ 
    int idx = XY(14,1); if(idx>=0) leds[idx] = CHSV((hueOffset + 128) % 255, 255, 200);
    idx = XY(14,3); if(idx>=0) leds[idx] = CHSV((hueOffset + 128) % 255, 255, 200);
    idx = XY(14,5); if(idx>=0) leds[idx] = CHSV((hueOffset + 128) % 255, 255, 200);
  }
  drawDigit(t->tm_min/10, 16, 0, CHSV((hueOffset + 192) % 255, 255, 200));
  drawDigit(t->tm_min%10, 22, 0, CHSV((hueOffset + 32) % 255, 255, 200));
}

// =====================================================================
//  ОГОНЬ (AlexGyver FireMatrixReal)
// =====================================================================
#define FIRE_WIDTH  MATRIX_W
#define FIRE_HEIGHT MATRIX_H
#define FIRE_COOLING  55
#define FIRE_SPARKING 120
#define FIRE_SPEED    30

byte fireHeat[FIRE_WIDTH][FIRE_HEIGHT];

void resetFireGyver() {
  memset(fireHeat, 0, sizeof(fireHeat));
}

void fireEffectGyver() {
  for (int x = 0; x < FIRE_WIDTH; x++) {
    for (int y = 0; y < FIRE_HEIGHT; y++) {
      int cooldown = random(0, ((FIRE_COOLING * 10) / FIRE_HEIGHT) + 2);
      int newHeat = fireHeat[x][y] - cooldown;
      if (newHeat < 0) newHeat = 0;
      fireHeat[x][y] = newHeat;
    }
  }

  for (int x = 0; x < FIRE_WIDTH; x++) {
    for (int y = 0; y <= FIRE_HEIGHT - 3; y++) {
      fireHeat[x][y] = (fireHeat[x][y + 1] + fireHeat[x][y + 2] + fireHeat[x][y + 2]) / 3;
    }
  }

  for (int x = 0; x < FIRE_WIDTH; x++) {
    if (random(0, 255) < FIRE_SPARKING) {
      fireHeat[x][FIRE_HEIGHT - 1] = random(160, 255);
      if (FIRE_HEIGHT > 1) fireHeat[x][FIRE_HEIGHT - 2] = random(160, 255);
    }
  }

  for (int x = 0; x < FIRE_WIDTH; x++) {
    for (int y = 0; y < FIRE_HEIGHT; y++) {
      int idx = XY(x, y);
      if (idx >= 0) {
        byte heat = fireHeat[x][y];
        if (heat > 200) {
          leds[idx] = CRGB(255, 255, 180);
        } else if (heat > 150) {
          leds[idx] = CRGB(255, 220, 80);
        } else if (heat > 100) {
          leds[idx] = CRGB(255, 150, 0);
        } else if (heat > 50) {
          leds[idx] = CRGB(220, 80, 0);
        } else if (heat > 20) {
          leds[idx] = CRGB(150, 30, 0);
        } else {
          leds[idx] = CRGB(0, 0, 0);
        }
      }
    }
  }
}

// =====================================================================
//  БЕГУЩАЯ СТРОКА
// =====================================================================
void resetText() {
  scrollPos = MATRIX_W;
  utf8ToIndices(displayText, textChars, 128, textLen);
}

void textScrollEffect() {
  FastLED.clear();
  int charWidth = 6;
  for (int i = 0; i < textLen; i++) {
    int xPos = scrollPos + i * charWidth;
    if (xPos > -5 && xPos < MATRIX_W) {
      if (textChars[i] == 255) { /* пробел */ }
      else drawLetter(textChars[i], xPos, 0, CRGB::White);
    }
  }
  scrollPos--;
  if (scrollPos < -(textLen * charWidth)) scrollPos = MATRIX_W;
}

// =====================================================================
//  ДНЕВНЫЕ ЭФФЕКТЫ
// =====================================================================

// --- 1. Летний полдень ---
void summerNoonEffect() {
  for (int y = 0; y < 6; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) {
        uint8_t r = 0;
        uint8_t g = (uint8_t)(120 + (y * 10));
        uint8_t b = (uint8_t)(220 + (y * 5));
        leds[idx] = CRGB(r, g, b);
      }
    }
  }
  drawSun(16, 1, 255, 255, 200);
  for (int y = 6; y < 8; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(0, 160, 40);
    }
  }
  drawTrees();
}

// --- 2. Утренний туман ---
void morningMistEffect() {
  for (int y = 0; y < 6; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) {
        uint8_t r = (uint8_t)(80 + (y * 15));
        uint8_t g = (uint8_t)(100 + (y * 10));
        uint8_t b = (uint8_t)(120 + (y * 8));
        leds[idx] = CRGB(r, g, b);
      }
    }
  }
  drawSun(16, 2, 200, 180, 150);
  for (int y = 6; y < 8; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(0, 100, 20);
    }
  }
  drawTrees();
}

// --- 3. Ветреный день ---
void windyDayEffect() {
  float oldSpeed = g_windSpeed;
  g_windSpeed = 8.0f;
  windEffect();
  g_windSpeed = oldSpeed;
}

// --- 4. Облачная погода ---
void cloudyDayEffect() {
  sunCloudyEffect();
}

// --- 5. Закатное небо ---
void sunsetSkyEffect() {
  sunsetDynamicEffect();
}

// --- 6. Звёздная ночь (базовый ночной эффект, используется runNightEffect) ---
void starryNightEffect() {
  nightEffectPhase();
}

// --- 7. Водопад с озером ---
#define WATERFALL_DROPS 14
float waterfallDropY[WATERFALL_DROPS];
int   waterfallDropX[WATERFALL_DROPS];
float waterfallDropSpeed[WATERFALL_DROPS];
bool waterfallInit = false;
uint8_t lakePhase = 0;

void resetWaterfall() {
  waterfallInit = false;
  lakePhase = 0;
}

void waterfallLakeEffect() {
  if (!waterfallInit) {
    for (int i = 0; i < WATERFALL_DROPS; i++) {
      waterfallDropX[i] = 21 + random(0, 4);
      waterfallDropY[i] = random(0, 6);
      waterfallDropSpeed[i] = random(35, 70) / 100.0;
    }
    waterfallInit = true;
  }
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(30, 90, 140);
    }
  for (int y = 0; y < 6; y++) {
    for (int x = 20; x <= 26; x++) {
      if (x < 21 || x > 24) {
        int idx = XY(x, y);
        if (idx >= 0) leds[idx] = CRGB(95, 90, 85);
      }
    }
  }
  for (int x = 19; x <= 27; x++) {
    int idx = XY(x, 0);
    if (idx >= 0) leds[idx] = CRGB(0, 130, 30);
  }
  for (int i = 0; i < WATERFALL_DROPS; i++) {
    int px = waterfallDropX[i];
    int py = (int)waterfallDropY[i];
    if (py >= 0 && py < 6) {
      int idx = XY(px, py);
      if (idx >= 0) leds[idx] = CRGB(190, 235, 255);
    }
    waterfallDropY[i] += waterfallDropSpeed[i];
    if (waterfallDropY[i] >= 6) {
      waterfallDropY[i] = 0;
      waterfallDropX[i] = 21 + random(0, 4);
      waterfallDropSpeed[i] = random(35, 70) / 100.0;
    }
  }
  for (int x = 21; x <= 24; x++) {
    int idx = XY(x, 5);
    if (idx >= 0) leds[idx] = CRGB(230, 245, 255);
  }
  lakePhase++;
  for (int x = 0; x < MATRIX_W; x++) {
    uint8_t shimmer = sin8(lakePhase * 3 + x * 12);
    uint8_t b = map(shimmer, 0, 255, 130, 255);
    int idx = XY(x, 6);
    if (idx >= 0) leds[idx] = CRGB(15, 90, b);
    idx = XY(x, 7);
    if (idx >= 0) leds[idx] = CRGB(8, 55, (uint8_t)(b * 0.7f));
  }
}

// --- 8. Цветочный луг ---
#define MEADOW_FLOWERS 18
struct MeadowFlower { uint8_t x; uint8_t y; uint8_t hue; uint8_t phase; };
MeadowFlower meadowFlowers[MEADOW_FLOWERS];
bool meadowInit = false;

void resetMeadow() {
  meadowInit = false;
}

void flowerMeadowEffect() {
  if (!meadowInit) {
    for (int i = 0; i < MEADOW_FLOWERS; i++) {
      meadowFlowers[i].x = random(0, MATRIX_W);
      meadowFlowers[i].y = random(5, 8);
      meadowFlowers[i].hue = random(0, 255);
      meadowFlowers[i].phase = random(0, 255);
    }
    meadowInit = true;
  }
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(90, 190, 255);
    }
  drawSun(6, 1, 255, 240, 130);
  for (int y = 4; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(20, (uint8_t)(110 + (y - 4) * 15), 20);
    }
  for (int i = 0; i < MEADOW_FLOWERS; i++) {
    uint8_t b = sin8(meadowFlowers[i].phase);
    CRGB c = CHSV(meadowFlowers[i].hue, 220, 190);
    c.nscale8_video(map(b, 0, 255, 150, 255));
    int idx = XY(meadowFlowers[i].x, meadowFlowers[i].y);
    if (idx >= 0) leds[idx] = c;
    meadowFlowers[i].phase += 2;
  }
}

// --- 9. Млечный путь (ночной суб-эффект) ---
uint8_t milkyWayPhase = 0;
void resetMilkyWay() { milkyWayPhase = 0; }

void milkyWayEffect() {
  milkyWayPhase += 2;
  for (int y = 0; y < MATRIX_H; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      float d = fabs((x * 0.28f) - y + 2.0f);
      uint8_t haze = (uint8_t)constrain(130.0f - d * 45.0f, 0.0f, 130.0f);
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(haze / 3, haze / 3, (uint8_t)(haze / 2 + haze / 4));
    }
  }
  for (int i = 0; i < 36; i++) {
    int x = (i * 7 + (milkyWayPhase / 3)) % MATRIX_W;
    int y = (i * 5) % MATRIX_H;
    float d = fabs((x * 0.28f) - y + 2.0f);
    if (d < 2.2f) {
      uint8_t br = sin8(milkyWayPhase + i * 17);
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(230, 230, 255).nscale8_video(br);
    }
  }
}

// --- 10. Квазар (ночной суб-эффект) ---
float quasarPhase = 0;
void resetQuasar() { quasarPhase = 0; }

void quasarEffect() {
  FastLED.clear();
  quasarPhase += 0.15f;
  int cx = 16, cy = 4;
  for (int y = 0; y < MATRIX_H; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      float dx = x - cx, dy = (y - cy) * 2.0f;
      float dist = sqrt(dx * dx + dy * dy);
      float pulse = 1.0f + 0.3f * sin(quasarPhase);
      float glow = constrain(10.0f * pulse - dist, 0.0f, 10.0f);
      if (glow > 0.1f) {
        uint8_t b = (uint8_t)(glow * 22);
        int idx = XY(x, y);
        if (idx >= 0) leds[idx] = CRGB(b / 3, b / 2, b);
      }
    }
  }
  uint8_t coreB = (uint8_t)(200 + 55 * sin(quasarPhase * 2));
  int idx = XY(cx, cy);
  if (idx >= 0) leds[idx] = CRGB(coreB, coreB, 255);
  for (int j = 1; j < 16; j++) {
    int jx1 = cx - j, jx2 = cx + j;
    if (random(0, 100) < 45) {
      if (jx1 >= 0) { int idx1 = XY(jx1, cy); if (idx1 >= 0) leds[idx1] = CRGB(160, 190, 255).nscale8_video((uint8_t)constrain(255 - j * 16, 0, 255)); }
      if (jx2 < MATRIX_W) { int idx2 = XY(jx2, cy); if (idx2 >= 0) leds[idx2] = CRGB(160, 190, 255).nscale8_video((uint8_t)constrain(255 - j * 16, 0, 255)); }
    }
  }
}

// --- 11. НОВОЕ: Бабочки в саду (дневной эффект) ---
#define BUTTERFLIES 6
struct Butterfly { float x, y, vx, vy; uint8_t hue, wingPhase; };
Butterfly butterflies[BUTTERFLIES];
bool butterflyInit = false;

void resetButterflyGarden() { butterflyInit = false; }

void butterflyGardenEffect() {
  if (!butterflyInit) {
    for (int i = 0; i < BUTTERFLIES; i++) {
      butterflies[i].x = random(0, MATRIX_W);
      butterflies[i].y = random(0, 5);
      butterflies[i].vx = (random(0,2)==0 ? 1 : -1) * (0.1f + random(0,10) / 100.0f);
      butterflies[i].vy = (random(0,2)==0 ? 1 : -1) * (0.05f + random(0,10) / 100.0f);
      butterflies[i].hue = random(0, 255);
      butterflies[i].wingPhase = random(0, 255);
    }
    butterflyInit = true;
  }
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < MATRIX_W; x++) { int idx = XY(x, y); if (idx >= 0) leds[idx] = CRGB(120, 200, 255); }
  drawSun(28, 1, 255, 240, 150);
  for (int y = 5; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) { int idx = XY(x, y); if (idx >= 0) leds[idx] = CRGB(20, (uint8_t)(120 + (y - 5) * 20), 20); }
  for (int i = 0; i < BUTTERFLIES; i++) {
    butterflies[i].x += butterflies[i].vx;
    butterflies[i].y += butterflies[i].vy;
    if (butterflies[i].x < 0 || butterflies[i].x >= MATRIX_W) butterflies[i].vx *= -1;
    if (butterflies[i].y < 0 || butterflies[i].y >= 5) butterflies[i].vy *= -1;
    butterflies[i].wingPhase += 12;
    uint8_t br = map(sin8(butterflies[i].wingPhase), 0, 255, 120, 255);
    CRGB c = CHSV(butterflies[i].hue, 230, br);
    int idx = XY((int)butterflies[i].x, (int)butterflies[i].y);
    if (idx >= 0) leds[idx] = c;
  }
}

// --- 12. НОВОЕ: Воздушный шар (дневной эффект) ---
float balloonX = -6;
uint8_t balloonHue = 30;
void resetHotAirBalloon() { balloonX = -6; balloonHue = random(0, 255); }

void hotAirBalloonEffect() {
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < MATRIX_W; x++) { int idx = XY(x, y); if (idx >= 0) leds[idx] = CRGB(90, 180, 250); }
  for (int y = 6; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) { int idx = XY(x, y); if (idx >= 0) leds[idx] = CRGB(10, 130, 30); }

  int bx = (int)balloonX;
  CRGB balloonColor = CHSV(balloonHue, 220, 220);
  for (int dy = 0; dy < 3; dy++)
    for (int dx = 0; dx < 3; dx++) {
      int idx = XY(bx + dx, 1 + dy); if (idx >= 0) leds[idx] = balloonColor;
    }
  int idx = XY(bx + 1, 4); if (idx >= 0) leds[idx] = CRGB(120, 80, 30);
  balloonX += 0.1f;
  if (balloonX > MATRIX_W + 4) { balloonX = -6; balloonHue = random(0, 255); }
}

// --- 13. НОВОЕ: Северное сияние (ночной суб-эффект) ---
uint16_t auroraPhase = 0;
void resetAurora() { auroraPhase = 0; resetStars(); }

void auroraEffect() {
  auroraPhase += 1;
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < MATRIX_W; x++) { int idx = XY(x, y); if (idx >= 0) leds[idx] = CRGB(3, 5, 20); }
  for (int x = 0; x < MATRIX_W; x++) {
    float wave1 = sin((x * 0.35f) + auroraPhase * 0.05f) * 2.0f;
    float wave2 = sin((x * 0.2f) + auroraPhase * 0.03f + 2.0f) * 1.5f;
    int y1 = (int)(2 + wave1);
    int y2 = (int)(3 + wave2);
    uint8_t hue1 = 96 + (uint8_t)(sin(x * 0.1f + auroraPhase * 0.02f) * 20);
    if (y1 >= 0 && y1 < 6) { int idx = XY(x, y1); if (idx >= 0) leds[idx] = CHSV(hue1, 200, 150); }
    if (y2 >= 0 && y2 < 6) { int idx = XY(x, y2); if (idx >= 0) leds[idx] = CHSV(hue1 + 40, 200, 100); }
  }
  updateStars();
  drawStars();
  for (int y = 6; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) { int idx = XY(x, y); if (idx >= 0) leds[idx] = CRGB(0, 30, 10); }
}

// --- 14. НОВОЕ: Светлячки (ночной суб-эффект) ---
#define FIREFLIES 10
struct Firefly { float x, y; uint8_t phase, speed; };
Firefly fireflies[FIREFLIES];
bool fireflyInit = false;
void resetFireflies() { fireflyInit = false; }

void fireflyEffect() {
  if (!fireflyInit) {
    for (int i = 0; i < FIREFLIES; i++) {
      fireflies[i].x = random(0, MATRIX_W);
      fireflies[i].y = random(0, 7);
      fireflies[i].phase = random(0, 255);
      fireflies[i].speed = random(2, 6);
    }
    fireflyInit = true;
  }
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) { int idx = XY(x, y); if (idx >= 0) leds[idx] = CRGB(2, 4, 8); }
  for (int i = 0; i < FIREFLIES; i++) {
    fireflies[i].x += sin(fireflies[i].phase * 0.05f) * 0.15f;
    fireflies[i].y += cos(fireflies[i].phase * 0.03f) * 0.1f;
    if (fireflies[i].x < 0) fireflies[i].x = MATRIX_W - 1;
    if (fireflies[i].x >= MATRIX_W) fireflies[i].x = 0;
    if (fireflies[i].y < 0) fireflies[i].y = 0;
    if (fireflies[i].y >= 8) fireflies[i].y = 7;
    fireflies[i].phase += fireflies[i].speed;
    uint8_t br = sin8(fireflies[i].phase);
    if (br > 60) {
      CRGB c = CRGB(180, 255, 60).nscale8_video(br);
      int idx = XY((int)fireflies[i].x, (int)fireflies[i].y);
      if (idx >= 0) leds[idx] = c;
    }
  }
}

// --- Смена ночных суб-эффектов: звёзды / млечный путь / квазар / сияние / светлячки ---
enum NightSubEffect { NIGHT_STARS, NIGHT_MILKYWAY, NIGHT_QUASAR, NIGHT_AURORA, NIGHT_FIREFLIES };
NightSubEffect currentNightEffect = NIGHT_STARS;
unsigned long lastNightEffectChange = 0;

void chooseNextNightEffect() {
  int r = random(0, 100);
  if (r < 40) currentNightEffect = NIGHT_STARS;
  else if (r < 60) currentNightEffect = NIGHT_MILKYWAY;
  else if (r < 75) currentNightEffect = NIGHT_QUASAR;
  else if (r < 92) currentNightEffect = NIGHT_AURORA;
  else currentNightEffect = NIGHT_FIREFLIES;

  switch (currentNightEffect) {
    case NIGHT_MILKYWAY:  resetMilkyWay();  break;
    case NIGHT_QUASAR:    resetQuasar();    break;
    case NIGHT_AURORA:    resetAurora();    break;
    case NIGHT_FIREFLIES: resetFireflies(); break;
    default:              resetStars();     break;
  }
}

void resetNightEffects() {
  currentNightEffect = NIGHT_STARS;
  lastNightEffectChange = millis();
  resetStars();
}

void runNightEffect() {
  if (millis() - lastNightEffectChange > NIGHT_EFFECT_CHANGE_INTERVAL) {
    lastNightEffectChange = millis();
    chooseNextNightEffect();
  }
  switch (currentNightEffect) {
    case NIGHT_MILKYWAY:  milkyWayEffect();  break;
    case NIGHT_QUASAR:    quasarEffect();    break;
    case NIGHT_AURORA:    auroraEffect();    break;
    case NIGHT_FIREFLIES: fireflyEffect();   break;
    default:               nightEffectPhase(); break;
  }
}

// =====================================================================
//  ДИНАМИЧЕСКАЯ СМЕНА ДНЕВНЫХ ЭФФЕКТОВ
//  (сюда же включён ночной цикл — так дневные и ночные сцены периодически
//   сменяют друг друга)
// =====================================================================
typedef void (*DayEffectFunc)();
DayEffectFunc dayEffects[] = {
  summerNoonEffect,
  morningMistEffect,
  windyDayEffect,
  cloudyDayEffect,
  sunsetSkyEffect,
  waterfallLakeEffect,
  flowerMeadowEffect,
  butterflyGardenEffect,
  hotAirBalloonEffect,
  runNightEffect
};

typedef void (*DayResetFunc)();
DayResetFunc dayResets[] = {
  NULL, NULL, resetWind, resetSunCloudy, NULL,
  resetWaterfall, resetMeadow, resetButterflyGarden, resetHotAirBalloon,
  resetNightEffects
};

int dayEffectCount = sizeof(dayEffects) / sizeof(dayEffects[0]);
int currentDayEffect = 0;
unsigned long lastDayEffectChange = 0;

void resetDayEffects() {
  currentDayEffect = 0;
  lastDayEffectChange = millis();
  if (dayResets[currentDayEffect]) dayResets[currentDayEffect]();
}

void runDayEffect() {
  if (millis() - lastDayEffectChange > DAY_EFFECT_CHANGE_INTERVAL) {
    lastDayEffectChange = millis();
    currentDayEffect = (currentDayEffect + 1) % dayEffectCount;
    if (dayResets[currentDayEffect]) dayResets[currentDayEffect]();
  }
  dayEffects[currentDayEffect]();
}

// =====================================================================
//  МАССИВЫ ЭФФЕКТОВ (используются режимом ДЕМО)
// =====================================================================
typedef void (*EffectFunc)();
typedef void (*ResetFunc)();

EffectFunc demoEffects[] = {
  dawnDynamicEffect, sunsetDynamicEffect, sunsetDynamicEffect, sunEffect2, sunCloudyEffect,
  runNightEffect, rainEffect1, snowEffect1, rainEffect2, fireEffectGyver, rainbowEffect, windEffect,
  waterfallLakeEffect, flowerMeadowEffect, butterflyGardenEffect, hotAirBalloonEffect
};
ResetFunc demoResets[] = {
  NULL, NULL, NULL, resetSun2, resetSunCloudy, resetNightEffects,
  resetRain1, resetSnow1, resetRain2, resetFireGyver, NULL, resetWind,
  resetWaterfall, resetMeadow, resetButterflyGarden, resetHotAirBalloon
};
int demoEffectsCount = sizeof(demoEffects) / sizeof(demoEffects[0]);

void checkAlarm() {
  if (!alarmActive) { alarmTriggered = false; return; }
  time_t now = time(nullptr);
  if (now < 100000) return;
  struct tm* t = localtime(&now);
  int currentMinutes = t->tm_hour * 60 + t->tm_min;
  int alarmMinutes   = alarmHour * 60 + alarmMinute;

  if (!alarmTriggered &&
      currentMinutes >= alarmMinutes &&
      currentMinutes <  alarmMinutes + 1) {
    alarmTriggered = true;
  }
  if (alarmTriggered) {
    if (globalBrightness < 200) globalBrightness += 1;
    if (currentMinutes >= alarmMinutes + 5) {
      alarmTriggered = false;
      alarmActive = false;
      globalBrightness = BRIGHTNESS;
    }
  }
}

void handleDemoMode() {
  unsigned long currentTime = millis();
  if (demoEffectStart == 0 || currentTime - demoEffectStart > demoEffectDuration) {
    demoEffectIndex = random(0, demoEffectsCount);
    demoEffectDuration = (10UL * 60UL * 1000UL) + random(60UL * 1000UL, 5UL * 60UL * 1000UL);
    demoEffectStart = currentTime;
    if (demoResets[demoEffectIndex]) demoResets[demoEffectIndex]();
  }
  demoEffects[demoEffectIndex]();
}
// =====================================================================
//  ОБЩИЙ СПИСОК ВСЕХ ЭФФЕКТОВ — используется веб-интерфейсом,
//  чтобы любой эффект можно было выбрать напрямую
// =====================================================================
typedef void (*GenericEffectFunc)();
typedef void (*GenericResetFunc)();

GenericEffectFunc allEffects[] = {
  rainEffect1, rainEffect2, rainEffect3, rainEffect4,
  snowEffect1, snowEffect2,
  sunEffect2, sunCloudyEffect,
  sunsetDynamicEffect, dawnDynamicEffect,
  rainbowEffect, clockEffect, fireEffectGyver, textScrollEffect, windEffect,
  waterfallLakeEffect, flowerMeadowEffect,
  summerNoonEffect, morningMistEffect,
  butterflyGardenEffect, hotAirBalloonEffect,
  nightEffectPhase, milkyWayEffect, quasarEffect, auroraEffect, fireflyEffect
};
GenericResetFunc allEffectResets[] = {
  resetRain1, resetRain2, resetRain3, resetRain4,
  resetSnow1, resetSnow2,
  resetSun2, resetSunCloudy,
  NULL, NULL,
  NULL, NULL, resetFireGyver, resetText, resetWind,
  resetWaterfall, resetMeadow,
  NULL, NULL,
  resetButterflyGarden, resetHotAirBalloon,
  resetStars, resetMilkyWay, resetQuasar, resetAurora, resetFireflies
};
const char* allEffectNames[] = {
  "Дождь слабый", "Дождь с грозой", "Морось", "Ливень сильный",
  "Снег лёгкий", "Снег сильный",
  "Солнце с облаками", "Пасмурно",
  "Закат", "Рассвет",
  "Радуга", "Часы", "Огонь", "Бегущая строка", "Ветер (дерево)",
  "Водопад и озеро", "Цветочный луг",
  "Летний полдень", "Утренний туман",
  "Бабочки в саду", "Воздушный шар",
  "Звёздная ночь", "Млечный путь", "Квазар", "Северное сияние", "Светлячки"
};
int allEffectsCount = sizeof(allEffects) / sizeof(allEffects[0]);
int lastSelectedEffect = -1;

void runCurrentMode() {
  if (!powerOn) { FastLED.clear(); FastLED.show(); return; }

  FastLED.setBrightness(globalBrightness);

  switch (currentMode) {
    case MODE_DEMO:    handleDemoMode();      break;
    case MODE_CLOCK:   clockEffect();         break;
    case MODE_FIRE:    fireEffectGyver();     break;
    case MODE_TEXT:    textScrollEffect();    break;
    case MODE_NORMAL:  runWeatherEffect();    break;
    case MODE_ALARM:   dawnDynamicEffect();   break;
    case MODE_SUNRISE: dawnDynamicEffect();   break;
    case MODE_SUNSET:  sunsetDynamicEffect(); break;
    case MODE_RAIN:    rainEffect1();         break;
    case MODE_SNOW:    snowEffect1();         break;
    case MODE_NIGHT:   runNightEffect();      break;
    case MODE_MOON:    nightEffectPhase();    break;
    case MODE_RAINBOW: rainbowEffect();       break;
    case MODE_EFFECT:
      if (allEffectsCount > 0) {
        selectedEffectIndex = constrain(selectedEffectIndex, 0, allEffectsCount - 1);
        allEffects[selectedEffectIndex]();
      }
      break;
  }
  FastLED.show();
}

EffectFunc effects[] = {
  rainEffect1, rainEffect2, rainEffect3, rainEffect4,
  snowEffect1, snowEffect2,
  sunEffect2, sunCloudyEffect,
  runNightEffect,
  sunsetDynamicEffect, sunsetDynamicEffect, dawnDynamicEffect,
  rainbowEffect, clockEffect,
  fireEffectGyver, textScrollEffect,
  windEffect
};
ResetFunc resets[] = {
  resetRain1, resetRain2, resetRain3, resetRain4,
  resetSnow1, resetSnow2,
  resetSun2, resetSunCloudy,
  resetNightEffects,
  NULL, NULL, NULL,
  NULL, NULL,
  resetFireGyver, resetText,
  resetWind
};
enum WeatherEffect {
  EFF_RAIN1, EFF_RAIN2, EFF_RAIN3, EFF_RAIN4,
  EFF_SNOW1, EFF_SNOW2,
  EFF_SUN2, EFF_SUN_CLOUDY,
  EFF_NIGHT,
  EFF_SUNSET_DYNAMIC, EFF_SUNSET_LATE, EFF_DAWN_LATE,
  EFF_RAINBOW, EFF_CLOCK,
  EFF_FIRE, EFF_TEXT,
  EFF_WIND
};
WeatherEffect currentEffect = EFF_SUN2;
int lastTemp = -100;
bool showTemp = false;
unsigned long lastTempShow = 0;

void runWeatherEffect() {
  if (showTemp && lastTemp != -100) {
    CRGB color;
    if      (lastTemp < -10) color = CRGB::Blue;
    else if (lastTemp < 0)   color = CRGB(0,100,255);
    else if (lastTemp < 10)  color = CRGB::Cyan;
    else if (lastTemp < 20)  color = CRGB::Green;
    else if (lastTemp < 25)  color = CRGB::Yellow;
    else if (lastTemp < 30)  color = CRGB::Orange;
    else                     color = CRGB::Red;
    drawNumber(lastTemp, color);
    if (millis() - lastTempShow > TEMP_DISPLAY_TIME) showTemp = false;
    return;
  }
  if (currentEffect == EFF_SUN2) {
    runDayEffect();
  } else {
    effects[currentEffect]();
  }
}

// ========== WiFi и погода ==========
void fetchWeatherAndPickEffect() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();

  String cityEncoded = OWM_CITY;
  cityEncoded.replace(",", "%2C");
  cityEncoded.replace(" ", "%20");
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + cityEncoded +
               "&appid=" + String(OWM_API_KEY) + "&units=" + String(OWM_UNITS) + "&lang=ru";

  http.setTimeout(3000);
  if (!http.begin(client, url)) { Serial.println("HTTP begin failed"); return; }

  ESP.wdtDisable();
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<4096> doc;
    if (!deserializeJson(doc, payload)) {
      int   weatherId  = doc["weather"][0]["id"] | 800;
      float temp       = doc["main"]["temp"]     | 0.0f;
      float windSpeed  = doc["wind"]["speed"]    | 0.0f;
      long  sunrise    = doc["sys"]["sunrise"]   | 0;
      long  sunset     = doc["sys"]["sunset"]    | 0;
      long  currentTime= doc["dt"]               | 0;
      uint8_t clouds   = doc["clouds"]["all"]    | 0;
      float rain1h     = doc["rain"]["1h"]       | 0.0f;
      float snow1h     = doc["snow"]["1h"]       | 0.0f;

      g_windSpeed  = windSpeed;
      g_sunrise    = sunrise;
      g_sunset     = sunset;
      g_clouds     = clouds;
      g_rainVolume = rain1h;
      g_snowVolume = snow1h;
      g_weatherId  = weatherId;
      lastTemp     = round(temp);

      bool isNight    = (currentTime < sunrise) || (currentTime > sunset);
      bool nearSunrise= abs((long)(currentTime - sunrise)) < 1800;
      bool nearSunset = abs((long)(currentTime - sunset))  < 1800;

      moonPhase = getMoonPhase(time(nullptr));

      if      (nearSunrise)                              currentEffect = EFF_DAWN_LATE;
      else if (nearSunset)                               currentEffect = EFF_SUNSET_DYNAMIC;
      else if (isNight)                                  currentEffect = EFF_NIGHT;
      else if (weatherId >= 200 && weatherId <= 232)     currentEffect = EFF_RAIN2;
      else if (weatherId >= 300 && weatherId <= 321)     currentEffect = EFF_RAIN3;
      else if (weatherId >= 500 && weatherId <= 501)     currentEffect = EFF_RAIN1;
      else if (weatherId >= 502 && weatherId <= 504)     currentEffect = EFF_RAIN4;
      else if (weatherId == 511)                         currentEffect = EFF_RAIN1;
      else if (weatherId >= 520 && weatherId <= 531)     currentEffect = EFF_RAIN4;
      else if (weatherId >= 600 && weatherId <= 601)     currentEffect = EFF_SNOW1;
      else if (weatherId >= 602 && weatherId <= 622)     currentEffect = EFF_SNOW2;
      else if (weatherId == 800) {
        if (windSpeed >= WIND_SPEED_THRESHOLD) currentEffect = EFF_WIND;
        else                                   currentEffect = EFF_SUN2;
      }
      else if (weatherId == 801 || weatherId == 802) {
        if (windSpeed >= WIND_SPEED_THRESHOLD) currentEffect = EFF_WIND;
        else                                   currentEffect = EFF_SUN_CLOUDY;
      }
      else if (weatherId >= 803) {
        if (windSpeed >= WIND_SPEED_THRESHOLD) currentEffect = EFF_WIND;
        else                                   currentEffect = EFF_SUN_CLOUDY;
      }
      else currentEffect = EFF_SUN2;

      if ((int)currentEffect < (int)(sizeof(resets)/sizeof(resets[0])) && resets[currentEffect])
        resets[currentEffect]();

      showTemp = true;
      lastTempShow = millis();
    }
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
    time_t now = time(nullptr);
    if (now > 100000) {
      struct tm *t = localtime(&now);
      if (t->tm_hour >= 21 || t->tm_hour < 6) {
        currentEffect = EFF_NIGHT;
        if (resets[currentEffect]) resets[currentEffect]();
      }
      moonPhase = getMoonPhase(now);
    }
  }

  http.end();
  ESP.wdtEnable(8000);
}

void handleWiFiConnection() {
  switch (wifiState) {
    case WIFI_IDLE:
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      wifiState = WIFI_CONNECTING;
      ntpStartTime = millis();
      Serial.println("WiFi connecting...");
      break;
    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected! Starting NTP sync...");
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "ntp1.ntp-servers.net", "ntp2.vniiftri.ru", "pool.ntp.org");
        ntpStartTime = millis();
        wifiState = WIFI_GET_TIME;
      } else if (millis() - ntpStartTime > WIFI_TIMEOUT) {
        Serial.println("WiFi timeout, retrying...");
        wifiState = WIFI_IDLE;
      }
      break;
    case WIFI_GET_TIME:
      if (time(nullptr) > 100000) {
        Serial.println("Time synchronized.");
        wifiState = WIFI_READY;
      } else if (millis() - ntpStartTime > 30000) {
        Serial.println("NTP timeout, retrying...");
        wifiState = WIFI_IDLE;
      }
      break;
    case WIFI_READY:
      break;
  }
}


// ========== Веб-интерфейс ==========
void startWebInterface() {
  if (!webInterfaceEnabled) {
    Serial.println("Starting Web Interface...");
    WiFi.mode(WIFI_STA);
    server.begin();
    webInterfaceStartTime = millis();
    webInterfaceEnabled = true;
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

void stopWebInterface() {
  if (webInterfaceEnabled) {
    Serial.println("Stopping Web Interface...");
    server.stop();
    webInterfaceEnabled = false;
  }
}

void handleRoot() {
  String effectOptions = "";
  for (int i = 0; i < allEffectsCount; i++) {
    effectOptions += "<option value='" + String(i) + "'";
    if (currentMode == MODE_EFFECT && i == selectedEffectIndex) effectOptions += " selected";
    effectOptions += ">" + String(allEffectNames[i]) + "</option>";
  }

  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>LED Matrix Control</title>
<style>
  body { font-family: sans-serif; background: #222; color: #eee; text-align: center; }
  button, select, input { font-size: 1.2em; margin: 5px; padding: 10px; }
</style>
</head>
<body>
  <h1>Управление LED матрицей</h1>
  <p>Режим: <span id="modeDisplay">)rawliteral" + String(currentMode) + R"rawliteral(</span></p>
  <select id="modeSelect" onchange="setMode(this.value)">
    <option value="0">Обычный (погода)</option><option value="1">Демо</option>
    <option value="2">Часы</option><option value="3">Огонь</option>
    <option value="4">Будильник</option><option value="5">Текст</option>
    <option value="6">Рассвет</option><option value="7">Закат</option>
    <option value="8">Дождь</option><option value="9">Снег</option>
    <option value="10">Ночь</option><option value="11">Луна</option><option value="12">Радуга</option>
  </select>
  <p>Все эффекты (список ниже переключает режим "Выбранный эффект"):</p>
  <select id="effectSelect" onchange="setEffect(this.value)">
)rawliteral";
  page += effectOptions;
  page += R"rawliteral(
  </select>
  <p>Яркость: <input type="range" min="0" max="255" id="brightness" oninput="setBrightness(this.value)" value=")rawliteral" + String(globalBrightness) + R"rawliteral("></p>
  <p>Текст: <input type="text" id="text" maxlength="20" placeholder="Введите текст"><button onclick="setText()">Установить</button></p>
  <p>Будильник: <input type="time" id="alarm"><button onclick="setAlarm()">Установить</button>
     <button onclick="toggleAlarm()">Вкл/Выкл</button></p>
  <script>
    function getUrl(){ return window.location.origin; }
    function setMode(v){ fetch(getUrl()+'/setmode?mode='+v); }
    function setEffect(v){ fetch(getUrl()+'/seteffect?id='+v); }
    function setBrightness(v){ fetch(getUrl()+'/setbrightness?b='+v); }
    function setText(){ let t=document.getElementById('text').value; fetch(getUrl()+'/settext?text='+encodeURIComponent(t)); }
    function setAlarm(){ let t=document.getElementById('alarm').value.split(':'); fetch(getUrl()+'/setalarm?h='+t[0]+'&m='+t[1]); }
    function toggleAlarm(){ fetch(getUrl()+'/togglealarm'); }
    function offInterface(){ fetch(getUrl()+'/off'); }
  </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", page);
}

void handleSetMode() {
  if (server.hasArg("mode")) {
    int m = server.arg("mode").toInt();
    currentMode = (Mode)constrain(m, 0, 13);
  }
  server.send(200);
}

void handleSetEffect() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    id = constrain(id, 0, allEffectsCount - 1);
    if (id != lastSelectedEffect) {
      if (allEffectResets[id]) allEffectResets[id]();
      lastSelectedEffect = id;
    }
    selectedEffectIndex = id;
    currentMode = MODE_EFFECT;
  }
  server.send(200);
}

void handleSetBrightness() {
  if (server.hasArg("b")) {
    globalBrightness = server.arg("b").toInt();
    if (globalBrightness < 0) globalBrightness = 0;
    if (globalBrightness > 255) globalBrightness = 255;
  }
  server.send(200);
}

void handleSetText() {
  if (server.hasArg("text")) {
    displayText = server.arg("text");
    resetText();
  }
  server.send(200);
}

void handleSetAlarm() {
  if (server.hasArg("h") && server.hasArg("m")) {
    alarmHour = server.arg("h").toInt();
    alarmMinute = server.arg("m").toInt();
  }
  server.send(200);
}

void handleToggleAlarm() {
  alarmActive = !alarmActive;
  server.send(200);
}

void handleOff() {
  stopWebInterface();
  server.send(200, "text/plain", "Web interface stopped");
}

// ========== СЕНСОРНАЯ КНОПКА ==========
void handleTouchButton() {
  bool currentTouch = (digitalRead(TOUCH_PIN) == HIGH);

  static bool lastStable = false;
  static unsigned long stableTime = 0;
  bool stable = currentTouch;
  if (currentTouch != lastStable) {
    stableTime = millis();
  }
  if (millis() - stableTime < 20) {
    stable = lastStable;
  }
  lastStable = currentTouch;
  currentTouch = stable;

  switch (btnState) {
    case BTN_IDLE:
      if (currentTouch) {
        btnState = BTN_PRESS;
        btnPressStart = millis();
        btnLongPressHandled = false;
      }
      break;
    case BTN_PRESS:
      if (!currentTouch) {
        if (!btnLongPressHandled && (millis() - btnPressStart > DEBOUNCE_TIME)) {
          if (millis() - btnLastClick < DOUBLE_CLICK_TIME) btnClickCount++;
          else btnClickCount = 1;
          btnLastClick = millis();
          btnState = BTN_RELEASE;
        } else btnState = BTN_IDLE;
      } else if (millis() - btnPressStart > LONG_PRESS_TIME) {
        btnState = BTN_LONG_PRESS;
        btnLongPressHandled = true;
      }
      break;
    case BTN_LONG_PRESS:
      if (!currentTouch) btnState = BTN_IDLE;
      else {
        static unsigned long lastBrightChange = 0;
        if (millis() - lastBrightChange > 100) {
          lastBrightChange = millis();
          globalBrightness += BRIGHTNESS_STEP;
          if (globalBrightness > 255) globalBrightness = 255;
        }
      }
      break;
    case BTN_RELEASE:
      if (millis() - btnLastClick > DOUBLE_CLICK_TIME) {
        if (btnClickCount == 1) {
          powerOn = !powerOn;
        } else if (btnClickCount == 2) {
          switch (currentMode) {
            case MODE_CLOCK: currentMode = MODE_FIRE; resetFireGyver(); break;
            case MODE_FIRE: currentMode = MODE_DEMO; demoEffectStart = 0; break;
            case MODE_DEMO: currentMode = MODE_NORMAL; break;
            case MODE_NORMAL: currentMode = MODE_CLOCK; break;
            default: currentMode = MODE_CLOCK;
          }
        } else if (btnClickCount >= 3) {
          currentMode = MODE_DEMO;
          demoEffectStart = 0;
        }
        btnClickCount = 0;
        btnState = BTN_IDLE;
      }
      break;
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  randomSeed(analogRead(A0));

  pinMode(TOUCH_PIN, INPUT);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(globalBrightness); // старт с минимальной яркости
  FastLED.clear(); FastLED.show();

  server.on("/", handleRoot);
  server.on("/setmode", handleSetMode);
  server.on("/seteffect", handleSetEffect);
  server.on("/setbrightness", handleSetBrightness);
  server.on("/settext", handleSetText);
  server.on("/setalarm", handleSetAlarm);
  server.on("/togglealarm", handleToggleAlarm);
  server.on("/off", handleOff);

  resetText();
  resetFireGyver();
  resetWind();
  resetDayEffects();
  wifiState = WIFI_IDLE;
  weatherUpdateRequested = true;
  Serial.println("Ready.");
}

// ========== LOOP ==========
void loop() {
  ESP.wdtFeed();

  if (wifiState != WIFI_READY && (weatherUpdateRequested || !initialSyncDone)) {
    handleWiFiConnection();
  }

  if (wifiState == WIFI_READY && weatherUpdateRequested) {
    fetchWeatherAndPickEffect();
    lastWeatherCheck = millis();
    weatherUpdateRequested = false;
    initialSyncDone = true;
    if (!webInterfaceEnabled) {
      startWebInterface();
    }
  }

  if (webInterfaceEnabled) {
    server.handleClient();
    if (millis() - webInterfaceStartTime > WEB_INTERFACE_DURATION) {
      stopWebInterface();
      weatherUpdateRequested = true;
      if (WiFi.status() != WL_CONNECTED) {
        wifiState = WIFI_IDLE;
      }
    }
  }

  handleTouchButton();
  checkAlarm();

  if (millis() - lastFrame >= FRAME_DELAY) {
    lastFrame = millis();
    runCurrentMode();
  }
  yield();
  delay(1);
}
