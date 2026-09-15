@echo off
rem "start" detaches PowerShell so this window closes right away instead of
rem staying open behind the GUI for the whole session; -WindowStyle Hidden
rem keeps PowerShell's own window from ever appearing. This used to go
rem through a KinectNavigator.vbs helper (WScript.Shell.Run into a hidden
rem PowerShell) for a flash-free launch, but a .vbs silently spawning a
rem hidden, -ExecutionPolicy Bypass PowerShell process is a well-documented
rem dropper/loader pattern antivirus and SmartScreen heuristics watch for --
rem dropped in favour of this direct call (same fix LegacyDownloader made).
start "" powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0app\KinectNavigator.ps1"
