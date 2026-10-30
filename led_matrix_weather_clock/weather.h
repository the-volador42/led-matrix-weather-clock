// =====================================================================
//  ПОГОДА: запрос к OpenWeatherMap и выбор эффекта
// =====================================================================
#pragma once
#include "effects.h"

extern int weatherEffect;   // эффект, выбранный по погоде (EffectId)
extern int lastTemp;        // -100 = температура ещё неизвестна
extern bool showTemp;
extern unsigned long lastTempShow;
extern unsigned long weatherUpdatedAt;   // millis() последнего удачного обновления
extern String weatherError;              // пусто, если всё хорошо

void requestWeatherUpdate();   // обновить погоду как можно скорее

// Запросить погоду и выбрать эффект (Wi-Fi должен быть подключён).
// Возвращает true, если погода получена.
bool fetchWeatherAndPickEffect();

// Вызывать в каждом loop(): обновляет погоду по таймеру.
void updateWeather(bool networkReady);

// Сменить погодный эффект. Эффект перезапускается, только если он
// действительно поменялся.
void setWeatherEffect(int id);

// Кадр режима «Обычный»: сначала несколько секунд температура, потом эффект.
void runWeatherEffect();
