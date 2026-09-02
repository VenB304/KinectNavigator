@echo off
rem Hand off to the .vbs launcher so no console window is left behind.
rem (Double-clicking KinectNavigator.vbs directly is fully flash-free.)
if exist "%~dp0app" attrib +h "%~dp0app" >nul 2>&1
start "" "%~dp0KinectNavigator.vbs"
