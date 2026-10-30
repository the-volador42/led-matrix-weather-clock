// Единый список эффектов. Порядок строк = порядок в enum EffectId (effects.h)
// и в выпадающем списке веб-интерфейса.
#include "effects.h"

const EffectInfo EFFECTS[] = {
  { "Дождь слабый",       rainEffect1,           resetRain1 },
  { "Дождь с грозой",     rainEffect2,           resetRain2 },
  { "Морось",             rainEffect3,           resetRain3 },
  { "Ливень сильный",     rainEffect4,           resetRain4 },
  { "Снег лёгкий",        snowEffect1,           resetSnow1 },
  { "Снег сильный",       snowEffect2,           resetSnow2 },
  { "Солнце с облаками",  sunEffect2,            resetSun2 },
  { "Пасмурно",           sunCloudyEffect,       resetSunCloudy },
  { "Закат",              sunsetDynamicEffect,   nullptr },
  { "Рассвет",            dawnDynamicEffect,     nullptr },
  { "Радуга",             rainbowEffect,         nullptr },
  { "Часы",               clockEffect,           nullptr },
  { "Огонь",              fireEffectGyver,       resetFireGyver },
  { "Бегущая строка",     textScrollEffect,      resetText },
  { "Ветер (дерево)",     windEffect,            resetWind },
  { "Водопад и озеро",    waterfallLakeEffect,   resetWaterfall },
  { "Цветочный луг",      flowerMeadowEffect,    resetMeadow },
  { "Летний полдень",     summerNoonEffect,      nullptr },
  { "Утренний туман",     morningMistEffect,     nullptr },
  { "Бабочки в саду",     butterflyGardenEffect, resetButterflyGarden },
  { "Воздушный шар",      hotAirBalloonEffect,   resetHotAirBalloon },
  { "Звёздная ночь",      nightEffectPhase,      resetStars },
  { "Млечный путь",       milkyWayEffect,        resetMilkyWay },
  { "Квазар",             quasarEffect,          resetQuasar },
  { "Северное сияние",    auroraEffect,          resetAurora },
  { "Светлячки",          fireflyEffect,         resetFireflies },
  { "Смена дневных сцен", runDayEffect,          resetDayEffects },
  { "Смена ночных сцен",  runNightEffect,        resetNightEffects },
};

static_assert(sizeof(EFFECTS) / sizeof(EFFECTS[0]) == FX_COUNT,
              "Список EFFECTS не совпадает с enum EffectId: проверь effects.h и effects.cpp");
