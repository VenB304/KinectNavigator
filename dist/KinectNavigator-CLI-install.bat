@echo off
setlocal EnableExtensions

rem  KinectNavigator installer (command line).
rem    - double-click it, or
rem    - KinectNavigator-CLI-install.bat "C:\path\to\your game folder"
rem
rem  Renames the genuine Kinect10.dll to Kinect10_backend.dll and drops
rem  KinectNavigator's Kinect10.dll in its place. KinectNavigator-CLI-uninstall.bat
rem  reverses this.

rem  the shim ships in the hidden "app" folder; fall back to next-to-this-script
set "SHIM=%~dp0app\Kinect10.dll"
if not exist "%SHIM%" set "SHIM=%~dp0Kinect10.dll"

set "GAME=%~1"
if "%GAME%"=="" if exist "%CD%\legacy.exe" set "GAME=%CD%"

:askpath
if "%GAME%"=="" (
    echo Enter the full path to your game folder ^(the one with legacy.exe in it^),
    set /p "GAME=or drag that folder onto this window and press Enter: "
)
rem  strip surrounding quotes if the user pasted a quoted path
set "GAME=%GAME:"=%"

if not exist "%GAME%\legacy.exe" (
    echo.
    echo [X] legacy.exe not found in "%GAME%".
    set "GAME="
    goto askpath
)
if not exist "%GAME%\Kinect10.dll" (
    echo [X] Kinect10.dll not found in "%GAME%".
    goto done
)
if exist "%GAME%\Kinect10_backend.dll" (
    echo [X] Kinect10_backend.dll already exists -- KinectNavigator looks installed.
    echo     Run KinectNavigator-CLI-uninstall.bat first if you want to reinstall.
    goto done
)
if not exist "%SHIM%" (
    echo [X] KinectNavigator's Kinect10.dll is not next to this script or in the app folder.
    goto done
)

rem  Guard: the genuine runtime is ~15 MB. Refuse if Kinect10.dll is tiny,
rem  which would mean KinectNavigator (or another shim) is already in place.
for %%A in ("%GAME%\Kinect10.dll") do set "SZ=%%~zA"
if %SZ% LSS 1000000 (
    echo [X] "%GAME%\Kinect10.dll" is only %SZ% bytes -- not the genuine ~15 MB runtime.
    echo     Restore the original Kinect10.dll before installing. If you drive the game
    echo     with a webcam emulator, KinectNavigator is not compatible with that setup.
    goto done
)

echo Installing KinectNavigator into "%GAME%"
copy /y "%GAME%\Kinect10.dll" "%GAME%\Kinect10.dll.orig-backup" >nul
ren "%GAME%\Kinect10.dll" "Kinect10_backend.dll"
copy /y "%SHIM%" "%GAME%\Kinect10.dll" >nul

echo.
echo Done.
echo   genuine runtime renamed to Kinect10_backend.dll  ^(backup: Kinect10.dll.orig-backup^)
echo   KinectNavigator installed as Kinect10.dll
echo   a log will be written to KinectNavigator.log in the game folder
echo   optional tuning: copy app\kinectnav.example.ini to "%GAME%\kinectnav.ini" and edit it

rem  Friendly reminder: the optional on-screen HUD can't draw over exclusive fullscreen.
if not exist "%GAME%\config.xml" goto done
findstr /i /c:"FullScreen=\"1\"" "%GAME%\config.xml" >nul 2>&1 || goto done
echo.
echo   NOTE: config.xml has FullScreen="1". Navigation works regardless, but
echo         the optional HUD ^(overlay = 1^) only shows when the game is
echo         windowed -- set FullScreen="0" if you want it.

:done
echo.
pause
endlocal
