// =====================================================================
//  РАСЧЁТ ОКНА БУДИЛЬНИКА (без Arduino — проверяется тестами на ПК)
//
//  Окно будильника:
//    [время будильника - dawnMinutes ; время будильника + holdMinutes)
//  Первые dawnMinutes — рассвет (progress растёт от 0 до 1),
//  потом holdMinutes полный свет (progress = 1).
// =====================================================================
#pragma once
#include <stdint.h>

struct AlarmWindow {
  bool  inWindow;   // сейчас время будильника
  long  dayKey;     // номер «дня» окна: чтобы будильник не сработал дважды за одно утро
  float progress;   // 0..1 — насколько рассвело
};

// localSeconds — местное время в секундах от 01.01.1970,
// alarmMinuteOfDay — время будильника в минутах от полуночи (7:30 -> 450).
inline AlarmWindow alarmWindowAt(int64_t localSeconds, int alarmMinuteOfDay,
                                 int dawnMinutes, int holdMinutes) {
  const int64_t DAY = 86400;
  // Сдвигаем время так, чтобы начало окна оказалось в полночь.
  int64_t shifted = localSeconds - (int64_t)(alarmMinuteOfDay - dawnMinutes) * 60;
  int64_t day = shifted / DAY;
  if (shifted % DAY < 0) day -= 1;          // деление вниз и для отрицательных
  int64_t pos = shifted - day * DAY;        // секунд с начала окна: 0..86399

  AlarmWindow w;
  w.dayKey = (long)day;
  w.inWindow = pos < (int64_t)(dawnMinutes + holdMinutes) * 60;
  float p = dawnMinutes > 0 ? (float)pos / (dawnMinutes * 60.0f) : 1.0f;
  w.progress = p > 1.0f ? 1.0f : p;
  return w;
}
