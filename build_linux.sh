#!/bin/bash
echo "========================================"
echo "  FPV HUNTER PRO v5.0 - BUILD LINUX"
echo "========================================"
echo ""

echo "[1/3] Установка зависимостей..."
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev libiio-dev

echo "[2/3] Сборка..."
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

echo "[3/3] Готово!"
echo "📁 Файл: build/FPV_Hunter_Pro"
