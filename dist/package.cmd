@echo off
setlocal EnableExtensions

rem  Assemble a player-facing release zip.
rem  Usage:  package.cmd  [version]
rem     e.g. package.cmd 1.0.0   (match src\KinectNavigator\version.h)
rem  Produces  dist\KinectNavigator-<version>.zip  (git-ignored) -- attach it to a
rem  GitHub Release. Build first (build.cmd) so dist\Kinect10.dll exists.
rem
rem  Layout in the zip: the two things a user runs at the root
rem  (KinectNavigator-GUI.bat / KinectNavigator-Console.bat) plus SETUP.md /
rem  USAGE.md; everything else (the ps1/psm1 engine, the shim, lang\, etc.)
rem  lives in "app\" -- and already does in this repo, not just at packaging
rem  time, so what you see in dist\app\ is what ships.

set "VER=%~1"
if "%VER%"=="" set "VER=dev"

set "HERE=%~dp0"
set "APP=%HERE%app"
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
if not exist "%ROOT%\docs\setup.md" ( echo [X] docs\setup.md not found. & exit /b 1 )
if not exist "%APP%\lang\en.json" ( echo [X] dist\app\lang\en.json not found. & exit /b 1 )
if not exist "%APP%\KinectNavigatorTutorial.exe" ( echo [X] dist\app\KinectNavigatorTutorial.exe not found -- run build.cmd first. & exit /b 1 )

if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
mkdir "%STAGE%\app"
mkdir "%STAGE%\app\lang"

rem ---- root: what the user runs / reads ----
copy /y "%HERE%KinectNavigator-GUI.bat"     "%STAGE%\" >nul
copy /y "%HERE%KinectNavigator-Console.bat" "%STAGE%\" >nul
copy /y "%ROOT%\docs\setup.md"              "%STAGE%\SETUP.md" >nul
copy /y "%ROOT%\docs\usage.md"              "%STAGE%\USAGE.md" >nul

rem ---- app\: the machinery (hidden in the zip) ----
copy /y "%APP%\KinectNavigator.ps1"          "%STAGE%\app\" >nul
copy /y "%APP%\KinectNavigator.Core.psm1"    "%STAGE%\app\" >nul
copy /y "%APP%\KinectNavigator.Gui.ps1"      "%STAGE%\app\" >nul
copy /y "%APP%\KinectNavigator.Console.ps1"  "%STAGE%\app\" >nul
copy /y "%APP%\gestures.png"                 "%STAGE%\app\" >nul
copy /y "%APP%\kinectnav.example.ini"        "%STAGE%\app\" >nul
copy /y "%HERE%Kinect10.dll"                 "%STAGE%\app\" >nul
copy /y "%APP%\KinectNavigatorTutorial.exe"  "%STAGE%\app\" >nul
copy /y "%APP%\lang\*.json"                  "%STAGE%\app\lang\" >nul

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
echo Root:  KinectNavigator-GUI.bat, KinectNavigator-Console.bat, SETUP.md, USAGE.md
echo app\:  KinectNavigator.ps1 (+ .Core.psm1 / .Gui.ps1 / .Console.ps1),
echo        Kinect10.dll, KinectNavigatorTutorial.exe, gestures.png,
echo        kinectnav.example.ini, lang\ (12 languages)
endlocal
