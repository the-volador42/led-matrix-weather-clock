// Заглушка Arduino для симулятора на ПК.
// Повторяет только то, что используют эффекты: время, random, String, Serial.
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string>

typedef uint8_t byte;

// ---- Время: им управляет симулятор, а не настоящие часы ----
extern uint32_t g_simMillis;
extern time_t   g_simTime;

inline unsigned long millis() { return g_simMillis; }
inline unsigned long micros() { return g_simMillis * 1000UL; }
inline void delay(unsigned long) {}
inline void yield() {}

inline time_t sim_time(time_t* out) { if (out) *out = g_simTime; return g_simTime; }
#define time(x) sim_time(x)

// ---- random() как в Arduino ----
extern uint32_t g_simRandState;
inline void randomSeed(unsigned long seed) { g_simRandState = seed ? (uint32_t)seed : 1u; }
inline uint32_t sim_rand32() {
  // xorshift32 — быстрый и предсказуемый генератор
  uint32_t x = g_simRandState;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  g_simRandState = x;
  return x;
}
inline long random(long howbig) { if (howbig <= 0) return 0; return (long)(sim_rand32() % (uint32_t)howbig); }
inline long random(long howsmall, long howbig) {
  if (howsmall >= howbig) return howsmall;
  return random(howbig - howsmall) + howsmall;
}

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  if (in_max == in_min) return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

#define F(s) (s)
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))

// ---- Упрощённый String ----
class String {
 public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  String(int v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  unsigned int length() const { return (unsigned int)s_.size(); }
  char operator[](unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
  String& operator=(const char* s) { s_ = s ? s : ""; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  String& operator+=(const char* s) { s_ += s; return *this; }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  long toInt() const { return atol(s_.c_str()); }
  const char* c_str() const { return s_.c_str(); }
 private:
  std::string s_;
};

// ---- Serial: вывод идёт в консоль браузера ----
// Функция приходит из JavaScript (выводит строку в консоль браузера).
extern "C" __attribute__((import_module("env"), import_name("sim_log"))) void sim_log(const char* text);
class SimSerial {
 public:
  void begin(unsigned long) {}
  int available() { return 0; }
  int read() { return -1; }
  void print(const char* s) { buf_ += s; }
  void print(const String& s) { buf_ += s.c_str(); }
  void print(char c) { buf_ += c; }
  void print(int v) { buf_ += std::to_string(v); }
  void print(long v) { buf_ += std::to_string(v); }
  void print(unsigned long v) { buf_ += std::to_string(v); }
  void print(double v) { buf_ += std::to_string(v); }
  void println() { sim_log(buf_.c_str()); buf_.clear(); }
  template <typename T> void println(T v) { print(v); println(); }
  template <typename... A> void printf(const char* fmt, A... a) {
    char tmp[256]; snprintf(tmp, sizeof(tmp), fmt, a...); buf_ += tmp;
  }
 private:
  std::string buf_;
};
extern SimSerial Serial;

struct SimESP { uint32_t getChipId() { return 0x123456; } };
extern SimESP ESP;
