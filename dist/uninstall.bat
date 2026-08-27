@echo off
setlocal EnableExtensions

rem  Skeleton Key uninstaller -- reverses install.bat.
rem  Usage:  uninstall.bat  "C:\path\to\LegacyPC - Game"

set "GAME=%~1"
if "%GAME%"=="" set "GAME=%CD%"

if not exist "%GAME%\Kinect10_backend.dll" (
    echo [X] Kinect10_backend.dll not found in "%GAME%" -- nothing to undo.
    exit /b 1
)

echo Removing Skeleton Key from "%GAME%"
if exist "%GAME%\Kinect10.dll" del "%GAME%\Kinect10.dll"
ren "%GAME%\Kinect10_backend.dll" "Kinect10.dll"
if exist "%GAME%\Kinect10.dll.orig-backup" del "%GAME%\Kinect10.dll.orig-backup"

echo Done. Genuine Kinect10.dll restored.
echo   ^(kinectnav.ini, SkeletonKey.log and any skcap-*.skcap were left in place^)
endlocal
