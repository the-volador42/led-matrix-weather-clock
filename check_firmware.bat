@echo off
chcp 65001 >nul
cd /d "%~dp0"
rem Проверка: компилируется ли прошивка для Wemos D1 mini (ESP8266).
rem Прошивать по-прежнему удобнее из Arduino IDE.
.tools\arduino\arduino-cli.exe --config-file .tools\arduino\arduino-cli.yaml compile -b esp8266:esp8266:d1_mini led_matrix_weather_clock
pause
