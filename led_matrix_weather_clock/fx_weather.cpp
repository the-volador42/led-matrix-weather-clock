// Погодные эффекты: дождь, гроза, снег, солнце, облачность.
#include "effects.h"

// =====================================================================
//  ДОЖДЬ, МОЛНИЯ
// =====================================================================
#define RAIN_DROPS 24
float rainY[RAIN_DROPS]; int rainX[RAIN_DROPS]; float rainSpeed[RAIN_DROPS];
bool rainInit1 = false;
void resetRain1() { rainInit1 = false; }
void rainEffect1() {
  if (!rainInit1) {
    for (int i = 0; i < RAIN_DROPS; i++) {
      rainX[i] = random(0, MATRIX_W); rainY[i] = random(0, MATRIX_H - 1);
      rainSpeed[i] = random(20, 60) / 100.0;
    }
    rainInit1 = true;
  }
  fadeToBlackBy(leds, NUM_LEDS, 80);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 7, seasonGround(x, 7, CRGB(0, 160, 0)));
  drawHouse();
  for (int i = 0; i < RAIN_DROPS; i++) {
    rainY[i] += rainSpeed[i];
    if (rainY[i] >= MATRIX_H - 1) {
      int sx = rainX[i];
      if (sx > 0 && !isHouse(sx - 1, 6)) setPixel(sx - 1, 6, CRGB(150, 200, 255));
      if (sx < MATRIX_W - 1 && !isHouse(sx + 1, 6)) setPixel(sx + 1, 6, CRGB(150, 200, 255));
      rainY[i] = 0; rainX[i] = random(0, MATRIX_W); rainSpeed[i] = random(20, 60) / 100.0;
    } else {
      int px = rainX[i], py = (int)rainY[i];
      if (!isHouse(px, py) && px >= 0 && px < MATRIX_W) setPixel(px, py, CRGB(40, 60, 140));
    }
  }
}

unsigned long lastLightningCheck = 0;
bool lightningActive = false;
unsigned long lightningStart = 0;
float lightningPoints[5][2];

void generateLightning() {
  float startX = random(5, MATRIX_W - 5);
  lightningPoints[0][0] = startX; lightningPoints[0][1] = 0.0;
  for (int i = 1; i < 4; i++) {
    lightningPoints[i][0] = constrain(lightningPoints[i-1][0] + random(-3, 4), 0.0f, (float)(MATRIX_W-1));
    lightningPoints[i][1] = lightningPoints[i-1][1] + 1.0;
  }
  lightningPoints[4][0] = constrain(lightningPoints[3][0] + random(-2, 3), 0.0f, (float)(MATRIX_W-1));
  lightningPoints[4][1] = 4.0;
}

void drawLightning() {
  for (int i = 0; i < 4; i++) {
    int x0 = lightningPoints[i][0], y0 = lightningPoints[i][1];
    int x1 = lightningPoints[i+1][0], y1 = lightningPoints[i+1][1];
    int dx = abs(x1-x0), dy = -abs(y1-y0);
    int sx = x0<x1 ? 1 : -1, sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2;
    for (;;) {
      if (x0>=0 && x0<MATRIX_W && y0>=0 && y0<5) {
        setPixel(x0,y0, CRGB::White);
        if (x0>0) setPixel(x0-1,y0, CRGB(100,100,100));
        if (x0<MATRIX_W-1) setPixel(x0+1,y0, CRGB(100,100,100));
      }
      if (x0==x1 && y0==y1) break;
      e2 = 2*err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
}

bool rainInit2 = false;
void resetRain2() { rainInit2 = false; lightningActive = false; }
void rainEffect2() {
  if (!rainInit2) {
    for (int i = 0; i < RAIN_DROPS; i++) {
      rainX[i] = random(0, MATRIX_W); rainY[i] = random(0, MATRIX_H - 1);
      rainSpeed[i] = random(40, 90) / 100.0;
    }
    rainInit2 = true;
    lastLightningCheck = millis();
  }
  if (!lightningActive) {
    if (millis() - lastLightningCheck > LIGHTNING_INTERVAL) {
      generateLightning();
      lightningActive = true;
      lightningStart = millis();
      lastLightningCheck = millis();
    }
  } else if (millis() - lightningStart > LIGHTNING_DURATION) lightningActive = false;

  fadeToBlackBy(leds, NUM_LEDS, 110);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 7, seasonGround(x, 7, CRGB(0, 160, 0)));
  drawHouse();
  for (int i = 0; i < RAIN_DROPS; i++) {
    rainY[i] += rainSpeed[i];
    if (rainY[i] >= MATRIX_H - 1) {
      int sx = rainX[i];
      if (sx > 0 && !isHouse(sx - 1, 6)) setPixel(sx - 1, 6, CRGB(150, 200, 255));
      if (sx < MATRIX_W - 1 && !isHouse(sx + 1, 6)) setPixel(sx + 1, 6, CRGB(150, 200, 255));
      rainY[i] = 0; rainX[i] = random(0, MATRIX_W); rainSpeed[i] = random(40, 90) / 100.0;
    } else {
      int px = rainX[i], py = (int)rainY[i];
      if (!isHouse(px, py) && px >= 0 && px < MATRIX_W) setPixel(px, py, CRGB(50, 70, 160));
    }
  }
  if (lightningActive) drawLightning();
}

bool rainInit3 = false;
void resetRain3() { rainInit3 = false; }
void rainEffect3() {
  if (!rainInit3) {
    for (int i = 0; i < RAIN_DROPS / 2; i++) {
      rainX[i] = random(0, MATRIX_W); rainY[i] = random(0, MATRIX_H - 1);
      rainSpeed[i] = random(8, 20) / 100.0;
    }
    rainInit3 = true;
  }
  fadeToBlackBy(leds, NUM_LEDS, 50);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 7, seasonGround(x, 7, CRGB(0, 160, 0)));
  drawHouse();
  for (int i = 0; i < RAIN_DROPS / 2; i++) {
    rainY[i] += rainSpeed[i];
    if (rainY[i] >= MATRIX_H - 1) {
      int sx = rainX[i];
      if (sx > 0 && !isHouse(sx - 1, 6)) setPixel(sx - 1, 6, CRGB(100, 160, 220));
      if (sx < MATRIX_W - 1 && !isHouse(sx + 1, 6)) setPixel(sx + 1, 6, CRGB(100, 160, 220));
      rainY[i] = 0; rainX[i] = random(0, MATRIX_W); rainSpeed[i] = random(8, 20) / 100.0;
    } else {
      int px = rainX[i], py = (int)rainY[i];
      if (!isHouse(px, py) && px >= 0 && px < MATRIX_W) setPixel(px, py, CRGB(25, 40, 90));
    }
  }
}

#define RAIN_DROPS_HEAVY 30
float rainYheavy[RAIN_DROPS_HEAVY]; int rainXheavy[RAIN_DROPS_HEAVY]; float rainSpeedHeavy[RAIN_DROPS_HEAVY];
bool rainInit4 = false;
void resetRain4() { rainInit4 = false; }
void rainEffect4() {
  if (!rainInit4) {
    for (int i = 0; i < RAIN_DROPS_HEAVY; i++) {
      rainXheavy[i] = random(0, MATRIX_W); rainYheavy[i] = random(0, MATRIX_H - 1);
      rainSpeedHeavy[i] = random(50, 100) / 100.0;
    }
    rainInit4 = true;
  }
  fadeToBlackBy(leds, NUM_LEDS, 130);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 7, seasonGround(x, 7, CRGB(0, 160, 0)));
  drawHouse();
  for (int i = 0; i < RAIN_DROPS_HEAVY; i++) {
    rainYheavy[i] += rainSpeedHeavy[i];
    if (rainYheavy[i] >= MATRIX_H - 1) {
      int sx = rainXheavy[i];
      if (sx > 0 && !isHouse(sx - 1, 6)) setPixel(sx - 1, 6, CRGB(150, 200, 255));
      if (sx < MATRIX_W - 1 && !isHouse(sx + 1, 6)) setPixel(sx + 1, 6, CRGB(150, 200, 255));
      rainYheavy[i] = 0; rainXheavy[i] = random(0, MATRIX_W); rainSpeedHeavy[i] = random(50, 100) / 100.0;
    } else {
      int px = rainXheavy[i], py = (int)rainYheavy[i];
      if (!isHouse(px, py) && px >= 0 && px < MATRIX_W) setPixel(px, py, CRGB(60, 80, 180));
    }
  }
}

// =====================================================================
//  СНЕГ
// =====================================================================
#define SNOW_FLAKES 20
float snowY[SNOW_FLAKES], snowX[SNOW_FLAKES], snowSpeed[SNOW_FLAKES], snowDrift[SNOW_FLAKES];
bool snowInit1 = false;
uint8_t snowDrifts1[MATRIX_W] = {0};
unsigned long lastSnowGen1 = 0;
void resetSnow1() { snowInit1 = false; memset(snowDrifts1, 0, sizeof(snowDrifts1)); lastSnowGen1 = 0; }
void snowEffect1() {
  if (!snowInit1) {
    for (int i = 0; i < SNOW_FLAKES; i++) {
      snowX[i] = random(0, MATRIX_W * 10) / 10.0; snowY[i] = random(0, MATRIX_H);
      snowSpeed[i] = random(5, 15) / 100.0; snowDrift[i] = random(-5, 5) / 100.0;
    }
    snowInit1 = true; lastSnowGen1 = millis();
  }
  fadeToBlackBy(leds, NUM_LEDS, 40);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 7, CRGB(200, 210, 230));
  if (millis() - lastSnowGen1 > SNOW_GEN_INTERVAL) {
    lastSnowGen1 = millis();
    int r = random(0, 100), x = random(0, MATRIX_W);
    if (r < 50) snowDrifts1[x] = 1;
    else if (r < 80) snowDrifts1[x] = 2;
    else if (r < 90) snowDrifts1[x] = 3;
    if (random(0, 100) < 20) { int tx = random(0, MATRIX_W); if (snowDrifts1[tx] > 0) snowDrifts1[tx]--; }
  }
  for (int x = 0; x < MATRIX_W; x++) {
    if (snowDrifts1[x] > 0) {
      setPixel(x, 6, CRGB(220, 225, 240));
      if (snowDrifts1[x] >= 2) setPixel(x, 5, CRGB(220, 225, 240));
      if (snowDrifts1[x] >= 3) setPixel(x, 4, CRGB(220, 225, 240));
    }
  }
  drawHouse();
  for (int i = 0; i < SNOW_FLAKES; i++) {
    int px = (int)snowX[i], py = (int)snowY[i];
    if (!isHouse(px, py) && px >= 0 && px < MATRIX_W && py >= 0) setPixel(px, py, CRGB(200, 210, 230));
    snowY[i] += snowSpeed[i]; snowX[i] += sin(snowY[i] * 0.5) * 0.08 + snowDrift[i];
    if (snowX[i] < 0) snowX[i] = MATRIX_W - 1;
    if (snowX[i] >= MATRIX_W) snowX[i] = 0;
    if (snowY[i] >= MATRIX_H) { snowY[i] = 0; snowX[i] = random(0, MATRIX_W * 10) / 10.0; snowSpeed[i] = random(5, 15) / 100.0; }
  }
}

uint8_t snowDrifts2[MATRIX_W] = {0};
unsigned long lastSnowGen2 = 0;
bool snowInit2 = false;
void resetSnow2() { snowInit2 = false; memset(snowDrifts2, 0, sizeof(snowDrifts2)); lastSnowGen2 = 0; }
void snowEffect2() {
  if (!snowInit2) {
    for (int i = 0; i < SNOW_FLAKES; i++) {
      snowX[i] = random(0, MATRIX_W * 10) / 10.0; snowY[i] = random(0, MATRIX_H);
      snowSpeed[i] = random(15, 30) / 100.0;
    }
    snowInit2 = true; lastSnowGen2 = millis();
  }
  fadeToBlackBy(leds, NUM_LEDS, 60);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 7, CRGB(200, 210, 230));
  if (millis() - lastSnowGen2 > SNOW_GEN_INTERVAL) {
    lastSnowGen2 = millis();
    int r = random(0, 100), x = random(0, MATRIX_W);
    if (r < 50) snowDrifts2[x] = 1;
    else if (r < 80) snowDrifts2[x] = 2;
    else if (r < 90) snowDrifts2[x] = 3;
    if (random(0, 100) < 20) { int tx = random(0, MATRIX_W); if (snowDrifts2[tx] > 0) snowDrifts2[tx]--; }
  }
  for (int x = 0; x < MATRIX_W; x++) {
    if (snowDrifts2[x] > 0) {
      setPixel(x, 6, CRGB(220, 225, 240));
      if (snowDrifts2[x] >= 2) setPixel(x, 5, CRGB(220, 225, 240));
      if (snowDrifts2[x] >= 3) setPixel(x, 4, CRGB(220, 225, 240));
    }
  }
  drawHouse();
  for (int i = 0; i < SNOW_FLAKES; i++) {
    int px = (int)snowX[i], py = (int)snowY[i];
    if (!isHouse(px, py) && px >= 0 && px < MATRIX_W && py >= 0) setPixel(px, py, CRGB(220, 225, 240));
    snowY[i] += snowSpeed[i]; snowX[i] += 0.2;
    if (snowX[i] >= MATRIX_W) snowX[i] = 0;
    if (snowY[i] >= MATRIX_H) { snowY[i] = 0; snowX[i] = random(0, MATRIX_W * 10) / 10.0; snowSpeed[i] = random(15, 30) / 100.0; }
  }
}

// =====================================================================
//  СОЛНЦЕ, ОБЛАЧНОСТЬ
// =====================================================================
#define NUM_CLOUDS_SUN 3
float sunCloudBaseX[NUM_CLOUDS_SUN] = {2.0, 10.0, 18.0};
float sunCloudBaseY[NUM_CLOUDS_SUN] = {0.8, 0.5, 0.9};
float sunCloudPhaseX[NUM_CLOUDS_SUN], sunCloudPhaseY[NUM_CLOUDS_SUN];
const float sunCloudSpeedX[NUM_CLOUDS_SUN] = {0.015, 0.012, 0.010};
const float sunCloudSpeedY[NUM_CLOUDS_SUN] = {0.020, 0.018, 0.025};
const byte sunCloudBitmap[3][4] = {{0,1,1,0},{1,1,1,1},{1,1,1,1}};

// Кустики для эффекта 7 (Солнце).
// При каждом новом запуске эффекта их становится 1-3,
// а позиции выбираются заново.
#define SUN_BUSHES_MAX 3
int sunBushCount = 1;
int sunBushX[SUN_BUSHES_MAX] = {4, 15, 26};
byte sunBushHeight[SUN_BUSHES_MAX] = {2, 2, 2};

void resetSun2() {
  for (int i=0;i<NUM_CLOUDS_SUN;i++){
    sunCloudPhaseX[i]=random(0,628)/100.0;
    sunCloudPhaseY[i]=random(0,628)/100.0;
  }

  // Новый набор кустиков при каждом вызове resetSun2().
  sunBushCount = random(1, SUN_BUSHES_MAX + 1);
  for (int i = 0; i < SUN_BUSHES_MAX; i++) {
    sunBushX[i] = random(1, MATRIX_W - 2);
    sunBushHeight[i] = random(1, 3);
  }
}

void sunEffect2() {
  for (int y = 0; y <= 5; y++) for (int x = 0; x < MATRIX_W; x++) setPixel(x,y, CRGB(0,120,220));
  for (int i=0;i<NUM_CLOUDS_SUN;i++) {
    float offX=sin(sunCloudPhaseX[i])*2.0, offY=sin(sunCloudPhaseY[i])*1.2;
    int cx=(int)(sunCloudBaseX[i]+offX), cy=(int)(sunCloudBaseY[i]+offY);
    for (int r=0;r<3;r++) for (int c=0;c<4;c++) if(sunCloudBitmap[r][c]) {
      int dx=cx+c, dy=cy+r;
      if(dx>=0 && dx<MATRIX_W && dy>=0 && dy<6) setPixel(dx,dy, CRGB(220,225,240));
    }
    sunCloudPhaseX[i]+=sunCloudSpeedX[i]; sunCloudPhaseY[i]+=sunCloudSpeedY[i];
    if(sunCloudPhaseX[i]>6.283)sunCloudPhaseX[i]-=6.283;
    if(sunCloudPhaseY[i]>6.283)sunCloudPhaseY[i]-=6.283;
  }
  for (int y=0;y<3;y++) for (int x=25;x<29;x++) setPixel(x,y, CRGB(255,230,0));
  for (int y=6;y<8;y++) for (int x=0;x<MATRIX_W;x++) setPixel(x,y, seasonGround(x,y, CRGB(0,180,40)));

  // Кустики стоят у нижнего края, поверх травы.
  // Форма маленькая, чтобы на матрице 32x8 они читались именно как кусты.
  for (int b = 0; b < sunBushCount; b++) {
    int x = sunBushX[b];
    int h = sunBushHeight[b];

    // Нижняя часть куста.
    for (int dx = -1; dx <= 1; dx++) {
      int px = x + dx;
      int py = 7;
      if (px >= 0 && px < MATRIX_W) setPixel(px, py, seasonFoliage(px, py, CRGB(0, 95, 20)));
    }

    // Верхушка и небольшие боковые ветки.
    if (h >= 1) setPixel(x, 6, seasonFoliage(x, 6, CRGB(0, 125, 25)));
    if (h >= 2) {
      if (x > 0) setPixel(x - 1, 6, seasonFoliage(x - 1, 6, CRGB(0, 110, 22)));
      if (x < MATRIX_W - 1) setPixel(x + 1, 6, seasonFoliage(x + 1, 6, CRGB(0, 110, 22)));
      setPixel(x, 5, seasonFoliage(x, 5, CRGB(0, 145, 30)));
    }
  }
}

#define NUM_CLOUDS_OVERCAST 5
float overcastCloudBaseX[NUM_CLOUDS_OVERCAST] = {2.0,8.0,14.0,20.0,26.0};
float overcastCloudBaseY[NUM_CLOUDS_OVERCAST] = {0.8,1.0,0.5,1.2,0.9};
float overcastCloudPhaseX[NUM_CLOUDS_OVERCAST], overcastCloudPhaseY[NUM_CLOUDS_OVERCAST];
const float overcastCloudSpeedX[NUM_CLOUDS_OVERCAST] = {0.008,0.010,0.007,0.009,0.006};
const float overcastCloudSpeedY[NUM_CLOUDS_OVERCAST] = {0.012,0.015,0.010,0.013,0.011};
const byte overcastCloudBitmap[3][4] = {{0,1,1,0},{1,1,1,1},{1,1,1,1}};
void resetSunCloudy() { for(int i=0;i<NUM_CLOUDS_OVERCAST;i++){ overcastCloudPhaseX[i]=random(0,628)/100.0; overcastCloudPhaseY[i]=random(0,628)/100.0; } }
void sunCloudyEffect() {
  for(int y=0;y<=5;y++) for(int x=0;x<MATRIX_W;x++) setPixel(x,y, CRGB(10,30,90));
  for(int i=0;i<NUM_CLOUDS_OVERCAST-1;i++){
    float offX=sin(overcastCloudPhaseX[i])*1.5, offY=sin(overcastCloudPhaseY[i])*1.2;
    int cx=(int)(overcastCloudBaseX[i]+offX), cy=(int)(overcastCloudBaseY[i]+offY);
    for(int r=0;r<3;r++) for(int c=0;c<4;c++) if(overcastCloudBitmap[r][c]){
      int dx=cx+c, dy=cy+r;
      if(dx>=0 && dx<MATRIX_W && dy>=0 && dy<6) setPixel(dx,dy, CRGB(140,150,170));
    }
    overcastCloudPhaseX[i]+=overcastCloudSpeedX[i]; overcastCloudPhaseY[i]+=overcastCloudSpeedY[i];
    if(overcastCloudPhaseX[i]>6.283)overcastCloudPhaseX[i]-=6.283;
    if(overcastCloudPhaseY[i]>6.283)overcastCloudPhaseY[i]-=6.283;
  }
  int i=NUM_CLOUDS_OVERCAST-1;
  float offX=sin(overcastCloudPhaseX[i])*1.5, offY=sin(overcastCloudPhaseY[i])*1.2;
  int cx=(int)(overcastCloudBaseX[i]+offX), cy=(int)(overcastCloudBaseY[i]+offY);
  for(int r=0;r<3;r++) for(int c=0;c<4;c++) if(overcastCloudBitmap[r][c]){
    int dx=cx+c, dy=cy+r;
    if(dx>=0 && dx<MATRIX_W && dy>=0 && dy<6) setPixel(dx,dy, CRGB(200,180,100));
  }
  overcastCloudPhaseX[i]+=overcastCloudSpeedX[i]; overcastCloudPhaseY[i]+=overcastCloudSpeedY[i];
  if(overcastCloudPhaseX[i]>6.283)overcastCloudPhaseX[i]-=6.283;
  if(overcastCloudPhaseY[i]>6.283)overcastCloudPhaseY[i]-=6.283;
  for(int y=6;y<8;y++) for(int x=0;x<MATRIX_W;x++) setPixel(x,y, seasonGround(x,y, CRGB(0,100,20)));
}
