// Ветер: дерево с «живой» кроной и улетающие листья.
#include "effects.h"

// =====================================================================
//  ВЕТЕР: живая крона + система листьев
// =====================================================================
// Отличие от остальных деревьев: здесь крона — не жёсткий монолитный
// шаблон, а "живой организм". Она поделена на несколько кластеров
// точек; каждый кластер гуляет к своей случайной цели независимо от
// остальных, поэтому крона не просто едет как единое целое, а реально
// меняет форму кадр за кадром (то вытягивается, то сжимается, то
// расходится по-разному в разные стороны).
//
// Отдельно — система листьев (эффект, не объект): одновременно на
// экране не больше MAX_WIND_LEAVES штук. Лист рождается подхваченным
// порывом ветра, летит, может осесть на земле, и в любом случае
// пропадает, как только уходит за левый край матрицы.

float windPhase = 0;
float windCrownX = 0.0f;
float windCrownY = 0.0f;
float windTargetX = 0.0f;
float windTargetY = 0.0f;
unsigned long nextCrownMove = 0;

#define WIND_CROWN_CLUSTERS 3
struct CrownCluster { float curX, curY, targetX, targetY; unsigned long nextMove; };
CrownCluster windClusters[WIND_CROWN_CLUSTERS];

// Точки кроны в координатах относительно baseX (та же форма, что и у
// обычного дерева), но каждая точка привязана к своему кластеру —
// именно разное поведение кластеров и делает крону "живой".
struct CrownPoint { int8_t dx, dy; uint8_t cluster; };
const CrownPoint windCrownShape[] = {
  {0,4,0},{1,4,0},{2,4,1},{3,4,1},{4,4,2},{5,4,2},
  {1,3,0},{2,3,1},{3,3,1},{4,3,2},
  {2,2,1},{3,2,1},
  {2,1,1},{3,1,1}
};
#define WIND_CROWN_POINTS (sizeof(windCrownShape) / sizeof(windCrownShape[0]))

#define MAX_WIND_LEAVES 5
enum LeafState { LEAF_NONE, LEAF_FLYING, LEAF_GROUNDED };
struct WindLeaf {
  LeafState state;
  float x, y, vx, vy, wob, wobSpeed, gravity;
  uint8_t life;
};
WindLeaf windLeaves[MAX_WIND_LEAVES];
unsigned long nextLeafSpawn = 0;

void resetWind() {
  windPhase = random(0, 628) / 100.0f;
  windCrownX = 0.0f;
  windCrownY = 0.0f;
  windTargetX = 0.0f;
  windTargetY = 0.0f;
  nextCrownMove = millis() + random(180, 800);
  nextLeafSpawn = millis() + random(120, 900);
  for (int i = 0; i < WIND_CROWN_CLUSTERS; i++) {
    windClusters[i].curX = 0; windClusters[i].curY = 0;
    windClusters[i].targetX = 0; windClusters[i].targetY = 0;
    windClusters[i].nextMove = millis() + random(150, 700);
  }
  for (int i = 0; i < MAX_WIND_LEAVES; i++) windLeaves[i].state = LEAF_NONE;
}

int activeWindLeaves() {
  int n = 0;
  for (int i = 0; i < MAX_WIND_LEAVES; i++) if (windLeaves[i].state != LEAF_NONE) n++;
  return n;
}

// Порыв ветра подхватывает новый лист с кроны. Если на экране уже
// MAX_WIND_LEAVES листьев — новый не появится, подождём, пока
// какой-то из старых не улетит за край или не истлеет.
void spawnWindLeaf(int fromX, int fromY) {
  if (activeWindLeaves() >= MAX_WIND_LEAVES) return;
  for (int i = 0; i < MAX_WIND_LEAVES; i++) {
    if (windLeaves[i].state == LEAF_NONE) {
      windLeaves[i].state = LEAF_FLYING;
      windLeaves[i].x = fromX + random(-2, 3) + (random(0, 100) / 100.0f);
      windLeaves[i].y = fromY + random(0, 3) + (random(0, 100) / 100.0f);
      float dir = (random(0, 2) == 0) ? -1.0f : 1.0f;
      windLeaves[i].vx = dir * (0.10f + random(0, 90) / 100.0f);
      windLeaves[i].vy = -0.60f + random(0, 121) / 100.0f;
      windLeaves[i].wob = random(0, 628) / 100.0f;
      windLeaves[i].wobSpeed = 0.08f + random(0, 22) / 100.0f;
      windLeaves[i].gravity = 0.01f + random(0, 8) / 100.0f;
      windLeaves[i].life = 190 + random(0, 66);
      return;
    }
  }
}

void drawWindTree(int baseX, CRGB trunkColor, Season season) {
  // Ствол неподвижен — "дышит" и деформируется только крона.
  int t1 = baseX + 2, t2 = baseX + 3;
  for (int y = 5; y < 8; y++) {
    setPixel(t1, y, trunkColor);
    setPixel(t2, y, trunkColor);
  }

  int crownX = (int)round(windCrownX);
  int crownY = (int)round(windCrownY);
  Season saved = currentSeason;
  currentSeason = season;

  for (uint8_t p = 0; p < WIND_CROWN_POINTS; p++) {
    const CrownPoint &pt = windCrownShape[p];
    CrownCluster &cl = windClusters[pt.cluster];
    int px = baseX + pt.dx + crownX + (int)round(cl.curX);
    int py = pt.dy + crownY + (int)round(cl.curY);
    if (season == SEASON_WINTER) {
      // Зимой — голые ветки: каждая вторая точка, верхушки в снегу.
      if (p % 2) continue;
      setPixel(px, py, pt.dy == 1 ? getLeafColor(SEASON_WINTER) : trunkColor);
    } else {
      // Цвет привязан к точке кроны, а не к экрану: при качании не мерцает.
      setPixel(px, py, seasonFoliage(baseX + pt.dx, pt.dy, CRGB(0, 150, 0)));
    }
  }
  currentSeason = saved;
}

void windEffect() {
  float windStrength = constrain(g_windSpeed, 0.0f, 20.0f);
  windPhase += 0.03f + windStrength * 0.008f;

  // Общая цель кроны — куда клонится дерево целиком.
  if (millis() >= nextCrownMove) {
    windTargetX = (float)random(-2, 2);
    windTargetY = (float)random(-1, 2);
    nextCrownMove = millis() + random(220, 900);
  }
  windCrownX += (windTargetX - windCrownX) * (0.045f + windStrength * 0.003f);
  windCrownY += (windTargetY - windCrownY) * (0.040f + windStrength * 0.002f);

  // Кластеры кроны гуляют независимо друг от друга — крона не едет
  // единым куском, а "дышит": меняет форму кадр за кадром.
  for (int i = 0; i < WIND_CROWN_CLUSTERS; i++) {
    if (millis() >= windClusters[i].nextMove) {
      windClusters[i].targetX = (float)random(-1, 2) * (0.5f + windStrength * 0.05f);
      windClusters[i].targetY = (float)random(-1, 2) * 0.4f;
      windClusters[i].nextMove = millis() + random(150, 500);
    }
    windClusters[i].curX += (windClusters[i].targetX - windClusters[i].curX) * 0.15f;
    windClusters[i].curY += (windClusters[i].targetY - windClusters[i].curY) * 0.15f;
  }

  // небо
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, CRGB(20, 30, 55));

  // Потоки ветра — у каждой полосы своя скорость, фаза и высота.
  int streaks = 6 + (int)constrain(windStrength, 0.0f, 10.0f);
  for (int i = 0; i < streaks; i++) {
    float t = windPhase * (1.0f + i * 0.11f) + i * 2.73f;
    int x = ((int)(t * (5.0f + (i % 4))) + i * 7) % (MATRIX_W + 12) - 6;
    int y = 1 + ((i * 3 + (int)(windPhase * 2.0f)) % 4);
    for (int k = 0; k < 4; k++) {
      int px = x - k;
      int idx = XY(px, y);
      if (idx >= 0 && px >= 0) {
        uint8_t br = 220 - k * 50;
        leds[idx] = CRGB(180, 200, 230).nscale8_video(br);
      }
    }
  }

  // земля
  for (int y = 6; y < 8; y++)
    for (int x = 0; x < MATRIX_W; x++) setPixel(x, y, seasonGround(x, y, CRGB(0, 110, 40)));

  // Только одно дерево справа, крона — "живая" (см. drawWindTree).
  drawWindTree(26, CRGB(100, 60, 20), currentSeason);

  // Спонтанные вылеты листьев — не больше MAX_WIND_LEAVES одновременно
  // (spawnWindLeaf сам это проверяет).
  if (millis() >= nextLeafSpawn) {
    nextLeafSpawn = millis() + random(120, (int)constrain(1200 - windStrength * 45.0f, 300.0f, 1200.0f));
    int crownX = (int)round(windCrownX);
    int crownY = (int)round(windCrownY);
    int fx = 28 + crownX + random(-1, 2);
    int fy = 2 + crownY + random(0, 3);
    spawnWindLeaf(fx, fy);
  }

  // Обновление и отрисовка летящих/лежащих листьев.
  for (int i = 0; i < MAX_WIND_LEAVES; i++) {
    CRGB leafColor = seasonFoliage(i * 3, i, CRGB(0, 150, 0));
    if (currentSeason == SEASON_WINTER) leafColor = getLeafColor(SEASON_WINTER);
    if (windLeaves[i].state == LEAF_NONE) continue;

    if (windLeaves[i].state == LEAF_FLYING) {
      windLeaves[i].wob += windLeaves[i].wobSpeed;
      windLeaves[i].vx += sin(windLeaves[i].wob) * 0.010f;
      windLeaves[i].vy += windLeaves[i].gravity;
      windLeaves[i].x += windLeaves[i].vx;
      windLeaves[i].y += windLeaves[i].vy;

      if (windLeaves[i].y >= 7.0f) {
        // Долетел до земли — оседает и постепенно гаснет.
        windLeaves[i].y = 7.0f;
        windLeaves[i].vy = 0;
        windLeaves[i].vx *= 0.25f;
        windLeaves[i].state = LEAF_GROUNDED;
      }
      if (windLeaves[i].life > 5) windLeaves[i].life -= 5; else windLeaves[i].life = 0;
    } else { // LEAF_GROUNDED
      windLeaves[i].x += windLeaves[i].vx;
      windLeaves[i].vx *= 0.9f;
      if (windLeaves[i].life > 2) windLeaves[i].life -= 2; else windLeaves[i].life = 0;
    }

    // Лист исчезает, как только уходит за левый край матрицы (или гаснет).
    if (windLeaves[i].x < 0.0f || windLeaves[i].life == 0 ||
        windLeaves[i].x > MATRIX_W + 2 || windLeaves[i].y < -2) {
      windLeaves[i].state = LEAF_NONE;
      continue;
    }

    int px = (int)round(windLeaves[i].x);
    int py = (int)round(windLeaves[i].y);
    if (px >= 0 && px < MATRIX_W && py >= 0 && py < MATRIX_H) {
      int idx = XY(px, py);
      if (idx >= 0) {
        CRGB particle = leafColor;
        particle.nscale8_video(windLeaves[i].life);
        leds[idx] = particle;
      }
    }
  }
}
