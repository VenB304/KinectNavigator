@echo off
setlocal EnableExtensions

rem  Assemble a player-facing release zip.
rem  Usage:  package.cmd  [version]
rem     e.g. package.cmd 1.0.0   (match src\KinectNavigator\version.h)
rem  Produces  dist\KinectNavigator-<version>.zip  (git-ignored) -- attach it to a
rem  GitHub Release. Build first (build.cmd) so dist\Kinect10.dll exists.
rem
rem  Layout in the zip: the four things a user runs at the root
rem  (KinectNavigator.vbs/.bat and KinectNavigator-CLI-install/uninstall.bat) plus
rem  SETUP.md / USAGE.md; everything else goes in a HIDDEN "app\" folder.

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
if not exist "%ROOT%\docs\setup.md" ( echo [X] docs\setup.md not found. & exit /b 1 )
if not exist "%HERE%lang\en.json" ( echo [X] dist\lang\en.json not found. & exit /b 1 )

if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
mkdir "%STAGE%\app"
mkdir "%STAGE%\app\lang"

rem ---- root: what the user runs / reads ----
copy /y "%HERE%KinectNavigator.vbs"                  "%STAGE%\" >nul
copy /y "%HERE%KinectNavigator.bat"                  "%STAGE%\" >nul
copy /y "%HERE%KinectNavigator-CLI-install.bat"      "%STAGE%\" >nul
copy /y "%HERE%KinectNavigator-CLI-uninstall.bat"    "%STAGE%\" >nul
copy /y "%ROOT%\docs\setup.md"                       "%STAGE%\SETUP.md" >nul
copy /y "%ROOT%\docs\usage.md"                       "%STAGE%\USAGE.md" >nul

rem ---- app\: the machinery (hidden in the zip) ----
copy /y "%HERE%KinectNavigator.ps1"    "%STAGE%\app\" >nul
copy /y "%HERE%Kinect10.dll"           "%STAGE%\app\" >nul
copy /y "%HERE%gestures.png"           "%STAGE%\app\" >nul
copy /y "%HERE%kinectnav.example.ini"  "%STAGE%\app\" >nul
copy /y "%HERE%lang\*.json"            "%STAGE%\app\lang\" >nul

if exist "%OUT%" del "%OUT%"
powershell -NoProfile -Command "$s='%STAGE%';$o='%OUT%';Add-Type -AssemblyName System.IO.Compression.FileSystem;$z=[System.IO.Compression.ZipFile]::Open($o,'Create');try{$d=$z.CreateEntry('app/');$d.ExternalAttributes=18;foreach($f in (Get-ChildItem -LiteralPath $s -Recurse -File)){$r=$f.FullName.Substring($s.Length+1).Replace('\','/');$e=[System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($z,$f.FullName,$r);if($r -like 'app/*'){$e.ExternalAttributes=($e.ExternalAttributes -bor 2)}}}finally{$z.Dispose()}"
if errorlevel 1 (
    echo [X] zip build failed.
    rmdir /s /q "%STAGE%"
    exit /b 1
)

rmdir /s /q "%STAGE%"
echo.
echo Packaged: %OUT%
for %%A in ("%OUT%") do echo   %%~zA bytes
echo Root:  KinectNavigator.vbs / .bat, KinectNavigator-CLI-install.bat,
echo        KinectNavigator-CLI-uninstall.bat, SETUP.md, USAGE.md
echo app\:  KinectNavigator.ps1, Kinect10.dll, gestures.png,
echo        kinectnav.example.ini, lang\ (12 languages)   [hidden]
endlocal
