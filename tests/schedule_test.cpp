// Тесты ночного расписания. Запуск: tests\run_tests.bat
#include "../led_matrix_weather_clock/schedule_math.h"
#include <cstdio>

static int failures = 0;
static int M(int h, int m) { return h * 60 + m; }

static void expect(const char* name, bool got, bool want) {
  printf("%s %s\n", got == want ? "ok  " : "FAIL", name);
  if (got != want) failures++;
}

int main() {
  printf("Ночь 23:00–07:00 (через полночь)\n");
  expect("22:59 — ещё день",   inDailyWindow(M(22, 59), M(23, 0), M(7, 0)), false);
  expect("23:00 — ночь",       inDailyWindow(M(23, 0),  M(23, 0), M(7, 0)), true);
  expect("00:00 — ночь",       inDailyWindow(M(0, 0),   M(23, 0), M(7, 0)), true);
  expect("06:59 — ночь",       inDailyWindow(M(6, 59),  M(23, 0), M(7, 0)), true);
  expect("07:00 — уже день",   inDailyWindow(M(7, 0),   M(23, 0), M(7, 0)), false);
  expect("12:00 — день",       inDailyWindow(M(12, 0),  M(23, 0), M(7, 0)), false);

  printf("\nОкно 01:00–05:00 (в пределах суток)\n");
  expect("00:59 — нет",        inDailyWindow(M(0, 59),  M(1, 0), M(5, 0)), false);
  expect("01:00 — да",         inDailyWindow(M(1, 0),   M(1, 0), M(5, 0)), true);
  expect("04:59 — да",         inDailyWindow(M(4, 59),  M(1, 0), M(5, 0)), true);
  expect("05:00 — нет",        inDailyWindow(M(5, 0),   M(1, 0), M(5, 0)), false);

  printf("\nНачало = конец\n");
  expect("пустое окно",        inDailyWindow(M(3, 0),   M(3, 0), M(3, 0)), false);

  printf(failures ? "\nОШИБОК: %d\n" : "\nВсе тесты прошли.\n", failures);
  return failures ? 1 : 0;
}
