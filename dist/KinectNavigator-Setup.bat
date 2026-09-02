@echo off
rem Hand off to the .vbs launcher so no console window is left behind.
rem (Double-clicking KinectNavigator-Setup.vbs directly is fully flash-free.)
start "" "%~dp0KinectNavigator-Setup.vbs"
