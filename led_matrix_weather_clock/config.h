// =====================================================================
//  НАСТРОЙКИ ПРОЕКТА
//  Пароль Wi-Fi и ключ погоды лежат отдельно — в secrets.h.
// =====================================================================
#pragma once

#define FW_VERSION      "3.1"
#define HOSTNAME        "ledclock"   // адрес в сети: http://ledclock.local

// ---------- Погода (по умолчанию; меняется в веб-интерфейсе) ----------
#define OWM_CITY        "Moscow,RU"
#define OWM_UNITS       "metric"
#define WEATHER_CHECK_INTERVAL    (10UL * 60UL * 1000UL)  // как часто обновлять погоду
#define WEATHER_RETRY_INTERVAL    (60UL * 1000UL)         // повтор после ошибки
#define WIND_SPEED_THRESHOLD  7.0f   // м/с: с какого ветра показывать эффект «Ветер»

// ---------- Время (по умолчанию; меняется в веб-интерфейсе) ----------
#define GMT_OFFSET_SEC       (3 * 3600)

// ---------- Ночное приглушение (по умолчанию выключено) ----------
#define NIGHT_DEFAULT_START       (23 * 60)   // 23:00
#define NIGHT_DEFAULT_END         (7 * 60)    // 07:00
#define NIGHT_DEFAULT_BRIGHTNESS  5

// ---------- Матрица ----------
#define LED_PIN         5
#define MATRIX_W        32
#define MATRIX_H        8
#define NUM_LEDS        (MATRIX_W * MATRIX_H)
#define DEFAULT_BRIGHTNESS 180  // яркость после будильника
#define MIN_BRIGHTNESS  10    // яркость при включении
#define FRAME_DELAY     40    // мс между кадрами (25 кадров в секунду)

// ---------- Питание ----------
// Матрица 32x8 на полной яркости может тянуть до 15 А, а небесные сцены
// на яркости 180 — около 4 А. FastLED сам приглушит кадр, если он не
// укладывается в этот лимит. Блок 5V 2A -> 1800 мА (с запасом на ESP).
#define LED_VOLTS          5
#define LED_MAX_MILLIAMPS  1800

// ---------- Эффекты ----------
#define TEMP_DISPLAY_TIME   5000
#define LIGHTNING_DURATION  500
#define LIGHTNING_INTERVAL  3000
#define SNOW_GEN_INTERVAL   10000
#define DAY_EFFECT_CHANGE_INTERVAL   (2UL * 60UL * 1000UL)
#define NIGHT_EFFECT_CHANGE_INTERVAL (3UL * 60UL * 1000UL)

// ---------- Будильник ----------
#define ALARM_DAWN_MINUTES    20    // рассвет начинается за 20 минут до будильника
#define ALARM_HOLD_MINUTES    10    // сколько минут держать полный свет после
#define ALARM_MAX_BRIGHTNESS  180   // яркость к моменту будильника
#define ALARM_REPEAT_DAILY    true  // false — будильник сработает один раз и выключится

// ---------- Сенсорная кнопка ----------
#define TOUCH_PIN         14
#define DEBOUNCE_TIME     200
#define LONG_PRESS_TIME   1000
#define DOUBLE_CLICK_TIME 400
#define BRIGHTNESS_STEP   5

// ---------- Wi-Fi ----------
#define WIFI_TIMEOUT 20000   // мс на попытку подключения
// Если за это время после включения подключиться не удалось, часы
// раздают свою сеть «LED-Clock-xxxx» для настройки с телефона.
#define PORTAL_AFTER_MS  (60UL * 1000UL)
