// =====================================================================
//  ОБЩИЕ ДАННЫЕ: погода и текст бегущей строки
//  Погода заполняется в weather.cpp, а эффекты только читают эти данные.
// =====================================================================
#pragma once
#include <Arduino.h>

// ---------- Погода (из OpenWeatherMap) ----------
extern long    g_sunrise, g_sunset;   // восход и закат, unix-время (0 — неизвестно)
extern float   g_windSpeed;           // м/с
extern float   g_rainVolume;          // мм за час
extern float   g_snowVolume;          // мм за час
extern uint8_t g_clouds;              // облачность, %
extern int     g_weatherId;           // код погоды OWM
extern float   moonPhase;             // 0 — новолуние, 0.5 — полнолуние

// ---------- Будильник ----------
// Прогресс рассвета будильника 0..1 (или -1, если будильник не звенит).
// Пока он >= 0, эффект «Рассвет» рисует именно его, а не настоящий восход.
extern float   g_alarmDawnProgress;

// ---------- Бегущая строка ----------
extern String  displayText;
extern uint8_t textChars[128];
extern int     textLen, scrollPos;
