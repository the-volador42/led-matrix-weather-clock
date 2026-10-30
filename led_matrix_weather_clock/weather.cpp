#include "weather.h"
#include "settings.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

int weatherEffect = FX_DAY_CYCLE;
unsigned long weatherUpdatedAt = 0;   // millis() последнего удачного обновления (0 — ещё не было)
String weatherError = "";
static bool forceUpdate = false;

void requestWeatherUpdate() { forceUpdate = true; }
int lastTemp = -100;
bool showTemp = false;
unsigned long lastTempShow = 0;

void setWeatherEffect(int id) {
  if (id == weatherEffect) return;
  weatherEffect = id;
  resetEffect(id);
}

void runWeatherEffect() {
  if (showTemp && lastTemp != -100) {
    CRGB color;
    if      (lastTemp < -10) color = CRGB::Blue;
    else if (lastTemp < 0)   color = CRGB(0,100,255);
    else if (lastTemp < 10)  color = CRGB::Cyan;
    else if (lastTemp < 20)  color = CRGB::Green;
    else if (lastTemp < 25)  color = CRGB::Yellow;
    else if (lastTemp < 30)  color = CRGB::Orange;
    else                     color = CRGB::Red;
    drawNumber(lastTemp, color);
    if (millis() - lastTempShow > TEMP_DISPLAY_TIME) showTemp = false;
    return;
  }
  runEffect(weatherEffect);
}

// Какой эффект показать при такой погоде.
static int pickEffect(int weatherId, float windSpeed, bool isNight, bool nearSunrise, bool nearSunset) {
  if      (nearSunrise)                              return FX_DAWN;
  else if (nearSunset)                               return FX_SUNSET;
  else if (isNight)                                  return FX_NIGHT_CYCLE;
  else if (weatherId >= 200 && weatherId <= 232)     return FX_RAIN_STORM;   // гроза
  else if (weatherId >= 300 && weatherId <= 321)     return FX_DRIZZLE;      // морось
  else if (weatherId >= 500 && weatherId <= 501)     return FX_RAIN_LIGHT;
  else if (weatherId >= 502 && weatherId <= 504)     return FX_RAIN_HEAVY;
  else if (weatherId == 511)                         return FX_RAIN_LIGHT;
  else if (weatherId >= 520 && weatherId <= 531)     return FX_RAIN_HEAVY;
  else if (weatherId >= 600 && weatherId <= 601)     return FX_SNOW_LIGHT;
  else if (weatherId >= 602 && weatherId <= 622)     return FX_SNOW_HEAVY;
  else if (weatherId == 800) {                                               // ясно
    if (windSpeed >= WIND_SPEED_THRESHOLD) return FX_WIND;
    else                                   return FX_DAY_CYCLE;
  }
  else if (weatherId >= 801) {                                               // облачно
    if (windSpeed >= WIND_SPEED_THRESHOLD) return FX_WIND;
    else                                   return FX_OVERCAST;
  }
  return FX_DAY_CYCLE;
}

// Запрос погоды. Возвращает true, если погода получена и разобрана.
bool fetchWeatherAndPickEffect() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (settings.owmKey.length() == 0) { weatherError = "не задан ключ OpenWeatherMap"; return false; }

  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();

  String cityEncoded = settings.city;
  cityEncoded.replace(",", "%2C");
  cityEncoded.replace(" ", "%20");
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + cityEncoded +
               "&appid=" + settings.owmKey + "&units=" + String(OWM_UNITS) + "&lang=ru";

  http.setTimeout(3000);
  if (!http.begin(client, url)) { weatherError = "не удалось начать запрос"; return false; }

  bool ok = false;
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();

    // Берём из ответа только нужные поля: так документ занимает пару сотен
    // байт в куче, а не 4 КБ на стеке (у ESP8266 весь стек — 4 КБ).
    JsonDocument filter;
    filter["weather"][0]["id"] = true;
    filter["main"]["temp"]     = true;
    filter["wind"]["speed"]    = true;
    filter["sys"]["sunrise"]   = true;
    filter["sys"]["sunset"]    = true;
    filter["dt"]               = true;
    filter["clouds"]["all"]    = true;
    filter["rain"]["1h"]       = true;
    filter["snow"]["1h"]       = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (err) {
      weatherError = String("ответ не разобран: ") + err.c_str();
      Serial.println(weatherError);
    } else {
      int   weatherId  = doc["weather"][0]["id"] | 800;
      float temp       = doc["main"]["temp"]     | 0.0f;
      float windSpeed  = doc["wind"]["speed"]    | 0.0f;
      long  sunrise    = doc["sys"]["sunrise"]   | 0L;
      long  sunset     = doc["sys"]["sunset"]    | 0L;
      long  currentTime= doc["dt"]               | 0L;
      uint8_t clouds   = doc["clouds"]["all"]    | 0;
      float rain1h     = doc["rain"]["1h"]       | 0.0f;
      float snow1h     = doc["snow"]["1h"]       | 0.0f;

      g_windSpeed  = windSpeed;
      g_sunrise    = sunrise;
      g_sunset     = sunset;
      g_clouds     = clouds;
      g_rainVolume = rain1h;
      g_snowVolume = snow1h;
      g_weatherId  = weatherId;
      lastTemp     = round(temp);

      bool isNight    = (currentTime < sunrise) || (currentTime > sunset);
      bool nearSunrise= abs((long)(currentTime - sunrise)) < 1800;
      bool nearSunset = abs((long)(currentTime - sunset))  < 1800;

      moonPhase = getMoonPhase(time(nullptr));

      setWeatherEffect(pickEffect(weatherId, windSpeed, isNight, nearSunrise, nearSunset));

      showTemp = true;
      lastTempShow = millis();
      ok = true;
      weatherError = "";
      weatherUpdatedAt = millis();
      Serial.printf("Weather: id=%d t=%d wind=%.1f\r\n", weatherId, lastTemp, windSpeed);
    }
  } else {
    Serial.printf("HTTP error: %d\r\n", httpCode);
    if (httpCode == 401)      weatherError = "неверный ключ OpenWeatherMap";
    else if (httpCode == 404) weatherError = "город не найден";
    else if (httpCode < 0)    weatherError = "нет связи с сервером погоды";
    else                      weatherError = "ошибка сервера погоды " + String(httpCode);
  }

  if (!ok) {
    // Погоды нет — хотя бы ночью показываем ночные сцены.
    time_t now = time(nullptr);
    if (now > 100000) {
      struct tm *t = localtime(&now);
      if (t->tm_hour >= 21 || t->tm_hour < 6) {
        setWeatherEffect(FX_NIGHT_CYCLE);
      }
      moonPhase = getMoonPhase(now);
    }
  }

  http.end();
  return ok;
}

// Вызывать в каждом loop(): сама решает, когда пора обновить погоду.
// Удачно — следующий раз через WEATHER_CHECK_INTERVAL, с ошибкой — через WEATHER_RETRY_INTERVAL.
void updateWeather(bool networkReady) {
  static bool attempted = false;
  static bool lastOk = false;
  static unsigned long lastAttempt = 0;
  if (!networkReady) return;
  unsigned long interval = lastOk ? WEATHER_CHECK_INTERVAL : WEATHER_RETRY_INTERVAL;
  if (attempted && !forceUpdate && millis() - lastAttempt < interval) return;
  forceUpdate = false;
  attempted = true;
  lastAttempt = millis();
  lastOk = fetchWeatherAndPickEffect();
}
