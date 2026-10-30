// =====================================================================
//  ОБЪЕКТЫ СЦЕНЫ: сезон, деревья, домик, солнце
// =====================================================================
#pragma once
#include "matrix.h"

// ---------- Время года ----------
enum Season : uint8_t {
  SEASON_SPRING = 0,
  SEASON_SUMMER = 1,
  SEASON_AUTUMN = 2,
  SEASON_WINTER = 3
};
extern Season currentSeason;
CRGB getLeafColor(Season season);       // цвет одного листа / лепестка / снежинки
Season seasonForMonth(int month);       // 1..12 -> сезон

// Сезонный цвет точки. base — «летний» цвет, который рисовал эффект;
// результат сохраняет его яркость. Летом возвращается base без изменений.
CRGB seasonGround(int x, int y, CRGB base);    // трава / земля
CRGB seasonFoliage(int x, int y, CRGB base);   // листва кустов и крон

// ---------- Реестр «твёрдых» объектов ----------
// Объекты (дерево, домик, звезда) не должны появляться вплотную друг к другу.
#define MAX_SCENE_OBJECTS 24   // домик + дерево + до 16 звёзд, с запасом
struct SceneObject {
  bool used;
  int x0, y0, x1, y1; // ограничивающий прямоугольник (включительно)
  uint8_t margin;     // минимальный зазор до других объектов
};
extern SceneObject sceneObjects[MAX_SCENE_OBJECTS];

void clearSceneObjects();
bool boxesTooClose(int ax0, int ay0, int ax1, int ay1,
                   int bx0, int by0, int bx1, int by1, int margin);
bool canPlaceObject(int x0, int y0, int x1, int y1, int margin = 1);
int  registerObject(int x0, int y0, int x1, int y1, uint8_t margin = 1);
void unregisterObject(int id);

// ---------- Рисование ----------
void drawTree(int baseX, int bend, CRGB trunkColor, Season season = currentSeason);
void drawTrees(Season season = currentSeason);  // два дерева: слева и справа
bool isHouse(int x, int y);
void drawHouse();
void drawSun(int x0, int y0, uint8_t r, uint8_t g, uint8_t b);
