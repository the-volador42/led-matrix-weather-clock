// Ночные сцены: млечный путь, квазар, северное сияние, светлячки.
#include "effects.h"

// --- 9. Млечный путь (ночной суб-эффект) ---
uint8_t milkyWayPhase = 0;
void resetMilkyWay() { milkyWayPhase = 0; }

void milkyWayEffect() {
  milkyWayPhase += 2;
  for (int y = 0; y < MATRIX_H; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      float d = fabs((x * 0.28f) - y + 2.0f);
      uint8_t haze = (uint8_t)constrain(130.0f - d * 45.0f, 0.0f, 130.0f);
      setPixel(x, y, CRGB(haze / 3, haze / 3, (uint8_t)(haze / 2 + haze / 4)));
    }
  }
  for (int i = 0; i < 36; i++) {
    int x = (i * 7 + (milkyWayPhase / 3)) % MATRIX_W;
    int y = (i * 5) % MATRIX_H;
    float d = fabs((x * 0.28f) - y + 2.0f);
    if (d < 2.2f) {
      uint8_t br = sin8(milkyWayPhase + i * 17);
      setPixel(x, y, CRGB(230, 230, 255).nscale8_video(br));
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
        setPixel(x, y, CRGB(b / 3, b / 2, b));
      }
    }
  }
  uint8_t coreB = (uint8_t)(200 + 55 * sin(quasarPhase * 2));
  setPixel(cx, cy, CRGB(coreB, coreB, 255));
  for (int j = 1; j < 16; j++) {
    int jx1 = cx - j, jx2 = cx + j;
    if (random(0, 100) < 45) {
      if (jx1 >= 0) setPixel(jx1, cy, CRGB(160, 190, 255).nscale8_video((uint8_t)constrain(255 - j * 16, 0, 255)));
      if (jx2 < MATRIX_W) setPixel(jx2, cy, CRGB(160, 190, 255).nscale8_video((uint8_t)constrain(255 - j * 16, 0, 255)));
    }
  }
}


// --- 13. НОВОЕ: Северное сияние (ночной суб-эффект) ---
uint16_t auroraPhase = 0;
void resetAurora() { auroraPhase = 0; resetStars(); }

void auroraEffect() {
  auroraPhase += 1;
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(3, 5, 20));
  for (int x = 0; x < MATRIX_W; x++) {
    float wave1 = sin((x * 0.35f) + auroraPhase * 0.05f) * 2.0f;
    float wave2 = sin((x * 0.2f) + auroraPhase * 0.03f + 2.0f) * 1.5f;
    int y1 = (int)(2 + wave1);
    int y2 = (int)(3 + wave2);
    // оттенок 76..116 (зелёный); сначала int, потом uint8_t — иначе
    // отрицательный синус превращается в непредсказуемое число
    uint8_t hue1 = (uint8_t)(96 + (int)(sin(x * 0.1f + auroraPhase * 0.02f) * 20));
    if (y1 >= 0 && y1 < 6) setPixel(x, y1, CHSV(hue1, 200, 150));
    if (y2 >= 0 && y2 < 6) setPixel(x, y2, CHSV(hue1 + 40, 200, 100));
  }
  updateStars();
  drawStars();
  for (int y = 6; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, seasonGround(x, y, CRGB(0, 30, 10)));
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
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(2, 4, 8));
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
      setPixel((int)fireflies[i].x, (int)fireflies[i].y, c);
    }
  }
}
