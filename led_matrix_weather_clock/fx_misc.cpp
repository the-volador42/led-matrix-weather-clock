// Радуга, часы, огонь, бегущая строка.
#include "effects.h"

// =====================================================================
//  РАДУГА — классическая дуга из 7 цветных полос
// =====================================================================
void rainbowEffect() {
  static float rainbowPhase = 0;
  rainbowPhase += 0.02f;

  // фон - посветлевшее после дождя небо
  for (int y = 0; y < MATRIX_H; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(15, 45, 85));

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
      if (band >= 0 && band < 7) setPixel(x, y, bands[band]);
    }
  }
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 7, seasonGround(x, 7, CRGB(0, 140, 30)));
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
    setPixel(14,1, CHSV((hueOffset + 128) % 255, 255, 200));
    setPixel(14,3, CHSV((hueOffset + 128) % 255, 255, 200));
    setPixel(14,5, CHSV((hueOffset + 128) % 255, 255, 200));
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
