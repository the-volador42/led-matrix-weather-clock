// Симулятор матрицы: собирает НАСТОЯЩИЙ код эффектов из скетча
// (matrix.cpp, scene.cpp, effects.cpp, fx_*.cpp — без изменений)
// и отдаёт кадры браузеру через WebAssembly.

#include "Arduino.h"
#include "FastLED.h"
#include "../led_matrix_weather_clock/effects.h"

uint32_t   g_simMillis = 0;
time_t     g_simTime = 0;
uint32_t   g_simRandState = 12345;
SimSerial  Serial;
SimESP     ESP;
SimFastLED FastLED;

// В WebAssembly исключений нет: если std::string когда-нибудь
// «бросит» ошибку (нехватка памяти), просто останавливаемся.
extern "C" void* __cxa_allocate_exception(size_t) { abort(); }
extern "C" void __cxa_throw(void*, void*, void (*)(void*)) { abort(); }

#define SIM_EXPORT(name) extern "C" __attribute__((export_name(#name)))

static int  currentEffect = 0;
static bool effectActive = false;

// Кадр для браузера: RGB по строкам, слева направо, сверху вниз.
static uint8_t frameRGB[MATRIX_W * MATRIX_H * 3];
// Сюда браузер кладёт текст (UTF-8) для бегущей строки.
static char textBuf[512];

SIM_EXPORT(sim_init) void sim_init(uint32_t seed, uint32_t unixTime) {
  g_simMillis = 0;
  g_simTime = (time_t)unixTime;
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(DEFAULT_BRIGHTNESS);
  FastLED.clear();
  resetText();
  resetStars();
  randomSeed(seed);
}

SIM_EXPORT(sim_width)  int sim_width()  { return MATRIX_W; }
SIM_EXPORT(sim_height) int sim_height() { return MATRIX_H; }
SIM_EXPORT(sim_effect_count) int sim_effect_count() { return FX_COUNT; }
SIM_EXPORT(sim_effect_name) const char* sim_effect_name(int i) {
  return (i >= 0 && i < FX_COUNT) ? EFFECTS[i].name : "";
}

// Выбор эффекта: очистить экран и запустить эффект с начала (-1 = выключить).
SIM_EXPORT(sim_select) void sim_select(int id) {
  FastLED.clear();
  if (id < 0 || id >= FX_COUNT) { effectActive = false; return; }
  resetEffect(id);
  currentEffect = id;
  effectActive = true;
  Serial.print("[sim] ");
  Serial.print(id + 1);
  Serial.print(" -> ");
  Serial.println(EFFECTS[id].name);
}

// Один шаг loop(): millis и время задаёт браузер.
SIM_EXPORT(sim_frame) const uint8_t* sim_frame(uint32_t ms, uint32_t unixTime) {
  g_simMillis = ms;
  g_simTime = (time_t)unixTime;
  if (effectActive) runEffect(currentEffect);
  for (int y = 0; y < MATRIX_H; y++)
    for (int x = 0; x < MATRIX_W; x++) {
      int idx = XY(x, y);
      uint8_t* p = &frameRGB[(y * MATRIX_W + x) * 3];
      if (idx >= 0) { p[0] = leds[idx].r; p[1] = leds[idx].g; p[2] = leds[idx].b; }
      else { p[0] = p[1] = p[2] = 0; }
    }
  return frameRGB;
}

SIM_EXPORT(sim_raw_leds) const uint8_t* sim_raw_leds() { return (const uint8_t*)leds; }
SIM_EXPORT(sim_brightness) int sim_brightness() { return FastLED.getBrightness(); }
SIM_EXPORT(sim_xy) int sim_xy(int x, int y) { return XY(x, y); }
SIM_EXPORT(sim_max_milliamps) int sim_max_milliamps() { return LED_MAX_MILLIAMPS; }
SIM_EXPORT(sim_volts) int sim_volts() { return LED_VOLTS; }

// ---- Параметры, которые на часах приходят из погоды ----
SIM_EXPORT(sim_set_wind)   void sim_set_wind(float v) { g_windSpeed = v; }
SIM_EXPORT(sim_set_season) void sim_set_season(int s) { currentSeason = (Season)constrain(s, 0, 3); }
SIM_EXPORT(sim_text_buffer) char* sim_text_buffer() { return textBuf; }
SIM_EXPORT(sim_text_buffer_size) int sim_text_buffer_size() { return sizeof(textBuf); }
SIM_EXPORT(sim_apply_text) void sim_apply_text() {
  textBuf[sizeof(textBuf) - 1] = 0;
  displayText = textBuf;
  resetText();
}
