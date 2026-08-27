@echo off
setlocal EnableExtensions

rem  Toggle skeleton capture for the next game session.
rem    record.bat "C:\path\to\LegacyPC - Game"          -> capture ON
rem    record.bat "C:\path\to\LegacyPC - Game" off      -> capture OFF
rem
rem  When ON, launching Legacy.exe writes skcap-<timestamp>.skcap into the game
rem  folder. Replay it with:  SkeletonKeyReplay skcap-....skcap

set "GAME=%~1"
if "%GAME%"=="" set "GAME=%CD%"

if not exist "%GAME%\Legacy.exe" (
    echo [X] Legacy.exe not found in "%GAME%".
    exit /b 1
)

if /I "%~2"=="off" (
    del "%GAME%\record.flag" 2>nul
    echo capture OFF
    exit /b 0
)

type nul > "%GAME%\record.flag"
echo capture ON -- next launch of Legacy.exe records skcap-*.skcap into
echo   "%GAME%"
echo run  record.bat "%GAME%" off  to stop.
endlocal
