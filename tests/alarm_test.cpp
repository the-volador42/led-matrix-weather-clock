// Тесты расчёта окна будильника. Запуск: tests\run_tests.bat
#include "../led_matrix_weather_clock/alarm_math.h"
#include <cstdio>
#include <cmath>

static int failures = 0;

static int64_t at(int day, int h, int m, int s = 0) {
  return (int64_t)day * 86400 + h * 3600 + m * 60 + s;
}

static void check(bool cond, const char* what) {
  if (!cond) { printf("  ОШИБКА: %s\n", what); failures++; }
}

static void expectWindow(const char* name, int64_t t, int alarmMin, bool in, float progress = -1) {
  AlarmWindow w = alarmWindowAt(t, alarmMin, 20, 10);
  bool ok = (w.inWindow == in) && (progress < 0 || std::fabs(w.progress - progress) < 0.001f);
  printf("%s %-45s in=%d progress=%.3f\n", ok ? "ok  " : "FAIL", name, w.inWindow, w.progress);
  if (!ok) failures++;
}

int main() {
  const int D = 20000;          // какой-то день
  const int A7 = 7 * 60;        // будильник 07:00

  printf("Будильник 07:00, рассвет 20 мин, свет 10 мин\n");
  expectWindow("06:39:59 — ещё рано",            at(D, 6, 39, 59), A7, false);
  expectWindow("06:40:00 — рассвет начался",     at(D, 6, 40),     A7, true, 0.0f);
  expectWindow("06:50:00 — половина рассвета",   at(D, 6, 50),     A7, true, 0.5f);
  expectWindow("07:00:00 — полный свет",         at(D, 7, 0),      A7, true, 1.0f);
  expectWindow("07:09:59 — ещё светит",          at(D, 7, 9, 59),  A7, true, 1.0f);
  expectWindow("07:10:00 — закончился",          at(D, 7, 10),     A7, false);
  expectWindow("12:00:00 — днём выключен",       at(D, 12, 0),     A7, false);

  AlarmWindow a = alarmWindowAt(at(D, 6, 45), A7, 20, 10);
  AlarmWindow b = alarmWindowAt(at(D, 7, 5), A7, 20, 10);
  AlarmWindow c = alarmWindowAt(at(D + 1, 6, 45), A7, 20, 10);
  check(a.dayKey == b.dayKey, "одно утро — один и тот же dayKey");
  check(c.dayKey == a.dayKey + 1, "следующее утро — следующий dayKey");

  printf("\nБудильник 00:10 — рассвет начинается накануне в 23:50\n");
  const int A0010 = 10;
  expectWindow("23:49:59 — ещё рано",            at(D, 23, 49, 59), A0010, false);
  expectWindow("23:55:00 — четверть рассвета",   at(D, 23, 55),     A0010, true, 0.25f);
  expectWindow("00:05:00 — три четверти",        at(D + 1, 0, 5),   A0010, true, 0.75f);
  expectWindow("00:19:59 — ещё светит",          at(D + 1, 0, 19, 59), A0010, true, 1.0f);
  expectWindow("00:20:00 — закончился",          at(D + 1, 0, 20),  A0010, false);
  AlarmWindow m1 = alarmWindowAt(at(D, 23, 55), A0010, 20, 10);
  AlarmWindow m2 = alarmWindowAt(at(D + 1, 0, 5), A0010, 20, 10);
  check(m1.dayKey == m2.dayKey, "окно через полночь — один dayKey");

  printf("\nБудильник 23:55 — свет держится после полуночи\n");
  const int A2355 = 23 * 60 + 55;
  expectWindow("00:04:59 — ещё светит",          at(D + 1, 0, 4, 59), A2355, true, 1.0f);
  expectWindow("00:05:00 — закончился",          at(D + 1, 0, 5),     A2355, false);

  printf(failures ? "\nОШИБОК: %d\n" : "\nВсе тесты прошли.\n", failures);
  return failures ? 1 : 0;
}
