#include "button.h"
#include "modes.h"
#include "alarm.h"
#include "settings.h"

// ========== СОСТОЯНИЯ КНОПКИ ==========
// BTN_IGNORE — касание уже обработано (выключили будильник), ждём, пока палец уберут.
enum ButtonState { BTN_IDLE, BTN_PRESS, BTN_LONG_PRESS, BTN_RELEASE, BTN_IGNORE };
static ButtonState btnState = BTN_IDLE;
static unsigned long btnPressStart = 0, btnLastClick = 0;
static int btnClickCount = 0;
static bool btnLongPressHandled = false;
static bool brightnessUp = false;   // куда крутит яркость удержание

void handleTouchButton() {
  bool currentTouch = (digitalRead(TOUCH_PIN) == HIGH);

  static bool lastStable = false;
  static unsigned long stableTime = 0;
  bool stable = currentTouch;
  if (currentTouch != lastStable) {
    stableTime = millis();
  }
  if (millis() - stableTime < 20) {
    stable = lastStable;
  }
  lastStable = currentTouch;
  currentTouch = stable;

  switch (btnState) {
    case BTN_IDLE:
      if (currentTouch && alarmRinging) {
        // Любое касание во время рассвета будильника — выключить будильник.
        stopAlarm();
        btnState = BTN_IGNORE;
      } else if (currentTouch) {
        btnState = BTN_PRESS;
        btnPressStart = millis();
        btnLongPressHandled = false;
      }
      break;
    case BTN_PRESS:
      if (!currentTouch) {
        if (!btnLongPressHandled && (millis() - btnPressStart > DEBOUNCE_TIME)) {
          if (millis() - btnLastClick < DOUBLE_CLICK_TIME) btnClickCount++;
          else btnClickCount = 1;
          btnLastClick = millis();
          btnState = BTN_RELEASE;
        } else btnState = BTN_IDLE;
      } else if (millis() - btnPressStart > LONG_PRESS_TIME) {
        btnState = BTN_LONG_PRESS;
        btnLongPressHandled = true;
        // Каждое новое удержание меняет направление: ярче / темнее.
        // На краях направление выбирается само.
        brightnessUp = !brightnessUp;
        if (globalBrightness >= 255) brightnessUp = false;
        if (globalBrightness <= MIN_BRIGHTNESS) brightnessUp = true;
      }
      break;
    case BTN_LONG_PRESS:
      if (!currentTouch) { btnState = BTN_IDLE; rememberState(); }   // запомнить яркость
      else {
        static unsigned long lastBrightChange = 0;
        if (millis() - lastBrightChange > 100) {
          lastBrightChange = millis();
          globalBrightness += brightnessUp ? BRIGHTNESS_STEP : -BRIGHTNESS_STEP;
          globalBrightness = constrain(globalBrightness, MIN_BRIGHTNESS, 255);
        }
      }
      break;
    case BTN_RELEASE:
      if (millis() - btnLastClick > DOUBLE_CLICK_TIME) {
        if (btnClickCount == 1) {
          powerOn = !powerOn;
        } else if (btnClickCount == 2) {
          switch (currentMode) {
            case MODE_CLOCK: currentMode = MODE_FIRE; resetFireGyver(); break;
            case MODE_FIRE: currentMode = MODE_DEMO; demoEffectStart = 0; break;
            case MODE_DEMO: currentMode = MODE_NORMAL; break;
            case MODE_NORMAL: currentMode = MODE_CLOCK; break;
            default: currentMode = MODE_CLOCK;
          }
        } else if (btnClickCount >= 3) {
          currentMode = MODE_DEMO;
          demoEffectStart = 0;
        }
        btnClickCount = 0;
        btnState = BTN_IDLE;
        rememberState();   // запомнить режим
      }
      break;
    case BTN_IGNORE:
      if (!currentTouch) btnState = BTN_IDLE;
      break;
  }
}
