// =====================================================================
//  НАСТРОЙКИ, КОТОРЫЕ ПЕРЕЖИВАЮТ ПЕРЕЗАГРУЗКУ
//  Хранятся во флеш-памяти в файле /settings.json (LittleFS).
//  Запись отложенная: через 3 секунды после последнего изменения
//  и только если что-то действительно поменялось.
// =====================================================================
#pragma once
#include <Arduino.h>

struct Settings {
  // Wi-Fi
  String  wifiSsid, wifiPass;
  // Погода и время
  String  city;            // как в OpenWeatherMap: "Moscow,RU"
  String  owmKey;
  int     tzHours;         // часовой пояс: часы от UTC (Москва = 3)
  // Экран
  uint8_t brightness;
  uint8_t mode;
  uint8_t effect;          // эффект для режима «Выбранный эффект»
  String  text;            // бегущая строка
  // Будильник
  uint8_t alarmHour, alarmMinute;
  bool    alarmEnabled;
  // Ночное приглушение
  bool     nightEnabled;
  uint16_t nightStart, nightEnd;   // минуты от полуночи (23:00 = 1380)
  uint8_t  nightBrightness;
  // Время года: 0 — по дате, 1 весна, 2 лето, 3 осень, 4 зима
  uint8_t season;
  // Прошивка по Wi-Fi
  String  otaPassword;
};

extern Settings settings;

void loadSettings();      // в setup(): прочитать файл (или взять значения по умолчанию)
void applySettings();     // перенести настройки в рабочие переменные (яркость, режим…)
void rememberState();     // запомнить текущую яркость/режим/будильник/текст и сохранить позже
void markSettingsDirty(); // настройки изменены напрямую — сохранить позже
void settingsLoop();      // в loop(): отложенная запись
void saveSettingsNow();
long tzOffsetSeconds();   // сдвиг часового пояса в секундах
