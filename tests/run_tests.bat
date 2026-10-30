@echo off
chcp 65001 >nul
cd /d "%~dp0"
set FAIL=0
python -m ziglang c++ -std=c++17 -O1 alarm_test.cpp -o alarm_test.exe 2>nul && alarm_test.exe || set FAIL=1
echo.
python -m ziglang c++ -std=c++17 -O1 schedule_test.cpp -o schedule_test.exe 2>nul && schedule_test.exe || set FAIL=1
echo.
if %FAIL%==1 (echo ЕСТЬ ОШИБКИ) else (echo ВСЕ ТЕСТЫ ПРОШЛИ)
pause
