// Общие объекты сцены: сезон, деревья, домик, солнце и реестр
// «твёрдых» объектов (чтобы звёзды не появлялись вплотную к домику).
#include "scene.h"

// =====================================================================
//  ВРЕМЯ ГОДА (сезон)
// =====================================================================
// Сезон меняет землю и кроны деревьев:
//   весна — розовый цвет в кронах и цветы в траве;
//   лето  — всё зелёное (так эффекты и были нарисованы);
//   осень — жёлтые, оранжевые и красные листья и на деревьях, и на земле;
//   зима  — голые ветки со снегом, земля белая.
// Сезон выбирается по дате или вручную (modes.cpp, updateSeason()).

Season currentSeason = SEASON_SUMMER;

static const CRGB SUMMER_LEAF(0, 150, 0);

CRGB getLeafColor(Season season) {
  switch (season) {
    case SEASON_SPRING: return CRGB(230, 110, 170);   // розовый цвет
    case SEASON_AUTUMN: return CRGB(230, 110, 0);     // оранжевый лист
    case SEASON_WINTER: return CRGB(210, 220, 240);   // снежинка
    default:            return SUMMER_LEAF;
  }
}

// Число 0..255, постоянное для каждой точки: осенние листья и весенние
// цветы стоят на своих местах и не мерцают от кадра к кадру.
static uint8_t cellHash(int x, int y) {
  uint32_t h = (uint32_t)(x + 11) * 2654435761u ^ (uint32_t)(y + 7) * 40503u;
  h ^= h >> 13;
  h *= 0x5bd1e995u;
  h ^= h >> 15;
  return (uint8_t)h;
}

static uint8_t levelOf(const CRGB& c) {
  uint8_t m = c.r > c.g ? c.r : c.g;
  return m > c.b ? m : c.b;
}

// Цвет из сезонной палитры с яркостью исходного «летнего» цвета:
// ночная трава останется тёмной, дневная — светлой.
static CRGB withLevel(CRGB color, uint8_t level) {
  color.nscale8_video(level);
  return color;
}

// Снег светлее травы: яркость в полтора раза выше (но не больше 255).
static uint8_t snowLevel(uint8_t level) {
  int l = level * 3 / 2;
  return l > 255 ? 255 : (uint8_t)l;
}

static const CRGB AUTUMN_COLORS[] = {
  CRGB(255, 190, 0),    // жёлтый
  CRGB(255, 110, 0),    // оранжевый
  CRGB(210, 30, 0),     // красный
  CRGB(200, 120, 10),   // охра
};
static const CRGB SPRING_PINK(255, 110, 180);
static const CRGB SPRING_PALE(255, 170, 210);
static const CRGB SNOW(225, 232, 255);
static const CRGB SNOW_SHADE(175, 190, 225);

CRGB seasonGround(int x, int y, CRGB base) {
  uint8_t h = cellHash(x, y), level = levelOf(base);
  switch (currentSeason) {
    case SEASON_AUTUMN:
      return withLevel(AUTUMN_COLORS[h & 3], level);
    case SEASON_WINTER:
      return withLevel(h % 5 == 0 ? SNOW_SHADE : SNOW, snowLevel(level));
    case SEASON_SPRING:
      if (h % 3 == 0) return withLevel(h & 16 ? SPRING_PINK : SPRING_PALE, level);   // цветы в траве
      return base;
    default:
      return base;
  }
}

CRGB seasonFoliage(int x, int y, CRGB base) {
  uint8_t h = cellHash(x, y), level = levelOf(base);
  switch (currentSeason) {
    case SEASON_AUTUMN:
      return withLevel(AUTUMN_COLORS[h & 3], level);
    case SEASON_WINTER:   // куст под снегом
      return withLevel(h % 3 == 0 ? CRGB(110, 70, 40) : SNOW, snowLevel(level));
    case SEASON_SPRING:   // две трети — розовый цвет
      if (h % 3 != 0) return withLevel(h & 16 ? SPRING_PINK : SPRING_PALE, level);
      return base;
    default:
      return base;
  }
}

Season seasonForMonth(int month) {   // 1..12
  if (month == 12 || month <= 2) return SEASON_WINTER;
  if (month <= 5) return SEASON_SPRING;
  if (month <= 8) return SEASON_SUMMER;
  return SEASON_AUTUMN;
}

// =====================================================================
//  ОБЪЕКТЫ И ЭФФЕКТЫ СЦЕНЫ
// =====================================================================
// Договорённость о терминах:
//  - "Объект" — твёрдая сущность сцены (дерево, домик, звезда). Объекты
//    не могут появляться вплотную друг к другу — между ними всегда
//    остаётся зазор (margin).
//  - "Эффект" — фантомная частица (дождевая капля, снежинка, лист,
//    трава, вода и т.п.). Эффекты друг друга не толкают и свободно
//    перекрываются с объектами и друг с другом.
//
// Ниже — простой реестр твёрдых объектов с проверкой "можно ли тут
// поставить ещё один объект, не касаясь соседей".

SceneObject sceneObjects[MAX_SCENE_OBJECTS];

void clearSceneObjects() {
  for (int i = 0; i < MAX_SCENE_OBJECTS; i++) sceneObjects[i].used = false;
}

bool boxesTooClose(int ax0, int ay0, int ax1, int ay1,
                    int bx0, int by0, int bx1, int by1, int margin) {
  // Расширяем бокс b на margin клеток во все стороны и проверяем пересечение.
  return !(ax1 < bx0 - margin || ax0 > bx1 + margin ||
           ay1 < by0 - margin || ay0 > by1 + margin);
}

// Можно ли поставить твёрдый объект с данным боксом, не касаясь вплотную
// уже существующих объектов сцены?
bool canPlaceObject(int x0, int y0, int x1, int y1, int margin) {
  for (int i = 0; i < MAX_SCENE_OBJECTS; i++) {
    if (!sceneObjects[i].used) continue;
    if (boxesTooClose(x0, y0, x1, y1,
                       sceneObjects[i].x0, sceneObjects[i].y0,
                       sceneObjects[i].x1, sceneObjects[i].y1, margin))
      return false;
  }
  return true;
}

int registerObject(int x0, int y0, int x1, int y1, uint8_t margin) {
  for (int i = 0; i < MAX_SCENE_OBJECTS; i++) {
    if (!sceneObjects[i].used) {
      sceneObjects[i].used = true;
      sceneObjects[i].x0 = x0; sceneObjects[i].y0 = y0;
      sceneObjects[i].x1 = x1; sceneObjects[i].y1 = y1;
      sceneObjects[i].margin = margin;
      return i;
    }
  }
  return -1;
}

void unregisterObject(int id) {
  if (id >= 0 && id < MAX_SCENE_OBJECTS) sceneObjects[id].used = false;
}

// =====================================================================
//  ДЕРЕВО — ЕДИНАЯ ФУНКЦИЯ ДЛЯ ВСЕХ ЭФФЕКТОВ
//  Раньше в коде было несколько отдельных функций (drawTreeLeft,
//  drawTreeRight, отдельная нарисованная вручную "ёлка" внутри ночного
//  эффекта и внутри эффекта ветра). Теперь всё дерево рисуется одной
//  параметризуемой функцией drawTree(), как это сделано для домика
//  (drawHouse) — а drawTrees() лишь вызывает её дважды (слева и справа).
//
//  Параметр season определяет крону: весной розовый цвет, летом зелёная,
//  осенью жёлто-оранжево-красная, зимой голые ветки со снегом.
//  ВАЖНО: для эффекта ветра (windEffect) дерево рисуется ОТДЕЛЬНОЙ
//  функцией drawWindTree() — там крона не статична, а "живая" и умеет
//  менять форму, поэтому она не может использовать этот же неподвижный
//  шаблон кроны.
// =====================================================================

// Зимнее дерево: голые ветки (#) и снег на них (*), 6 колонок, строки 1..4.
static const char* const WINTER_BRANCHES[4] = {
  "*.**.*",
  ".#..#.",
  "#.##.#",
  ".#..#.",
};

void drawTree(int baseX, int bend, CRGB trunkColor, Season season) {
  // baseX — левый край кроны (крона занимает 6 колонок по ширине)
  int t1 = baseX + 2, t2 = baseX + 3;
  for (int y = 5; y < 8; y++) {
    setPixel(t1, y, trunkColor);
    setPixel(t2, y, trunkColor);
  }

  if (season == SEASON_WINTER) {
    for (int row = 0; row < 4; row++)
      for (int col = 0; col < 6; col++) {
        char c = WINTER_BRANCHES[row][col];
        if (c == '#') setPixel(baseX + col + bend, row + 1, trunkColor);
        else if (c == '*') setPixel(baseX + col + bend, row + 1, SNOW);
      }
    return;
  }

  // Крона: каждая точка получает свой сезонный цвет (летом — прежний зелёный).
  Season saved = currentSeason;
  currentSeason = season;
  for (int x = 0; x <= 5; x++) setPixel(baseX + x + bend, 4, seasonFoliage(baseX + x, 4, SUMMER_LEAF));
  for (int x = 1; x <= 4; x++) setPixel(baseX + x + bend, 3, seasonFoliage(baseX + x, 3, SUMMER_LEAF));
  for (int x = 2; x <= 3; x++) setPixel(baseX + x + bend, 2, seasonFoliage(baseX + x, 2, SUMMER_LEAF));
  setPixel(baseX + 2 + bend, 1, seasonFoliage(baseX + 2, 1, SUMMER_LEAF));
  setPixel(baseX + 3 + bend, 1, seasonFoliage(baseX + 3, 1, SUMMER_LEAF));
  currentSeason = saved;
}

void drawTrees(Season season) {
  drawTree(0, 0, CRGB(100, 60, 20), season);
  drawTree(26, 0, CRGB(100, 60, 20), season);
}

// =====================================================================
//  ДОМИК
// =====================================================================
bool isHouse(int x, int y) {
  if (y == 3 && x == 26) return true;
  if (y == 4 && (x >= 25 && x <= 27)) return true;
  if (y == 5 && (x >= 24 && x <= 28)) return true;
  if (y == 6 && (x >= 25 && x <= 27)) return true;
  if (y == 7 && (x >= 24 && x <= 28)) return true;
  return false;
}

void drawHouse() {
  setPixel(26,3, CRGB(200, 100, 0));
  setPixel(25,4, CRGB(200, 100, 0));
  setPixel(27,4, CRGB(200, 100, 0));
  setPixel(24,5, CRGB(200, 100, 0));
  setPixel(25,5, CRGB(200, 100, 0));
  setPixel(26,5, CRGB(200, 100, 0));
  setPixel(27,5, CRGB(200, 100, 0));
  setPixel(28,5, CRGB(200, 100, 0));
  setPixel(26,4, CRGB(80, 40, 10));
  setPixel(25,6, CRGB(80, 40, 10));
  setPixel(26,6, CRGB(80, 40, 10));
  setPixel(27,6, CRGB(80, 40, 10));
  setPixel(24,7, CRGB(80, 40, 10));
  setPixel(25,7, CRGB(80, 40, 10));
  setPixel(26,7, CRGB(80, 40, 10));
  setPixel(27,7, CRGB(80, 40, 10));
  setPixel(28,7, CRGB(80, 40, 10));
  setPixel(26,5, CRGB(100, 150, 255));
}

void drawSun(int x0, int y0, uint8_t r, uint8_t g, uint8_t b) {
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      int px = x0 + dx, py = y0 + dy;
      if (py < 0 || py > 5) continue;
      int idx = XY(px, py);
      if (idx < 0) continue;
      if (dx == 0 && dy == 0) leds[idx] = CRGB(r, g, b);
      else leds[idx] = CRGB(r/2, g/2, b/2);
    }
  }
}
