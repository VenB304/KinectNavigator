@echo off
setlocal EnableExtensions

rem  Assemble a player-facing release zip.
rem  Usage:  package.cmd  [version]
rem     e.g. package.cmd 0.9
rem  Produces  dist\KinectNavigator-<version>.zip  (git-ignored) -- attach it to a
rem  GitHub Release. Build first (build.cmd) so dist\Kinect10.dll exists.

set "VER=%~1"
if "%VER%"=="" set "VER=dev"

set "HERE=%~dp0"
set "ROOT=%HERE%.."
set "STAGE=%HERE%_pkg"
set "OUT=%HERE%KinectNavigator-%VER%.zip"

if not exist "%HERE%Kinect10.dll" (
    echo [X] dist\Kinect10.dll not found -- run build.cmd first.
    exit /b 1
)

for %%A in ("%HERE%Kinect10.dll") do set "SZ=%%~zA"
if %SZ% GEQ 1000000 (
    echo [X] dist\Kinect10.dll is %SZ% bytes -- that looks like the genuine runtime,
    echo     not the shim. Rebuild.
    exit /b 1
)

if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"

copy /y "%HERE%Kinect10.dll"            "%STAGE%\" >nul
copy /y "%HERE%gestures.png"            "%STAGE%\" >nul
copy /y "%HERE%KinectNavigator-Setup.ps1"   "%STAGE%\" >nul
copy /y "%HERE%KinectNavigator-Setup.vbs"   "%STAGE%\" >nul
copy /y "%HERE%KinectNavigator-Setup.bat"   "%STAGE%\" >nul
copy /y "%HERE%install.bat"             "%STAGE%\" >nul
copy /y "%HERE%uninstall.bat"           "%STAGE%\" >nul
copy /y "%HERE%record.bat"              "%STAGE%\" >nul
copy /y "%HERE%kinectnav.example.ini"   "%STAGE%\" >nul
copy /y "%ROOT%\SETUP.md"              "%STAGE%\" >nul

if not exist "%HERE%lang\en.json" (
    echo [X] dist\lang\en.json not found -- the language files are missing.
    rmdir /s /q "%STAGE%"
    exit /b 1
)
mkdir "%STAGE%\lang"
copy /y "%HERE%lang\*.json"             "%STAGE%\lang\" >nul

if exist "%OUT%" del "%OUT%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%OUT%' -Force"
if errorlevel 1 (
    echo [X] Compress-Archive failed.
    rmdir /s /q "%STAGE%"
    exit /b 1
)

rmdir /s /q "%STAGE%"
echo.
echo Packaged: %OUT%
for %%A in ("%OUT%") do echo   %%~zA bytes
echo Contents: Kinect10.dll, KinectNavigator-Setup.ps1/.vbs/.bat, gestures.png,
echo           install.bat, uninstall.bat, record.bat,
echo           kinectnav.example.ini, SETUP.md, lang\ (12 languages)
endlocal
