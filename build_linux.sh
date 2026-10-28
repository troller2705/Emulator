#!/usr/bin/env bash
set -e

VCPKG_PATH="${VCPKG_ROOT:-$HOME/vcpkg}"

echo "[1/3] Configuring CMake for Standalone Linux Executable (x64-linux)..."
cmake -B build-release-linux \
    -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_STATIC_EXE=ON \
    -DVCPKG_TARGET_TRIPLET=x64-linux \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_PATH/scripts/buildsystems/vcpkg.cmake"

echo "[2/3] Building Single Executable..."
cmake --build build-release-linux --config Release -j "$(nproc)"

echo "[3/3] Packaging into dist/..."
mkdir -p dist
cp build-release-linux/EmulatorLauncher dist/EmulatorLauncher
strip dist/EmulatorLauncher

echo "Done! Standalone binary created at: dist/EmulatorLauncher"