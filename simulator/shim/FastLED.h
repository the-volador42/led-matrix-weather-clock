// Заглушка FastLED для симулятора на ПК.
// Математика цветов (scale8, sin8, CHSV -> CRGB) повторяет FastLED,
// чтобы цвета в симуляторе совпадали с матрицей.
#pragma once
#include "Arduino.h"

// ---- 8-битная математика FastLED ----
inline uint8_t scale8(uint8_t i, uint8_t scale) { return (uint8_t)(((uint16_t)i * (1 + (uint16_t)scale)) >> 8); }
inline uint8_t scale8_video(uint8_t i, uint8_t scale) {
  return (uint8_t)((((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0));
}
inline uint8_t qadd8(uint8_t a, uint8_t b) { int t = a + b; return t > 255 ? 255 : (uint8_t)t; }
inline uint8_t qsub8(uint8_t a, uint8_t b) { int t = a - b; return t < 0 ? 0 : (uint8_t)t; }

inline uint8_t sin8(uint8_t theta) {
  static const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};
  uint8_t offset = theta;
  if (theta & 0x40) offset = (uint8_t)255 - offset;
  offset &= 0x3F;
  uint8_t secoffset = offset & 0x0F;
  if (theta & 0x40) ++secoffset;
  uint8_t section = offset >> 4;
  const uint8_t* p = b_m16_interleave + section * 2;
  uint8_t b = p[0], m16 = p[1];
  uint8_t mx = (m16 * secoffset) >> 4;
  int8_t y = (int8_t)(mx + b);
  if (theta & 0x80) y = -y;
  y += 128;
  return (uint8_t)y;
}
inline uint8_t cos8(uint8_t theta) { return sin8(theta + 64); }

// ---- Цвета ----
struct CHSV {
  uint8_t hue, sat, val;
  CHSV() : hue(0), sat(0), val(0) {}
  CHSV(uint8_t h, uint8_t s, uint8_t v) : hue(h), sat(s), val(v) {}
};

struct CRGB;
void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb);

struct CRGB {
  uint8_t r, g, b;

  enum HTMLColorCode : uint32_t {
    Black = 0x000000, White = 0xFFFFFF, Red = 0xFF0000, Green = 0x008000,
    Blue = 0x0000FF, Yellow = 0xFFFF00, Cyan = 0x00FFFF, Orange = 0xFFA500,
    Magenta = 0xFF00FF, Purple = 0x800080, Gray = 0x808080
  };

  CRGB() : r(0), g(0), b(0) {}
  CRGB(uint8_t ir, uint8_t ig, uint8_t ib) : r(ir), g(ig), b(ib) {}
  CRGB(uint32_t code) : r((code >> 16) & 0xFF), g((code >> 8) & 0xFF), b(code & 0xFF) {}
  CRGB(HTMLColorCode code) : CRGB((uint32_t)code) {}
  CRGB(const CHSV& hsv) { hsv2rgb_rainbow(hsv, *this); }

  CRGB& operator=(const CHSV& hsv) { hsv2rgb_rainbow(hsv, *this); return *this; }

  CRGB& nscale8_video(uint8_t scale) {
    uint8_t nz = scale ? 1 : 0;
    r = r ? (uint8_t)(((int)r * scale >> 8) + nz) : 0;
    g = g ? (uint8_t)(((int)g * scale >> 8) + nz) : 0;
    b = b ? (uint8_t)(((int)b * scale >> 8) + nz) : 0;
    return *this;
  }
  CRGB& nscale8(uint8_t scale) { r = scale8(r, scale); g = scale8(g, scale); b = scale8(b, scale); return *this; }
  CRGB& fadeToBlackBy(uint8_t f) { return nscale8(255 - f); }
  CRGB& operator+=(const CRGB& o) { r = qadd8(r, o.r); g = qadd8(g, o.g); b = qadd8(b, o.b); return *this; }
  CRGB& operator-=(const CRGB& o) { r = qsub8(r, o.r); g = qsub8(g, o.g); b = qsub8(b, o.b); return *this; }
};

inline void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb) {
  uint8_t hue = hsv.hue, sat = hsv.sat, val = hsv.val;
  uint8_t offset = hue & 0x1F;
  uint8_t offset8 = offset << 3;
  uint8_t third = scale8(offset8, (256 / 3));
  uint8_t r = 0, g = 0, b = 0;
  if (!(hue & 0x80)) {
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) { r = 255 - third; g = third; b = 0; }
      else { r = 171; g = 85 + third; b = 0; }
    } else {
      if (!(hue & 0x20)) { uint8_t tt = scale8(offset8, ((256 * 2) / 3)); r = 171 - tt; g = 170 + third; b = 0; }
      else { r = 0; g = 255 - third; b = third; }
    }
  } else {
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) { uint8_t tt = scale8(offset8, ((256 * 2) / 3)); r = 0; g = 171 - tt; b = 85 + tt; }
      else { r = third; g = 0; b = 255 - third; }
    } else {
      if (!(hue & 0x20)) { r = 85 + third; g = 0; b = 171 - third; }
      else { r = 170 + third; g = 0; b = 85 - third; }
    }
  }
  if (sat != 255) {
    if (sat == 0) { r = 255; g = 255; b = 255; }
    else {
      uint8_t desat = 255 - sat;
      desat = scale8_video(desat, desat);
      uint8_t satscale = 255 - desat;
      // Как в FastLED с FASTLED_SCALE8_FIXED=1 (по умолчанию): без «+1»,
      // иначе 255 + пол яркости переполняется в 0.
      r = scale8(r, satscale);
      g = scale8(g, satscale);
      b = scale8(b, satscale);
      r += desat; g += desat; b += desat;
    }
  }
  if (val != 255) {
    val = scale8_video(val, val);
    if (val == 0) { r = 0; g = 0; b = 0; }
    else {
      r = scale8(r, val);
      g = scale8(g, val);
      b = scale8(b, val);
    }
  }
  rgb.r = r; rgb.g = g; rgb.b = b;
}

inline void fadeToBlackBy(CRGB* leds, int n, uint8_t fade) { for (int i = 0; i < n; i++) leds[i].nscale8(255 - fade); }
inline void fill_solid(CRGB* leds, int n, const CRGB& c) { for (int i = 0; i < n; i++) leds[i] = c; }

// ---- Объект FastLED ----
enum EOrder { RGB, RBG, GRB, GBR, BRG, BGR };
struct WS2812B {};
struct WS2812 {};
struct NEOPIXEL {};

class SimFastLED {
 public:
  template <typename CHIPSET, int PIN, EOrder ORDER>
  void addLeds(CRGB* leds, int n) { leds_ = leds; n_ = n; }
  void clear() { if (leds_) for (int i = 0; i < n_; i++) leds_[i] = CRGB(); }
  void show() { shows_++; }
  void setBrightness(uint8_t b) { brightness_ = b; }
  uint8_t getBrightness() const { return brightness_; }
  void setMaxPowerInVoltsAndMilliamps(uint8_t, uint32_t) {}
  CRGB* leds() const { return leds_; }
  int size() const { return n_; }
  uint32_t shows_ = 0;
 private:
  CRGB* leds_ = nullptr;
  int n_ = 0;
  uint8_t brightness_ = 255;
};
extern SimFastLED FastLED;
