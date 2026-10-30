// =====================================================================
//  МАТРИЦА 32x8: светодиоды, координаты, шрифты
// =====================================================================
#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

extern CRGB leds[NUM_LEDS];

// Номер светодиода в ленте по координатам (x — столбец, y — строка сверху).
// Лента идёт «змейкой» по столбцам. За пределами матрицы — -1.
int XY(int x, int y);

// Зажечь пиксель. Координаты за пределами матрицы просто игнорируются,
// поэтому проверять их перед вызовом не нужно.
inline void setPixel(int x, int y, const CRGB& color) {
  int idx = XY(x, y);
  if (idx >= 0) leds[idx] = color;
}

// Цифры 5x7
void drawDigit(int digit, int startX, int startY, CRGB color);
void drawNumber(int num, CRGB color);   // очищает экран и пишет число по центру

// Текст 5x7: русские и латинские буквы (строчные рисуются заглавными),
// цифры и знаки . , : - ! ? + / % ( ) ' " = °.  Ширина знака 6 точек.
int  mapUnicodeToIndex(uint16_t unicode);                  // номер знака или -1
void utf8ToIndices(String input, uint8_t* outBuf, int maxLen, int& outLen);
void drawLetter(int index, int startX, int startY, CRGB color);
void drawGlyphLine(const uint8_t* glyphs, int len, int x0, int y0, CRGB color);
