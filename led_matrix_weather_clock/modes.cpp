#include "modes.h"
#include "weather.h"
#include "alarm.h"
#include "settings.h"
#include "schedule_math.h"

Mode currentMode = MODE_NORMAL;
int  selectedEffectIndex = 0;
int  lastSelectedEffect = -1;

int  globalBrightness = MIN_BRIGHTNESS;  // старт с минимальной яркости
bool powerOn = true;

const char* const MODE_NAMES[MODE_COUNT] = {
  "Погода", "Демо", "Часы", "Огонь", "Будильник", "Бегущая строка",
  "Рассвет", "Закат", "Дождь", "Снег", "Ночь", "Луна", "Радуга", "Выбранный эффект"
};

// =====================================================================
//  ВРЕМЯ ГОДА: по дате или как выбрано в веб-интерфейсе
// =====================================================================
static void updateSeason() {
  if (settings.season >= 1 && settings.season <= 4) {
    currentSeason = (Season)(settings.season - 1);
    return;
  }
  time_t now = time(nullptr);
  if (now < 100000) return;            // дата ещё неизвестна — оставляем как есть
  time_t local = now + tzOffsetSeconds();
  struct tm* t = gmtime(&local);
  currentSeason = seasonForMonth(t->tm_mon + 1);
}

// =====================================================================
//  НОЧНОЕ ПРИГЛУШЕНИЕ
// =====================================================================
bool nightDimActive() {
  if (!settings.nightEnabled || alarmRinging) return false;
  time_t now = time(nullptr);
  if (now < 100000) return false;
  int minute = (int)(((int64_t)now + tzOffsetSeconds()) % 86400 / 60);
  return inDailyWindow(minute, settings.nightStart, settings.nightEnd);
}

uint8_t effectiveBrightness() {
  int b = globalBrightness;
  if (nightDimActive() && b > settings.nightBrightness) b = settings.nightBrightness;
  return (uint8_t)constrain(b, 0, 255);
}

// =====================================================================
//  УВЕДОМЛЕНИЯ
// =====================================================================
static uint8_t noticeGlyphs[96];
static int  noticeLen = 0, noticePos = 0, noticeRepeats = 0;
static bool noticeOn = false;
static CRGB noticeColor;

void showNotice(const String& text, int repeats, CRGB color) {
  utf8ToIndices(text, noticeGlyphs, sizeof(noticeGlyphs), noticeLen);
  noticePos = MATRIX_W;
  noticeRepeats = repeats;
  noticeColor = color;
  noticeOn = noticeLen > 0;
}

void clearNotice() { noticeOn = false; }

static bool drawNotice() {
  if (!noticeOn) return false;
  FastLED.clear();
  drawGlyphLine(noticeGlyphs, noticeLen, noticePos, 0, noticeColor);
  noticePos--;
  if (noticePos < -noticeLen * 6) {
    noticePos = MATRIX_W;
    if (noticeRepeats > 0 && --noticeRepeats == 0) noticeOn = false;
  }
  return true;
}

// =====================================================================
//  ДЕМО: случайный эффект из списка на 11–15 минут
// =====================================================================
static const EffectId DEMO_EFFECTS[] = {
  FX_DAWN, FX_SUNSET, FX_SUNSET, FX_SUN_CLOUDS, FX_OVERCAST,
  FX_NIGHT_CYCLE, FX_RAIN_LIGHT, FX_SNOW_LIGHT, FX_RAIN_STORM, FX_FIRE, FX_RAINBOW, FX_WIND,
  FX_WATERFALL, FX_MEADOW, FX_BUTTERFLIES, FX_BALLOON
};
static const int DEMO_COUNT = sizeof(DEMO_EFFECTS) / sizeof(DEMO_EFFECTS[0]);

unsigned long demoEffectStart = 0;
static unsigned long demoEffectDuration = 0;
static int demoEffectIndex = 0;

static void handleDemoMode() {
  unsigned long currentTime = millis();
  if (demoEffectStart == 0 || currentTime - demoEffectStart > demoEffectDuration) {
    demoEffectIndex = random(0, DEMO_COUNT);
    demoEffectDuration = (10UL * 60UL * 1000UL) + random(60UL * 1000UL, 5UL * 60UL * 1000UL);
    demoEffectStart = currentTime;
    resetEffect(DEMO_EFFECTS[demoEffectIndex]);
  }
  runEffect(DEMO_EFFECTS[demoEffectIndex]);
}

// =====================================================================
//  КАДР ТЕКУЩЕГО РЕЖИМА
// =====================================================================
void runCurrentMode() {
  updateSeason();
  FastLED.setBrightness(effectiveBrightness());
  if (drawNotice()) { FastLED.show(); return; }   // уведомление видно даже при выключенном экране
  if (!powerOn) { FastLED.clear(); FastLED.show(); return; }

  switch (currentMode) {
    case MODE_DEMO:    handleDemoMode();           break;
    case MODE_CLOCK:   runEffect(FX_CLOCK);        break;
    case MODE_FIRE:    runEffect(FX_FIRE);         break;
    case MODE_TEXT:    runEffect(FX_TEXT);         break;
    case MODE_NORMAL:  runWeatherEffect();         break;
    case MODE_ALARM:   runEffect(FX_DAWN);         break;
    case MODE_SUNRISE: runEffect(FX_DAWN);         break;
    case MODE_SUNSET:  runEffect(FX_SUNSET);       break;
    case MODE_RAIN:    runEffect(FX_RAIN_LIGHT);   break;
    case MODE_SNOW:    runEffect(FX_SNOW_LIGHT);   break;
    case MODE_NIGHT:   runEffect(FX_NIGHT_CYCLE);  break;
    case MODE_MOON:    runEffect(FX_STARRY_NIGHT); break;
    case MODE_RAINBOW: runEffect(FX_RAINBOW);      break;
    case MODE_EFFECT:
      selectedEffectIndex = constrain(selectedEffectIndex, 0, FX_COUNT - 1);
      runEffect(selectedEffectIndex);
      break;
    default: break;
  }
  FastLED.show();
}
