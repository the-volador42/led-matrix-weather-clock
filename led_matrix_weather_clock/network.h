// =====================================================================
//  WI-FI, ТОЧНОЕ ВРЕМЯ, НАСТРОЙКА С ТЕЛЕФОНА, ПРОШИВКА ПО WI-FI
//
//  Нет сохранённой сети (или подключиться не вышло за PORTAL_AFTER_MS) —
//  часы раздают открытую сеть «LED-Clock-xxxx». Подключись к ней телефоном:
//  откроется страница настройки (или зайди на http://192.168.4.1).
//  После подключения к домашней сети часы показывают свой IP бегущей
//  строкой и доступны по адресу http://ledclock.local
// =====================================================================
#pragma once
#include <Arduino.h>

enum WifiState { WIFI_IDLE, WIFI_CONNECTING, WIFI_GET_TIME, WIFI_READY };

extern WifiState wifiState;

void handleWiFiConnection();   // в каждом loop()
bool networkReady();           // Wi-Fi есть и время известно
bool portalActive();           // часы сейчас раздают свою сеть для настройки
String apName();               // имя этой сети: LED-Clock-xxxx

// Сохранить новую сеть и подключиться к ней.
void connectToWifi(const String& ssid, const String& pass);
// Применить часовой пояс из настроек.
void applyTimezone();
