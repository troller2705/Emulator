@echo off
setlocal

:: Point to your vcpkg root (adjust if vcpkg is installed elsewhere, or rely on VCPKG_ROOT env var)
if "%VCPKG_ROOT%"=="" (
    set "VCPKG_ROOT=C:\vcpkg"
)

echo [1/3] Configuring CMake for Standalone Windows Executable (x64-mingw-static)...
cmake -B build-release-win ^
    -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_STATIC_EXE=ON ^
    -DVCPKG_TARGET_TRIPLET=x64-mingw-static ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake"

if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [2/3] Building Single Executable...
cmake --build build-release-win --config Release -j

if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [3/3] Packaging into dist/...
if not exist dist mkdir dist
copy /Y "build-release-win\EmulatorLauncher.exe" "dist\EmulatorLauncher.exe"

echo Done! Standalone binary created at: dist\EmulatorLauncher.exe