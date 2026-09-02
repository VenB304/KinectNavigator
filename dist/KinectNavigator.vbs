' KinectNavigator.vbs - flash-free launcher for the setup window.
'
' Double-click this (or KinectNavigator.bat, which just calls it) to open the
' KinectNavigator installer with no console window. Prefer the command line?
' KinectNavigator-CLI-install.bat / -uninstall.bat still work.

Option Explicit

Dim shell, fso, here, appDir, ps, cmd
Set shell = CreateObject("WScript.Shell")
Set fso   = CreateObject("Scripting.FileSystemObject")

here = fso.GetParentFolderName(WScript.ScriptFullName)
shell.CurrentDirectory = here

appDir = here & "\app"

' keep the folder that holds the machinery out of the way
On Error Resume Next
If fso.FolderExists(appDir) Then
    fso.GetFolder(appDir).Attributes = fso.GetFolder(appDir).Attributes Or 2   ' +Hidden
End If
On Error Goto 0

ps = appDir & "\KinectNavigator.ps1"
If Not fso.FileExists(ps) Then ps = here & "\KinectNavigator.ps1"

cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File """ & ps & """"

' 0 = hidden window, False = don't wait for it to exit
shell.Run cmd, 0, False
