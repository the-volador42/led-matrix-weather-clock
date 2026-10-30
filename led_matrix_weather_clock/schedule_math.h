// =====================================================================
//  ЕЖЕДНЕВНОЕ РАСПИСАНИЕ (без Arduino — проверяется тестами на ПК)
// =====================================================================
#pragma once

// Попадает ли время (минуты от полуночи) в окно [start; end).
// Окно может переходить через полночь: 23:00–07:00.
// start == end — окно пустое.
inline bool inDailyWindow(int minuteOfDay, int startMin, int endMin) {
  if (startMin == endMin) return false;
  if (startMin < endMin) return minuteOfDay >= startMin && minuteOfDay < endMin;
  return minuteOfDay >= startMin || minuteOfDay < endMin;
}
