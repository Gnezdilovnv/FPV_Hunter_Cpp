@echo off
chcp 65001 > nul
echo ========================================
echo  FPV HUNTER PRO v5.0 - BUILD WINDOWS
echo ========================================
echo.

echo [1/4] Проверка MSYS2...
if not exist "C:\msys64" (
    echo ❌ MSYS2 не найден!
    echo Скачайте: https://www.msys2.org/
    pause
    exit /b 1
)

echo [2/4] Проверка Qt6...
if not exist "C:\Qt\6.5.0\mingw_64" (
    echo ❌ Qt6 не найден!
    echo Скачайте: https://www.qt.io/download
    pause
    exit /b 1
)

echo [3/4] Сборка...
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.5.0/mingw_64
make -j4

echo [4/4] Готово!
echo 📁 Файл: build/FPV_Hunter_Pro.exe
pause
