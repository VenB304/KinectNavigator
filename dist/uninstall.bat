@echo off
setlocal EnableExtensions

rem  KinectNavigator uninstaller -- reverses install.bat.
rem    - double-click it, or
rem    - uninstall.bat "C:\path\to\your game folder"

set "GAME=%~1"
if "%GAME%"=="" if exist "%CD%\legacy.exe" set "GAME=%CD%"

:askpath
if "%GAME%"=="" (
    echo Enter the full path to your game folder ^(the one with legacy.exe in it^),
    set /p "GAME=or drag that folder onto this window and press Enter: "
)
set "GAME=%GAME:"=%"

if not exist "%GAME%\legacy.exe" (
    echo.
    echo [X] legacy.exe not found in "%GAME%".
    set "GAME="
    goto askpath
)
if not exist "%GAME%\Kinect10_backend.dll" (
    echo [X] Kinect10_backend.dll not found in "%GAME%" -- nothing to undo.
    goto done
)

echo Removing KinectNavigator from "%GAME%"
if exist "%GAME%\Kinect10.dll" del "%GAME%\Kinect10.dll"
ren "%GAME%\Kinect10_backend.dll" "Kinect10.dll"
if exist "%GAME%\Kinect10.dll.orig-backup" del "%GAME%\Kinect10.dll.orig-backup"

echo Done. Genuine Kinect10.dll restored.
echo   ^(kinectnav.ini, KinectNavigator.log and any skcap-*.skcap were left in place^)

:done
echo.
pause
endlocal
