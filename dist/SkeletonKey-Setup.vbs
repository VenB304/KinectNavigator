' SkeletonKey-Setup.vbs - flash-free launcher for the setup window.
'
' Double-click this (or SkeletonKey-Setup.bat, which just calls it) to open the
' Skeleton Key installer with no console window. Prefer the command line?
' install.bat / uninstall.bat still work.

Option Explicit

Dim shell, fso, here, cmd
Set shell = CreateObject("WScript.Shell")
Set fso   = CreateObject("Scripting.FileSystemObject")

here = fso.GetParentFolderName(WScript.ScriptFullName)
shell.CurrentDirectory = here

cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File """ & _
      here & "\SkeletonKey-Setup.ps1"""

' 0 = hidden window, False = don't wait for it to exit
shell.Run cmd, 0, False
