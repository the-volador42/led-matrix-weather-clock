// Небо: звёзды, луна, ночная сцена, закат и рассвет.
#include "effects.h"

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

// Звезда улетела падающей — освобождаем её место в реестре объектов.
// Без этого реестр за ~25 минут забивался «призраками», новые звёзды
// переставали появляться и небо пустело.
static void forgetStarObject(const Star& s) {
  int ix = (int)s.x, iy = (int)s.y;
  for (int i = 0; i < MAX_SCENE_OBJECTS; i++) {
    const SceneObject& o = sceneObjects[i];
    if (o.used && o.x0 == ix && o.y0 == iy && o.x1 == ix && o.y1 == iy) {
      unregisterObject(i);
      return;
    }
  }
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
          forgetStarObject(stars[idx]);
          stars[idx] = stars[--numStars];
        }
      } else {
        if (numStars > 0 && !shooting.active) {
          int idx = random(0, numStars);
          shooting.active = true;
          shooting.x = stars[idx].x; shooting.y = stars[idx].y;
          shooting.vx = -1.2; shooting.vy = 1.2;
          shooting.trailLen = 4;
          forgetStarObject(stars[idx]);
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
    if (px>=0 && px<MATRIX_W && py>=0 && py<5) setPixel(px,py, color);
    stars[i].phase += stars[i].speed;
  }
  if (shooting.active) {
    float sx = shooting.x, sy = shooting.y;
    for (int t = 0; t < shooting.trailLen; t++) {
      int px = (int)(sx - t*shooting.vx*0.7), py = (int)(sy - t*shooting.vy*0.7);
      if (px>=0 && px<MATRIX_W && py>=0 && py<5) setPixel(px,py, CRGB(255,230,150).nscale8_video(map(t,0,shooting.trailLen-1,255,40)));
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
        if (moonCrescent[y][x]) setPixel(x0 + x, y0 + y, CRGB(40, 40, 40));
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
        if (mask[y][x]) setPixel(x0 + x, y0 + y, color);
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
      setPixel(x,y, c);
    }
  }
  drawMoonByPhase(23, 0, moonPhase);
  for (int y=6; y<8; y++)
    for (int x=0; x<32; x++) setPixel(x,y, seasonGround(x,y, CRGB(0, 40, 10)));
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
  if (g_alarmDawnProgress >= 0.0f) return g_alarmDawnProgress;  // рассвет будильника
  if (g_sunrise < 100000) return (millis() % 60000) / 60000.0f;
  long now = (long)time(nullptr);
  long window = 3600;
  long start = g_sunrise - window;
  if (now <= start) return 0.0f;
  if (now >= g_sunrise) return 1.0f;
  return (float)(now - start) / (float)window;
}



void sunsetDynamicEffect() {
  float p = getSunsetProgress();
  FastLED.clear();

  for (int y = 0; y < 6; y++) {
    float py = (float)y / 5.0f;
    uint8_t r = (uint8_t)(20  + p * 210 * (1.0f - py));
    uint8_t g = (uint8_t)(100 - p * 70  * (1.0f - py));
    uint8_t b = (uint8_t)(200 - p * 160 * (1.0f - py));
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(r, g, b));
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
        leds[idx] = seasonGround(x, y, CRGB(gr, gg, 8));
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
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(r, g, b));
  }
  int sunY = 5 - (int)(p * 4.0f);
  drawSun(16, sunY, 255, 220, (uint8_t)(60 + p * 60));
  for (int y = 6; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, seasonGround(x, y, CRGB(0, (uint8_t)(90 + p * 50), 20)));
  drawTrees();
}
