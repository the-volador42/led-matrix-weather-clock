#include "state.h"
#include "config.h"

long    g_sunrise = 0, g_sunset = 0;
float   g_windSpeed = 0.0f;
float   g_rainVolume = 0.0f;
float   g_snowVolume = 0.0f;
uint8_t g_clouds = 0;
int     g_weatherId = 800;
float   moonPhase = 0.5f;
float   g_alarmDawnProgress = -1.0f;

String  displayText = "ПРИВЕТ";
uint8_t textChars[128];
int     textLen = 0, scrollPos = MATRIX_W;
