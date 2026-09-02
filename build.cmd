@echo off
setlocal EnableExtensions

rem  Build KinectNavigator (Release ^| Win32) with whatever Visual Studio is installed.

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [X] vswhere.exe not found. Install Visual Studio with the Desktop C++ workload.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if "%VSINSTALL%"=="" (
    echo [X] No VS install with the C++ toolset found.
    exit /b 1
)

set "MSBUILD=%VSINSTALL%\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
    echo [X] MSBuild not found at "%MSBUILD%".
    exit /b 1
)

"%MSBUILD%" "%~dp0src\KinectNavigator.sln" /nologo /m ^
    /p:Configuration=Release /p:Platform=Win32 %*
if errorlevel 1 exit /b 1

echo.
echo Built: %~dp0build\Win32\Release\Kinect10.dll
echo Copied to: %~dp0dist\Kinect10.dll
endlocal
