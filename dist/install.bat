@echo off
setlocal EnableExtensions

rem  Skeleton Key installer.
rem  Usage:  install.bat  "C:\path\to\LegacyPC - Game"
rem  If no path is given, the current directory is used.
rem
rem  Renames the genuine Kinect10.dll to Kinect10_backend.dll and drops
rem  Skeleton Key's Kinect10.dll in its place. uninstall.bat reverses this.

set "GAME=%~1"
if "%GAME%"=="" set "GAME=%CD%"

if not exist "%GAME%\Legacy.exe" (
    echo [X] Legacy.exe not found in "%GAME%".
    echo     Pass the game folder as the first argument.
    exit /b 1
)
if not exist "%GAME%\Kinect10.dll" (
    echo [X] Kinect10.dll not found in "%GAME%".
    exit /b 1
)
if exist "%GAME%\Kinect10_backend.dll" (
    echo [X] Kinect10_backend.dll already exists -- Skeleton Key looks installed.
    echo     Run uninstall.bat first if you want to reinstall.
    exit /b 1
)
if not exist "%~dp0Kinect10.dll" (
    echo [X] Skeleton Key's Kinect10.dll is not next to this script.
    echo     Build the project first ^(build.cmd^).
    exit /b 1
)

rem  Guard: the genuine runtime is ~15 MB. Refuse if Kinect10.dll is tiny,
rem  which would mean Skeleton Key (or another shim) is already in place.
for %%A in ("%GAME%\Kinect10.dll") do set "SZ=%%~zA"
if %SZ% LSS 1000000 (
    echo [X] "%GAME%\Kinect10.dll" is only %SZ% bytes -- not the genuine runtime.
    echo     Restore the original Kinect10.dll before installing.
    exit /b 1
)

echo Installing Skeleton Key into "%GAME%"
copy /y "%GAME%\Kinect10.dll" "%GAME%\Kinect10.dll.orig-backup" >nul
ren "%GAME%\Kinect10.dll" "Kinect10_backend.dll"
copy /y "%~dp0Kinect10.dll" "%GAME%\Kinect10.dll" >nul
if not exist "%GAME%\kinectnav.json" (
    if exist "%~dp0kinectnav.default.json" copy /y "%~dp0kinectnav.default.json" "%GAME%\kinectnav.json" >nul
)

echo.
echo Done.
echo   genuine runtime renamed to Kinect10_backend.dll  ^(backup: Kinect10.dll.orig-backup^)
echo   Skeleton Key installed as Kinect10.dll
echo   runtime log will be written to SkeletonKey.log in the game folder
endlocal
