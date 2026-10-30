// =====================================================================
//  РЕЖИМЫ РАБОТЫ И ДЕМО
// =====================================================================
#pragma once
#include "effects.h"

// MODE_EFFECT — показать любой эффект из списка EFFECTS (выбор в веб-интерфейсе).
enum Mode {
  MODE_NORMAL, MODE_DEMO, MODE_CLOCK, MODE_FIRE,
  MODE_ALARM, MODE_TEXT, MODE_SUNRISE, MODE_SUNSET,
  MODE_RAIN, MODE_SNOW, MODE_NIGHT, MODE_MOON, MODE_RAINBOW,
  MODE_EFFECT,
  MODE_COUNT
};

extern Mode currentMode;
extern int  selectedEffectIndex;   // эффект для MODE_EFFECT
extern int  lastSelectedEffect;

extern int  globalBrightness;
extern bool powerOn;

extern unsigned long demoEffectStart;  // 0 = в демо выбрать новый эффект

void runCurrentMode();   // нарисовать кадр текущего режима и показать его

// Яркость с учётом ночного приглушения.
uint8_t effectiveBrightness();
bool nightDimActive();

// Уведомление — бегущая строка поверх любого режима (IP-адрес, имя сети…).
// repeats = сколько раз прокрутить; 0 — пока не вызовут clearNotice().
void showNotice(const String& text, int repeats, CRGB color = CRGB::White);
void clearNotice();

// Названия режимов для веб-интерфейса.
extern const char* const MODE_NAMES[MODE_COUNT];
