@echo off
rem Text-menu install / update / uninstall / settings -- full parity with the
rem GUI, for anyone who'd rather not (or can't) click through a window.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0app\KinectNavigator.ps1" -Console
