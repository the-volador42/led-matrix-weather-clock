/*
 * effects.ino
 *
 * САМОСТОЯТЕЛЬНЫЙ ТЕСТОВЫЙ СКЕТЧ ЭФФЕКТОВ
 * -----------------------------------------
 * Этот файл специально сделан автономным: его можно открыть как отдельный
 * Arduino-скетч и сразу проверить все эффекты через Serial Monitor.
 *
 * Serial Monitor: 115200 baud
 *   1..26  - выбрать эффект
 *   0      - выключить ручной эффект
 *   ?      - показать список
 *
 * Матрица: 32x8, ESP8266 / Wemos D1 mini, WS2812/WS2812B
 */

#include <Arduino.h>
#include <FastLED.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define LED_PIN       5
#define MATRIX_W      32
#define MATRIX_H      8
#define NUM_LEDS      (MATRIX_W * MATRIX_H)
#define BRIGHTNESS    30
#define FRAME_DELAY   40
#define LIGHTNING_DURATION 500
#define LIGHTNING_INTERVAL 3000
#define SNOW_GEN_INTERVAL 10000

CRGB leds[NUM_LEDS];

// Типы данных для эффекта звёзд. Должны быть объявлены ДО любых функций,
// потому что Arduino IDE автоматически создаёт прототипы функций.
struct Star {
  float x, y;
  uint8_t phase, speed;
};

struct ShootingStar {
  bool active;
  float x, y;
  float vx, vy;
  int trailLen;
};

// =====================================================================
//  ВРЕМЯ ГОДА (сезон)
// =====================================================================
// Единый параметр сезона, который передаётся в функцию отрисовки дерева.
// Сейчас цвет листвы для всех сезонов одинаковый (зелёный) — это
// временная заглушка. Позже сюда добавим реальные сезонные цвета
// (весна/лето — зелёный, осень — жёлто-оранжевый, зима — голые ветки
// или снег на кроне) и способ вычислять текущий сезон по дате.
enum Season : uint8_t {
  SEASON_SPRING = 0,
  SEASON_SUMMER = 1,
  SEASON_AUTUMN = 2,
  SEASON_WINTER = 3
};

Season currentSeason = SEASON_SUMMER; // TODO: вычислять по реальной дате

CRGB getLeafColor(Season season) {
  switch (season) {
    case SEASON_SPRING: return CRGB(0, 150, 0); // TODO: светлая молодая листва
    case SEASON_SUMMER: return CRGB(0, 150, 0);
    case SEASON_AUTUMN: return CRGB(0, 150, 0); // TODO: жёлто-оранжевый
    case SEASON_WINTER: return CRGB(0, 150, 0); // TODO: голые ветки / иней
  }
  return CRGB(0, 150, 0);
}

// =====================================================================
//  ОБЪЕКТЫ И ЭФФЕКТЫ СЦЕНЫ
// =====================================================================
// Договорённость о терминах:
//  - "Объект" — твёрдая сущность сцены (дерево, домик, звезда). Объекты
//    не могут появляться вплотную друг к другу — между ними всегда
//    остаётся зазор (margin).
//  - "Эффект" — фантомная частица (дождевая капля, снежинка, лист,
//    трава, вода и т.п.). Эффекты друг друга не толкают и свободно
//    перекрываются с объектами и друг с другом.
//
// Ниже — простой реестр твёрдых объектов с проверкой "можно ли тут
// поставить ещё один объект, не касаясь соседей".
#define MAX_SCENE_OBJECTS 200
struct SceneObject {
  bool used;
  int x0, y0, x1, y1; // ограничивающий прямоугольник (включительно)
  uint8_t margin;     // минимальный зазор до других объектов
};
SceneObject sceneObjects[MAX_SCENE_OBJECTS];

void clearSceneObjects() {
  for (int i = 0; i < MAX_SCENE_OBJECTS; i++) sceneObjects[i].used = false;
}

bool boxesTooClose(int ax0, int ay0, int ax1, int ay1,
                    int bx0, int by0, int bx1, int by1, int margin) {
  // Расширяем бокс b на margin клеток во все стороны и проверяем пересечение.
  return !(ax1 < bx0 - margin || ax0 > bx1 + margin ||
           ay1 < by0 - margin || ay0 > by1 + margin);
}

// Можно ли поставить твёрдый объект с данным боксом, не касаясь вплотную
// уже существующих объектов сцены?
bool canPlaceObject(int x0, int y0, int x1, int y1, int margin = 1) {
  for (int i = 0; i < MAX_SCENE_OBJECTS; i++) {
    if (!sceneObjects[i].used) continue;
    if (boxesTooClose(x0, y0, x1, y1,
                       sceneObjects[i].x0, sceneObjects[i].y0,
                       sceneObjects[i].x1, sceneObjects[i].y1, margin))
      return false;
  }
  return true;
}

int registerObject(int x0, int y0, int x1, int y1, uint8_t margin = 1) {
  for (int i = 0; i < MAX_SCENE_OBJECTS; i++) {
    if (!sceneObjects[i].used) {
      sceneObjects[i].used = true;
      sceneObjects[i].x0 = x0; sceneObjects[i].y0 = y0;
      sceneObjects[i].x1 = x1; sceneObjects[i].y1 = y1;
      sceneObjects[i].margin = margin;
      return i;
    }
  }
  return -1;
}

void unregisterObject(int id) {
  if (id >= 0 && id < MAX_SCENE_OBJECTS) sceneObjects[id].used = false;
}

// Минимальный набор "внешних" данных, который в исходном проекте давался
// погодным/Wi-Fi частью. Для автономного теста задаём безопасные значения.
String displayText = "ПРИВЕТ";
long g_sunrise = 0;
long g_sunset  = 0;
float g_windSpeed = 8.0f;
float moonPhase = 0.5f;
int selectedEffectIndex = 0;
int lastSelectedEffect = -1;
bool effectActive = false;
uint8_t textChars[128];
int textLen = 0;
int scrollPos = MATRIX_W;

// Небольшая защита от случайного изменения режима: число выбирается
// только после Enter, поэтому 10/11/12... не срабатывают по отдельным цифрам.

// ========== XY ==========
int XY(int x, int y) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return -1;
  return (x % 2 == 0) ? x * 8 + y : x * 8 + (7 - y);
}

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
//
//  Параметр season определяет цвет листвы (сейчас везде зелёный —
//  заглушка на будущее, когда добавим реальную сезонность).
//  ВАЖНО: для эффекта ветра (windEffect) дерево рисуется ОТДЕЛЬНОЙ
//  функцией drawWindTree() — там крона не статична, а "живая" и умеет
//  менять форму, поэтому она не может использовать этот же неподвижный
//  шаблон кроны.
// =====================================================================
void drawTree(int baseX, int bend, CRGB trunkColor, Season season = currentSeason) {
  // baseX — левый край кроны (крона занимает 6 колонок по ширине)
  CRGB leafColor = getLeafColor(season);
  int t1 = baseX + 2, t2 = baseX + 3;
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

void drawTrees(Season season = currentSeason) {
  drawTree(0, 0, CRGB(100, 60, 20), season);
  drawTree(26, 0, CRGB(100, 60, 20), season);
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

// Кустики для эффекта 7 (Солнце).
// При каждом новом запуске эффекта их становится 1-3,
// а позиции выбираются заново.
#define SUN_BUSHES_MAX 3
int sunBushCount = 1;
int sunBushX[SUN_BUSHES_MAX] = {4, 15, 26};
byte sunBushHeight[SUN_BUSHES_MAX] = {2, 2, 2};

void resetSun2() {
  for (int i=0;i<NUM_CLOUDS_SUN;i++){
    sunCloudPhaseX[i]=random(0,628)/100.0;
    sunCloudPhaseY[i]=random(0,628)/100.0;
  }

  // Новый набор кустиков при каждом вызове resetSun2().
  sunBushCount = random(1, SUN_BUSHES_MAX + 1);
  for (int i = 0; i < SUN_BUSHES_MAX; i++) {
    sunBushX[i] = random(1, MATRIX_W - 2);
    sunBushHeight[i] = random(1, 3);
  }
}

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

  // Кустики стоят у нижнего края, поверх травы.
  // Форма маленькая, чтобы на матрице 32x8 они читались именно как кусты.
  for (int b = 0; b < sunBushCount; b++) {
    int x = sunBushX[b];
    int h = sunBushHeight[b];

    // Нижняя часть куста.
    for (int dx = -1; dx <= 1; dx++) {
      int px = x + dx;
      int py = 7;
      if (px >= 0 && px < MATRIX_W) {
        int idx = XY(px, py);
        if (idx >= 0) leds[idx] = CRGB(0, 95, 20);
      }
    }

    // Верхушка и небольшие боковые ветки.
    if (h >= 1) {
      int idx = XY(x, 6);
      if (idx >= 0) leds[idx] = CRGB(0, 125, 25);
    }
    if (h >= 2) {
      if (x > 0) {
        int idx = XY(x - 1, 6);
        if (idx >= 0) leds[idx] = CRGB(0, 110, 22);
      }
      if (x < MATRIX_W - 1) {
        int idx = XY(x + 1, 6);
        if (idx >= 0) leds[idx] = CRGB(0, 110, 22);
      }
      int idx = XY(x, 5);
      if (idx >= 0) leds[idx] = CRGB(0, 145, 30);
    }
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

// Звезда — тоже "объект" в терминах общей системы объектов сцены:
// новая звезда не может появиться вплотную к домику, дереву или другой
// уже существующей звезде. Проверка идёт через общий canPlaceObject().
bool tryPlaceStar(Star &s) {
  for (int attempt = 0; attempt < 100; attempt++) {
    float tx = random(0, MATRIX_W);
    float ty = random(0, 5);
    int ix = (int)tx, iy = (int)ty;
    if (canPlaceObject(ix, iy, ix, iy, 1)) {
      s.x = tx; s.y = ty; s.phase = random(0,255); s.speed = random(1,4);
      return true;
    }
  }
  return false;
}

void resetStars() {
  numStars = 0;
  clearSceneObjects();
  // Домик и дерево — твёрдые объекты сцены, звёзды не должны появляться
  // вплотную к ним.
  registerObject(24, 3, 28, 7, 1); // домик
  registerObject(0, 1, 5, 7, 1);   // левое дерево (то, что рисуется ночью)
  int desired = random(3,17);
  for (int i=0;i<desired;i++) {
    if (numStars >= MAX_STARS) break;
    Star newStar;
    if (tryPlaceStar(newStar)) {
      stars[numStars++] = newStar;
      registerObject((int)newStar.x, (int)newStar.y, (int)newStar.x, (int)newStar.y, 1);
    }
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
          if (tryPlaceStar(newStar)) {
            stars[numStars++] = newStar;
            registerObject((int)newStar.x, (int)newStar.y, (int)newStar.x, (int)newStar.y, 1);
          }
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
  drawTree(0, 0, CRGB(100, 60, 20), currentSeason);
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

// =====================================================================
//  ВЕТЕР: живая крона + система листьев
// =====================================================================
// Отличие от остальных деревьев: здесь крона — не жёсткий монолитный
// шаблон, а "живой организм". Она поделена на несколько кластеров
// точек; каждый кластер гуляет к своей случайной цели независимо от
// остальных, поэтому крона не просто едет как единое целое, а реально
// меняет форму кадр за кадром (то вытягивается, то сжимается, то
// расходится по-разному в разные стороны).
//
// Отдельно — система листьев (эффект, не объект): одновременно на
// экране не больше MAX_WIND_LEAVES штук. Лист рождается подхваченным
// порывом ветра, летит, может осесть на земле, и в любом случае
// пропадает, как только уходит за левый край матрицы.

float windPhase = 0;
float windCrownX = 0.0f;
float windCrownY = 0.0f;
float windTargetX = 0.0f;
float windTargetY = 0.0f;
unsigned long nextCrownMove = 0;

#define WIND_CROWN_CLUSTERS 3
struct CrownCluster { float curX, curY, targetX, targetY; unsigned long nextMove; };
CrownCluster windClusters[WIND_CROWN_CLUSTERS];

// Точки кроны в координатах относительно baseX (та же форма, что и у
// обычного дерева), но каждая точка привязана к своему кластеру —
// именно разное поведение кластеров и делает крону "живой".
struct CrownPoint { int8_t dx, dy; uint8_t cluster; };
const CrownPoint windCrownShape[] = {
  {0,4,0},{1,4,0},{2,4,1},{3,4,1},{4,4,2},{5,4,2},
  {1,3,0},{2,3,1},{3,3,1},{4,3,2},
  {2,2,1},{3,2,1},
  {2,1,1},{3,1,1}
};
#define WIND_CROWN_POINTS (sizeof(windCrownShape) / sizeof(windCrownShape[0]))

#define MAX_WIND_LEAVES 5
enum LeafState { LEAF_NONE, LEAF_FLYING, LEAF_GROUNDED };
struct WindLeaf {
  LeafState state;
  float x, y, vx, vy, wob, wobSpeed, gravity;
  uint8_t life;
};
WindLeaf windLeaves[MAX_WIND_LEAVES];
unsigned long nextLeafSpawn = 0;

void resetWind() {
  windPhase = random(0, 628) / 100.0f;
  windCrownX = 0.0f;
  windCrownY = 0.0f;
  windTargetX = 0.0f;
  windTargetY = 0.0f;
  nextCrownMove = millis() + random(180, 800);
  nextLeafSpawn = millis() + random(120, 900);
  for (int i = 0; i < WIND_CROWN_CLUSTERS; i++) {
    windClusters[i].curX = 0; windClusters[i].curY = 0;
    windClusters[i].targetX = 0; windClusters[i].targetY = 0;
    windClusters[i].nextMove = millis() + random(150, 700);
  }
  for (int i = 0; i < MAX_WIND_LEAVES; i++) windLeaves[i].state = LEAF_NONE;
}

int activeWindLeaves() {
  int n = 0;
  for (int i = 0; i < MAX_WIND_LEAVES; i++) if (windLeaves[i].state != LEAF_NONE) n++;
  return n;
}

// Порыв ветра подхватывает новый лист с кроны. Если на экране уже
// MAX_WIND_LEAVES листьев — новый не появится, подождём, пока
// какой-то из старых не улетит за край или не истлеет.
void spawnWindLeaf(int fromX, int fromY) {
  if (activeWindLeaves() >= MAX_WIND_LEAVES) return;
  for (int i = 0; i < MAX_WIND_LEAVES; i++) {
    if (windLeaves[i].state == LEAF_NONE) {
      windLeaves[i].state = LEAF_FLYING;
      windLeaves[i].x = fromX + random(-2, 3) + (random(0, 100) / 100.0f);
      windLeaves[i].y = fromY + random(0, 3) + (random(0, 100) / 100.0f);
      float dir = (random(0, 2) == 0) ? -1.0f : 1.0f;
      windLeaves[i].vx = dir * (0.10f + random(0, 90) / 100.0f);
      windLeaves[i].vy = -0.60f + random(0, 121) / 100.0f;
      windLeaves[i].wob = random(0, 628) / 100.0f;
      windLeaves[i].wobSpeed = 0.08f + random(0, 22) / 100.0f;
      windLeaves[i].gravity = 0.01f + random(0, 8) / 100.0f;
      windLeaves[i].life = 190 + random(0, 66);
      return;
    }
  }
}

void drawWindTree(int baseX, CRGB trunkColor, Season season) {
  // Ствол неподвижен — "дышит" и деформируется только крона.
  int t1 = baseX + 2, t2 = baseX + 3;
  for (int y = 5; y < 8; y++) {
    int idx1 = XY(t1, y); if (idx1 >= 0) leds[idx1] = trunkColor;
    int idx2 = XY(t2, y); if (idx2 >= 0) leds[idx2] = trunkColor;
  }

  CRGB leafColor = getLeafColor(season);
  int crownX = (int)round(windCrownX);
  int crownY = (int)round(windCrownY);

  for (uint8_t p = 0; p < WIND_CROWN_POINTS; p++) {
    const CrownPoint &pt = windCrownShape[p];
    CrownCluster &cl = windClusters[pt.cluster];
    int px = baseX + pt.dx + crownX + (int)round(cl.curX);
    int py = pt.dy + crownY + (int)round(cl.curY);
    int idx = XY(px, py);
    if (idx >= 0) leds[idx] = leafColor;
  }
}

void windEffect() {
  float windStrength = constrain(g_windSpeed, 0.0f, 20.0f);
  windPhase += 0.03f + windStrength * 0.008f;

  // Общая цель кроны — куда клонится дерево целиком.
  if (millis() >= nextCrownMove) {
    windTargetX = (float)random(-2, 2);
    windTargetY = (float)random(-1, 2);
    nextCrownMove = millis() + random(220, 900);
  }
  windCrownX += (windTargetX - windCrownX) * (0.045f + windStrength * 0.003f);
  windCrownY += (windTargetY - windCrownY) * (0.040f + windStrength * 0.002f);

  // Кластеры кроны гуляют независимо друг от друга — крона не едет
  // единым куском, а "дышит": меняет форму кадр за кадром.
  for (int i = 0; i < WIND_CROWN_CLUSTERS; i++) {
    if (millis() >= windClusters[i].nextMove) {
      windClusters[i].targetX = (float)random(-1, 2) * (0.5f + windStrength * 0.05f);
      windClusters[i].targetY = (float)random(-1, 2) * 0.4f;
      windClusters[i].nextMove = millis() + random(150, 500);
    }
    windClusters[i].curX += (windClusters[i].targetX - windClusters[i].curX) * 0.15f;
    windClusters[i].curY += (windClusters[i].targetY - windClusters[i].curY) * 0.15f;
  }

  // небо
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      if (idx >= 0) leds[idx] = CRGB(20, 30, 55);
    }

  // Потоки ветра — у каждой полосы своя скорость, фаза и высота.
  int streaks = 6 + (int)constrain(windStrength, 0.0f, 10.0f);
  for (int i = 0; i < streaks; i++) {
    float t = windPhase * (1.0f + i * 0.11f) + i * 2.73f;
    int x = ((int)(t * (5.0f + (i % 4))) + i * 7) % (MATRIX_W + 12) - 6;
    int y = 1 + ((i * 3 + (int)(windPhase * 2.0f)) % 4);
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

  // Только одно дерево справа, крона — "живая" (см. drawWindTree).
  drawWindTree(26, CRGB(100, 60, 20), currentSeason);

  // Спонтанные вылеты листьев — не больше MAX_WIND_LEAVES одновременно
  // (spawnWindLeaf сам это проверяет).
  if (millis() >= nextLeafSpawn) {
    nextLeafSpawn = millis() + random(120, (int)constrain(1200 - windStrength * 45.0f, 300.0f, 1200.0f));
    int crownX = (int)round(windCrownX);
    int crownY = (int)round(windCrownY);
    int fx = 28 + crownX + random(-1, 2);
    int fy = 2 + crownY + random(0, 3);
    spawnWindLeaf(fx, fy);
  }

  // Обновление и отрисовка летящих/лежащих листьев.
  CRGB leafColor = getLeafColor(currentSeason);
  for (int i = 0; i < MAX_WIND_LEAVES; i++) {
    if (windLeaves[i].state == LEAF_NONE) continue;

    if (windLeaves[i].state == LEAF_FLYING) {
      windLeaves[i].wob += windLeaves[i].wobSpeed;
      windLeaves[i].vx += sin(windLeaves[i].wob) * 0.010f;
      windLeaves[i].vy += windLeaves[i].gravity;
      windLeaves[i].x += windLeaves[i].vx;
      windLeaves[i].y += windLeaves[i].vy;

      if (windLeaves[i].y >= 7.0f) {
        // Долетел до земли — оседает и постепенно гаснет.
        windLeaves[i].y = 7.0f;
        windLeaves[i].vy = 0;
        windLeaves[i].vx *= 0.25f;
        windLeaves[i].state = LEAF_GROUNDED;
      }
      if (windLeaves[i].life > 5) windLeaves[i].life -= 5; else windLeaves[i].life = 0;
    } else { // LEAF_GROUNDED
      windLeaves[i].x += windLeaves[i].vx;
      windLeaves[i].vx *= 0.9f;
      if (windLeaves[i].life > 2) windLeaves[i].life -= 2; else windLeaves[i].life = 0;
    }

    // Лист исчезает, как только уходит за левый край матрицы (или гаснет).
    if (windLeaves[i].x < 0.0f || windLeaves[i].life == 0 ||
        windLeaves[i].x > MATRIX_W + 2 || windLeaves[i].y < -2) {
      windLeaves[i].state = LEAF_NONE;
      continue;
    }

    int px = (int)round(windLeaves[i].x);
    int py = (int)round(windLeaves[i].y);
    if (px >= 0 && px < MATRIX_W && py >= 0 && py < MATRIX_H) {
      int idx = XY(px, py);
      if (idx >= 0) {
        CRGB particle = leafColor;
        particle.nscale8_video(windLeaves[i].life);
        leds[idx] = particle;
      }
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



// =====================================================================
//  ТЕРМИНАЛ: ПЕРЕКЛЮЧЕНИЕ ЭФФЕКТОВ ПО НОМЕРУ
// =====================================================================

typedef void (*EffectFunc)();
typedef void (*ResetFunc)();

EffectFunc allEffects[] = {
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

ResetFunc allEffectResets[] = {
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

static String effectsSerialBuffer;

void printEffectsMenu() {
  Serial.println();
  Serial.println(F("=== LED EFFECTS ==="));
  Serial.println(F("1..26  - выбрать эффект"));
  Serial.println(F("0      - выключить ручной эффект"));
  Serial.println(F("?      - показать список"));
  Serial.println();
  for (int i = 0; i < allEffectsCount; i++) {
    Serial.print(i + 1);
    Serial.print(F(" - "));
    Serial.println(allEffectNames[i]);
  }
  Serial.println();
}

void selectEffectFromTerminal(int userNumber) {
  if (userNumber == 0) {
    effectActive = false;
    FastLED.clear();
    FastLED.show();
    lastSelectedEffect = -1;
    Serial.println(F("[effects] -> эффект выключен"));
    return;
  }

  int id = userNumber - 1;
  if (id < 0 || id >= allEffectsCount) {
    Serial.print(F("[effects] неизвестный номер: "));
    Serial.println(userNumber);
    Serial.println(F("Введите число 0..26 или ?"));
    return;
  }

  if (id != lastSelectedEffect) {
    FastLED.clear();
    if (allEffectResets[id]) allEffectResets[id]();
    lastSelectedEffect = id;
  }

  selectedEffectIndex = id;
  effectActive = true;

  Serial.print(F("[effects] "));
  Serial.print(id + 1);
  Serial.print(F(" -> "));
  Serial.println(allEffectNames[id]);
}

void handleEffectsSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '?' ) {
      printEffectsMenu();
      effectsSerialBuffer = "";
      continue;
    }

    if (c == '\n' || c == '\r') {
      if (effectsSerialBuffer.length() > 0) {
        int value = effectsSerialBuffer.toInt();
        selectEffectFromTerminal(value);
        effectsSerialBuffer = "";
      }
      continue;
    }

    // Принимаем только цифры. Прочие символы в строке игнорируем.
    if (c >= '0' && c <= '9') {
      if (effectsSerialBuffer.length() < 3) {
        effectsSerialBuffer += c;
      }
    }
  }
}



// =====================================================================
//  АВТОНОМНЫЙ ЗАПУСК
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  randomSeed(ESP.getChipId() ^ micros());

  resetText();
  resetStars();

  Serial.println();
  Serial.println(F("======================================"));
  Serial.println(F(" LED MATRIX EFFECTS TEST"));
  Serial.println(F(" ESP8266 / 32x8 / 115200 baud"));
  Serial.println(F("======================================"));
  printEffectsMenu();
}

void loop() {
  handleEffectsSerial();

  if (!effectActive) {
    delay(5);
    return;
  }

  FastLED.setBrightness(BRIGHTNESS);
  allEffects[selectedEffectIndex]();
  FastLED.show();
  delay(FRAME_DELAY);
}
