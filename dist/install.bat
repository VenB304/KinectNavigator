@echo off
setlocal EnableExtensions

rem  KinectNavigator installer.
rem    - double-click it, or
rem    - install.bat "C:\path\to\your game folder"
rem
rem  Renames the genuine Kinect10.dll to Kinect10_backend.dll and drops
rem  KinectNavigator's Kinect10.dll in its place. uninstall.bat reverses this.

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
    echo     Run uninstall.bat first if you want to reinstall.
    goto done
)
if not exist "%~dp0Kinect10.dll" (
    echo [X] KinectNavigator's Kinect10.dll is not next to this script.
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
copy /y "%~dp0Kinect10.dll" "%GAME%\Kinect10.dll" >nul

echo.
echo Done.
echo   genuine runtime renamed to Kinect10_backend.dll  ^(backup: Kinect10.dll.orig-backup^)
echo   KinectNavigator installed as Kinect10.dll
echo   a log will be written to KinectNavigator.log in the game folder
echo   optional tuning: copy kinectnav.example.ini to "%GAME%\kinectnav.ini" and edit it

rem  Friendly reminder: the optional on-screen HUD can't draw over exclusive fullscreen.
if exist "%GAME%\config.xml" (
    findstr /i /c:"FullScreen=\"1\"" "%GAME%\config.xml" >nul 2>&1 && (
        echo.
        echo   NOTE: config.xml has FullScreen="1". Navigation works regardless, but the
        echo         optional HUD ^(overlay = 1^) only shows when the game runs windowed
        echo         -- set FullScreen="0" if you want it.
    )
)

:done
echo.
pause
endlocal
