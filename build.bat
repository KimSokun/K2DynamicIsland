@echo off
setlocal EnableDelayedExpansion
echo ===================================================
echo   K2 Dynamic Island - Windows Build Script
echo ===================================================

cd /d "%~dp0"

taskkill /f /im K2DynamicIsland.exe 2>nul
taskkill /f /im K2DynamicIsland-Setup.exe 2>nul

if not exist bin mkdir bin
if not exist dist mkdir dist

where cmake >nul 2>nul
if %errorlevel% equ 0 (
    echo [*] CMake detected, building via CMake...
    if not exist build mkdir build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    if exist build\K2DynamicIslandTests.exe (
        build\K2DynamicIslandTests.exe
    )
    goto BUILD_DIST
)

echo [*] CMake not found in PATH, using native toolchain (g++ / windres)...
set "GXX="
where g++ >nul 2>nul
if %errorlevel% equ 0 (
    set "GXX=g++"
    set "WINDRES=windres"
)

if "!GXX!"=="" (
    for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW*") do (
        for /d %%B in ("%%D\llvm-mingw*\bin") do (
            if exist "%%B\g++.exe" (
                set "GXX=%%B\g++.exe"
                set "WINDRES=%%B\windres.exe"
            )
        )
    )
)

if "!GXX!"=="" (
    echo [ERROR] No C++ compiler found.
    exit /b 1
)

echo [*] Using compiler: !GXX!

echo [1/5] Compiling resources...
"!WINDRES!" src/resource.rc -O coff -o bin/resource.o

echo [2/5] Compiling K2DynamicIsland.exe...
"!GXX!" -std=c++17 -O2 -mwindows -Isrc -Ipackages/build/native/include -Ipackages ^
    src/main.cpp src/app_window.cpp src/webview_manager.cpp src/bridge_dispatcher.cpp ^
    src/storage_manager.cpp src/system_stats.cpp src/media_controller.cpp src/update_manager.cpp bin/resource.o ^
    -luuid -lole32 -lshell32 -lshlwapi -luser32 -lgdi32 -ladvapi32 -ldwmapi -lcomdlg32 -lwinhttp -lwintrust -lcrypt32 ^
    -o bin/K2DynamicIsland.exe
if %errorlevel% neq 0 (
    echo [ERROR] Application compilation failed!
    exit /b %errorlevel%
)

echo [3/5] Compiling unit test suite...
"!GXX!" -std=c++17 -O2 -Isrc -Ipackages/build/native/include -Ipackages ^
    tests/unit_tests.cpp src/storage_manager.cpp src/bridge_dispatcher.cpp ^
    src/system_stats.cpp src/media_controller.cpp src/update_manager.cpp ^
    -luuid -lole32 -lshell32 -lshlwapi -luser32 -lgdi32 -ladvapi32 -ldwmapi -lcomdlg32 -lwinhttp -lwintrust -lcrypt32 ^
    -o bin/K2DynamicIslandTests.exe
if %errorlevel% neq 0 (
    echo [ERROR] Unit test compilation failed!
    exit /b %errorlevel%
)

echo [4/5] Running automated unit tests...
bin\K2DynamicIslandTests.exe
if %errorlevel% neq 0 (
    echo [ERROR] Unit tests failed!
    exit /b %errorlevel%
)

:BUILD_DIST
echo [5/5] Packaging distribution and installer...
copy /y packages\build\native\x64\WebView2Loader.dll bin\WebView2Loader.dll >nul

rem Harden and protect frontend assets into bin\assets
python scripts/protect_assets.py
if %errorlevel% neq 0 (
    echo [WARNING] Python asset protection script had issues, falling back to direct copy...
    xcopy /e /i /y assets bin\assets >nul
)

rem Compile Uninstaller and build payload.zip
"!GXX!" -std=c++17 -O2 -mwindows -municode installer/uninstaller_main.cpp -lshell32 -lshlwapi -luser32 -ladvapi32 -o bin\Uninstall.exe
powershell -NoProfile -Command "Compress-Archive -Path 'bin\K2DynamicIsland.exe', 'bin\WebView2Loader.dll', 'bin\assets', 'bin\Uninstall.exe' -DestinationPath 'installer\payload.zip' -Force"

rem Compile Standalone Installer
"!WINDRES!" installer/installer_resource.rc -O coff -o installer/installer_resource.o
"!GXX!" -std=c++17 -O2 -mwindows -municode installer/installer_main.cpp installer/installer_resource.o -luuid -lole32 -lshell32 -lshlwapi -luser32 -ladvapi32 -lcomctl32 -o dist\K2DynamicIsland-Setup.exe

rem Copy binaries to dist
copy /y bin\K2DynamicIsland.exe dist\K2DynamicIsland.exe >nul
copy /y bin\WebView2Loader.dll dist\WebView2Loader.dll >nul
xcopy /e /i /y bin\assets dist\assets >nul

rem Remove any legacy portable zip
if exist "dist\K2DynamicIsland-Portable.zip" del /f /q "dist\K2DynamicIsland-Portable.zip"

echo ===================================================
echo   Build completed successfully!
echo   Application: dist\K2DynamicIsland.exe
echo   Installer:   dist\K2DynamicIsland-Setup.exe
echo ===================================================
