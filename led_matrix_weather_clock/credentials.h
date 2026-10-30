// =====================================================================
//  ЗНАЧЕНИЯ ПО УМОЛЧАНИЮ ДЛЯ ПАРОЛЕЙ
//  secrets.h теперь не обязателен: Wi-Fi, город и ключ погоды можно
//  ввести в веб-интерфейсе (они сохраняются во флеш-памяти часов).
//  Если secrets.h есть, его значения используются при первом запуске.
// =====================================================================
#pragma once

#if __has_include("secrets.h")
  #include "secrets.h"
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif
#ifndef OWM_API_KEY
  #define OWM_API_KEY ""
#endif
#ifndef OTA_PASSWORD
  #define OTA_PASSWORD "ledclock"   // пароль для прошивки по Wi-Fi
#endif
