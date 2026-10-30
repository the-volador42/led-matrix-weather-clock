// Автоматическая смена сцен: ночные (каждые 3 мин) и дневные (каждые 2 мин).
#include "effects.h"

// --- Смена ночных суб-эффектов: звёзды / млечный путь / квазар / сияние / светлячки ---
enum NightSubEffect { NIGHT_STARS, NIGHT_MILKYWAY, NIGHT_QUASAR, NIGHT_AURORA, NIGHT_FIREFLIES };
NightSubEffect currentNightEffect = NIGHT_STARS;
unsigned long lastNightEffectChange = 0;

void chooseNextNightEffect() {
  int r = random(0, 100);
  if (r < 40) currentNightEffect = NIGHT_STARS;
  else if (r < 60) currentNightEffect = NIGHT_MILKYWAY;
  else if (r < 75) currentNightEffect = NIGHT_QUASAR;
  else if (r < 92) currentNightEffect = NIGHT_AURORA;
  else currentNightEffect = NIGHT_FIREFLIES;

  switch (currentNightEffect) {
    case NIGHT_MILKYWAY:  resetMilkyWay();  break;
    case NIGHT_QUASAR:    resetQuasar();    break;
    case NIGHT_AURORA:    resetAurora();    break;
    case NIGHT_FIREFLIES: resetFireflies(); break;
    default:              resetStars();     break;
  }
}

void resetNightEffects() {
  currentNightEffect = NIGHT_STARS;
  lastNightEffectChange = millis();
  resetStars();
}

void runNightEffect() {
  if (millis() - lastNightEffectChange > NIGHT_EFFECT_CHANGE_INTERVAL) {
    lastNightEffectChange = millis();
    chooseNextNightEffect();
  }
  switch (currentNightEffect) {
    case NIGHT_MILKYWAY:  milkyWayEffect();  break;
    case NIGHT_QUASAR:    quasarEffect();    break;
    case NIGHT_AURORA:    auroraEffect();    break;
    case NIGHT_FIREFLIES: fireflyEffect();   break;
    default:               nightEffectPhase(); break;
  }
}

// =====================================================================
//  ДИНАМИЧЕСКАЯ СМЕНА ДНЕВНЫХ ЭФФЕКТОВ
//  (сюда же включён ночной цикл — так дневные и ночные сцены периодически
//   сменяют друг друга)
// =====================================================================
typedef void (*DayEffectFunc)();
DayEffectFunc dayEffects[] = {
  summerNoonEffect,
  morningMistEffect,
  windyDayEffect,
  cloudyDayEffect,
  sunsetSkyEffect,
  waterfallLakeEffect,
  flowerMeadowEffect,
  butterflyGardenEffect,
  hotAirBalloonEffect,
  runNightEffect
};

typedef void (*DayResetFunc)();
DayResetFunc dayResets[] = {
  NULL, NULL, resetWind, resetSunCloudy, NULL,
  resetWaterfall, resetMeadow, resetButterflyGarden, resetHotAirBalloon,
  resetNightEffects
};

int dayEffectCount = sizeof(dayEffects) / sizeof(dayEffects[0]);
int currentDayEffect = 0;
unsigned long lastDayEffectChange = 0;

void resetDayEffects() {
  currentDayEffect = 0;
  lastDayEffectChange = millis();
  if (dayResets[currentDayEffect]) dayResets[currentDayEffect]();
}

void runDayEffect() {
  if (millis() - lastDayEffectChange > DAY_EFFECT_CHANGE_INTERVAL) {
    lastDayEffectChange = millis();
    currentDayEffect = (currentDayEffect + 1) % dayEffectCount;
    if (dayResets[currentDayEffect]) dayResets[currentDayEffect]();
  }
  dayEffects[currentDayEffect]();
}
