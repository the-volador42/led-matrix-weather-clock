// =====================================================================
//  ВЕБ-ИНТЕРФЕЙС: http://ledclock.local или по IP-адресу часов
//  Страница — web_page.h, данные — JSON по адресам /api/...
// =====================================================================
#pragma once
#include <Arduino.h>

extern bool webInterfaceEnabled;

void setupWebRoutes();      // один раз в setup(), после loadSettings()
void startWebInterface();   // запускается сразу: работает и в режиме настройки Wi-Fi
void handleWebClient();     // в loop(): обработать запросы браузера
