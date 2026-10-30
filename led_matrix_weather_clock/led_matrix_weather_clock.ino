// =====================================================================
//  ЧАСЫ-МАТРИЦА С ПОГОДОЙ  (ESP8266 + WS2812B 32x8)
//
//  Где что лежит:
//    config.h          — настройки по умолчанию (пины, интервалы, лимит тока)
//    settings.*        — настройки, которые меняются в веб-интерфейсе и
//                        хранятся во флеш-памяти (Wi-Fi, город, будильник…)
//    secrets.h         — необязательно: Wi-Fi и ключ погоды для первого запуска
//    effects.h/.cpp    — единый список эффектов
//    fx_*.cpp          — сами эффекты
//    matrix.*, scene.* — светодиоды, шрифт, деревья, домик, солнце
//    modes.*           — режимы, демо, ночное приглушение, уведомления
//    alarm.*           — будильник с плавным рассветом
//    weather.*         — погода и выбор эффекта по ней
//    network.*         — Wi-Fi, время, настройка с телефона, прошивка по Wi-Fi
//    web.*, web_page.h — веб-интерфейс
//    button.*          — сенсорная кнопка
// =====================================================================
#include <ESP8266WiFi.h>
#include "config.h"
#include "effects.h"
#include "modes.h"
#include "weather.h"
#include "network.h"
#include "web.h"
#include "button.h"
#include "alarm.h"
#include "settings.h"

static unsigned long lastFrame = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("LED clock " FW_VERSION);
  randomSeed(analogRead(A0));

  loadSettings();      // настройки из флеш-памяти (или по умолчанию)

  pinMode(TOUCH_PIN, INPUT);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(LED_VOLTS, LED_MAX_MILLIAMPS);
  FastLED.setBrightness(MIN_BRIGHTNESS);
  FastLED.clear(); FastLED.show();

  resetFireGyver();
  resetWind();
  resetDayEffects();
  applySettings();     // яркость, режим, эффект, текст, будильник
  if (currentMode == MODE_EFFECT) resetEffect(selectedEffectIndex);

  setupWebRoutes();
  startWebInterface(); // работает и в режиме настройки Wi-Fi
  Serial.println("Ready.");
}

void loop() {
  handleWiFiConnection();              // Wi-Fi, время, режим настройки, прошивка по Wi-Fi
  updateWeather(networkReady());       // погода раз в 10 минут (после ошибки — через минуту)
  handleWebClient();
  settingsLoop();                      // отложенное сохранение настроек

  handleTouchButton();
  updateAlarm();

  if (millis() - lastFrame >= FRAME_DELAY) {
    lastFrame = millis();
    runCurrentMode();
  }
  delay(1);
}
