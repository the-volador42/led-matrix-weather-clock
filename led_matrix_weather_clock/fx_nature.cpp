// Дневные сцены: полдень, туман, водопад, луг, бабочки, воздушный шар.
#include "effects.h"

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
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, seasonGround(x, y, CRGB(0, 160, 40)));
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
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, seasonGround(x, y, CRGB(0, 100, 20)));
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
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(30, 90, 140));
  for (int y = 0; y < 6; y++) {
    for (int x = 20; x <= 26; x++) {
      if (x < 21 || x > 24) setPixel(x, y, CRGB(95, 90, 85));
    }
  }
  for (int x = 19; x <= 27; x++) setPixel(x, 0, seasonGround(x, 0, CRGB(0, 130, 30)));
  for (int i = 0; i < WATERFALL_DROPS; i++) {
    int px = waterfallDropX[i];
    int py = (int)waterfallDropY[i];
    if (py >= 0 && py < 6) setPixel(px, py, CRGB(190, 235, 255));
    waterfallDropY[i] += waterfallDropSpeed[i];
    if (waterfallDropY[i] >= 6) {
      waterfallDropY[i] = 0;
      waterfallDropX[i] = 21 + random(0, 4);
      waterfallDropSpeed[i] = random(35, 70) / 100.0;
    }
  }
  for (int x = 21; x <= 24; x++) setPixel(x, 5, CRGB(230, 245, 255));
  lakePhase++;
  for (int x = 0; x < MATRIX_W; x++) {
    uint8_t shimmer = sin8(lakePhase * 3 + x * 12);
    uint8_t b = map(shimmer, 0, 255, 130, 255);
    setPixel(x, 6, CRGB(15, 90, b));
    setPixel(x, 7, CRGB(8, 55, (uint8_t)(b * 0.7f)));
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
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(90, 190, 255));
  drawSun(6, 1, 255, 240, 130);
  for (int y = 4; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, seasonGround(x, y, CRGB(20, (uint8_t)(110 + (y - 4) * 15), 20)));
  for (int i = 0; i < MEADOW_FLOWERS; i++) {
    uint8_t b = sin8(meadowFlowers[i].phase);
    CRGB c = CHSV(meadowFlowers[i].hue, 220, 190);
    c.nscale8_video(map(b, 0, 255, 150, 255));
    if (currentSeason != SEASON_WINTER) setPixel(meadowFlowers[i].x, meadowFlowers[i].y, c);   // зимой цветов нет
    meadowFlowers[i].phase += 2;
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
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(120, 200, 255));
  drawSun(28, 1, 255, 240, 150);
  for (int y = 5; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, seasonGround(x, y, CRGB(20, (uint8_t)(120 + (y - 5) * 20), 20)));
  for (int i = 0; i < BUTTERFLIES; i++) {
    butterflies[i].x += butterflies[i].vx;
    butterflies[i].y += butterflies[i].vy;
    if (butterflies[i].x < 0 || butterflies[i].x >= MATRIX_W) butterflies[i].vx *= -1;
    if (butterflies[i].y < 0 || butterflies[i].y >= 5) butterflies[i].vy *= -1;
    butterflies[i].wingPhase += 12;
    uint8_t br = map(sin8(butterflies[i].wingPhase), 0, 255, 120, 255);
    CRGB c = CHSV(butterflies[i].hue, 230, br);
    setPixel((int)butterflies[i].x, (int)butterflies[i].y, c);
  }
}

// --- 12. НОВОЕ: Воздушный шар (дневной эффект) ---
float balloonX = -6;
uint8_t balloonHue = 30;
void resetHotAirBalloon() { balloonX = -6; balloonHue = random(0, 255); }

void hotAirBalloonEffect() {
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(90, 180, 250));
  for (int y = 6; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, seasonGround(x, y, CRGB(10, 130, 30)));

  int bx = (int)balloonX;
  CRGB balloonColor = CHSV(balloonHue, 220, 220);
  for (int dy = 0; dy < 3; dy++)
    for (int dx = 0; dx < 3; dx++) setPixel(bx + dx, 1 + dy, balloonColor);
  setPixel(bx + 1, 4, CRGB(120, 80, 30));
  balloonX += 0.1f;
  if (balloonX > MATRIX_W + 4) { balloonX = -6; balloonHue = random(0, 255); }
}
