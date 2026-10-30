// =====================================================================
//  ЭФФЕКТЫ
//  Каждый эффект — это пара функций:
//    xxxEffect() — рисует один кадр (вызывается 25 раз в секунду);
//    resetXxx()  — начинает эффект с начала (может отсутствовать).
//
//  Все эффекты собраны в ОДНОМ списке EFFECTS (см. effects.cpp).
//  Чтобы добавить новый эффект:
//    1) напиши функции эффекта в одном из fx_*.cpp и объяви их ниже;
//    2) добавь имя в enum EffectId;
//    3) добавь строку в EFFECTS[] в effects.cpp — на то же место.
// =====================================================================
#pragma once
#include "matrix.h"
#include "scene.h"
#include "state.h"

// ---------- fx_weather.cpp ----------
void resetRain1();  void rainEffect1();   // слабый дождь
void resetRain2();  void rainEffect2();   // дождь с грозой
void resetRain3();  void rainEffect3();   // морось
void resetRain4();  void rainEffect4();   // ливень
void resetSnow1();  void snowEffect1();   // лёгкий снег
void resetSnow2();  void snowEffect2();   // сильный снег
void resetSun2();   void sunEffect2();    // солнце с облаками
void resetSunCloudy(); void sunCloudyEffect();  // пасмурно

// ---------- fx_sky.cpp ----------
void  resetStars();
void  updateStars();
void  drawStars();
float getMoonPhase(time_t t);
void  drawMoonByPhase(int x0, int y0, float phase);
void  nightEffectPhase();                 // звёздная ночь с луной
float getSunsetProgress();
float getSunriseProgress();
void  sunsetDynamicEffect();
void  dawnDynamicEffect();

// ---------- fx_wind.cpp ----------
void resetWind();   void windEffect();

// ---------- fx_misc.cpp ----------
void rainbowEffect();
void clockEffect();
void resetFireGyver();  void fireEffectGyver();
void resetText();       void textScrollEffect();

// ---------- fx_nature.cpp ----------
void summerNoonEffect();
void morningMistEffect();
void windyDayEffect();
void cloudyDayEffect();
void sunsetSkyEffect();
void starryNightEffect();
void resetWaterfall();       void waterfallLakeEffect();
void resetMeadow();          void flowerMeadowEffect();
void resetButterflyGarden(); void butterflyGardenEffect();
void resetHotAirBalloon();   void hotAirBalloonEffect();

// ---------- fx_night.cpp ----------
void resetMilkyWay();  void milkyWayEffect();
void resetQuasar();    void quasarEffect();
void resetAurora();    void auroraEffect();
void resetFireflies(); void fireflyEffect();

// ---------- fx_cycles.cpp ----------
void resetNightEffects(); void runNightEffect();  // смена ночных сцен
void resetDayEffects();   void runDayEffect();    // смена дневных сцен

// =====================================================================
//  ЕДИНЫЙ СПИСОК ЭФФЕКТОВ
// =====================================================================
enum EffectId : uint8_t {
  FX_RAIN_LIGHT, FX_RAIN_STORM, FX_DRIZZLE, FX_RAIN_HEAVY,
  FX_SNOW_LIGHT, FX_SNOW_HEAVY,
  FX_SUN_CLOUDS, FX_OVERCAST,
  FX_SUNSET, FX_DAWN,
  FX_RAINBOW, FX_CLOCK, FX_FIRE, FX_TEXT, FX_WIND,
  FX_WATERFALL, FX_MEADOW,
  FX_SUMMER_NOON, FX_MORNING_MIST,
  FX_BUTTERFLIES, FX_BALLOON,
  FX_STARRY_NIGHT, FX_MILKY_WAY, FX_QUASAR, FX_AURORA, FX_FIREFLIES,
  FX_DAY_CYCLE, FX_NIGHT_CYCLE,
  FX_COUNT
};

struct EffectInfo {
  const char* name;     // название для веб-интерфейса
  void (*run)();        // кадр
  void (*reset)();      // запуск с начала (или nullptr)
};

extern const EffectInfo EFFECTS[];   // ровно FX_COUNT строк (проверяется при сборке)

// Запустить эффект с начала (вызывает его reset, если он есть).
inline void resetEffect(int id) {
  if (id >= 0 && id < FX_COUNT && EFFECTS[id].reset) EFFECTS[id].reset();
}
inline void runEffect(int id) {
  if (id >= 0 && id < FX_COUNT) EFFECTS[id].run();
}
