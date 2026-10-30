#include "alarm.h"
#include "alarm_math.h"
#include "modes.h"
#include "settings.h"
#include <limits.h>

int  alarmHour = 7, alarmMinute = 0;
bool alarmEnabled = false;
bool alarmRinging = false;

static long lastRingKey = LONG_MIN;   // какое утро уже отзвенело
static Mode modeBeforeAlarm = MODE_NORMAL;

void setAlarmTime(int hour, int minute) {
  alarmHour = constrain(hour, 0, 23);
  alarmMinute = constrain(minute, 0, 59);
  lastRingKey = LONG_MIN;   // новое время — можно звонить снова
  stopAlarm();
  rememberState();
}

void setAlarmEnabled(bool on) {
  alarmEnabled = on;
  if (!on) stopAlarm();
  rememberState();
}

void stopAlarm() {
  if (!alarmRinging) return;
  alarmRinging = false;
  g_alarmDawnProgress = -1.0f;
  if (currentMode == MODE_ALARM) currentMode = modeBeforeAlarm;
  globalBrightness = DEFAULT_BRIGHTNESS;
  if (!ALARM_REPEAT_DAILY) alarmEnabled = false;
  rememberState();
  Serial.println("Alarm stopped.");
}

void updateAlarm() {
  time_t now = time(nullptr);
  if (now < 100000) return;   // время ещё не известно

  AlarmWindow w = alarmWindowAt((int64_t)now + tzOffsetSeconds(),
                                alarmHour * 60 + alarmMinute,
                                ALARM_DAWN_MINUTES, ALARM_HOLD_MINUTES);

  if (!alarmRinging) {
    if (!alarmEnabled || !w.inWindow || w.dayKey == lastRingKey) return;
    // Начинаем рассвет.
    lastRingKey = w.dayKey;
    alarmRinging = true;
    modeBeforeAlarm = (currentMode == MODE_ALARM) ? MODE_NORMAL : currentMode;
    currentMode = MODE_ALARM;
    powerOn = true;
    clearNotice();
    Serial.println("Alarm: dawn started.");
  }

  if (!w.inWindow) { stopAlarm(); return; }

  g_alarmDawnProgress = w.progress;
  globalBrightness = MIN_BRIGHTNESS + (int)(w.progress * (ALARM_MAX_BRIGHTNESS - MIN_BRIGHTNESS));
}
