# KinectNavigator.ps1 - entry point / launcher.
#
# Loads the shared engine (KinectNavigator.Core.psm1), checks the shim DLL is
# present, then hands off to a front-end:
#   * the GUI  (KinectNavigator.Gui.ps1)      - default
#   * the text menu (KinectNavigator.Console.ps1) - with -Console
#
# The GUI is normally launched with a hidden console (see
# KinectNavigator-GUI.bat, one level up), so anything fatal here has to
# surface as a message box, not a Write-Host the user will never see.
param([switch]$Console)

$ErrorActionPreference = 'Stop'

$ScriptDir = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ScriptDir)) {
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}

Import-Module (Join-Path $ScriptDir 'KinectNavigator.Core.psm1') -Force -DisableNameChecking

$Paths = Initialize-KinectNavigatorCore -ScriptDir $ScriptDir

$bootCfg  = Load-Config $Paths.ConfigPath
$bootLang = if ($bootCfg.Lang) { $bootCfg.Lang } else { Resolve-DefaultLanguage }
$null = Initialize-Language -LangDir $Paths.LangDir -Code $bootLang

function Show-FatalError([string]$Message) {
    if (-not $Console) {
        try {
            Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop
            [System.Windows.Forms.MessageBox]::Show(
                $Message, (T 'app.msgbox_title'),
                [System.Windows.Forms.MessageBoxButtons]::OK,
                [System.Windows.Forms.MessageBoxIcon]::Error) | Out-Null
        } catch {
            Write-Host $Message -ForegroundColor Red
        }
    } else {
        Write-Host $Message -ForegroundColor Red
        Write-Host ""
        Read-Host (T 'console.press_enter_close') | Out-Null
    }
}

if (-not (Test-Path -LiteralPath $Paths.ShimSrc)) {
    Show-FatalError (T 'dlg.no_shim')
    exit 1
}

$GuiScript     = Join-Path $ScriptDir 'KinectNavigator.Gui.ps1'
$ConsoleScript = Join-Path $ScriptDir 'KinectNavigator.Console.ps1'
$UseGui        = (-not $Console) -and (Test-Path -LiteralPath $GuiScript)

if ($UseGui) {
    try {
        . $GuiScript -Paths $Paths -BootConfig $bootCfg
    } catch {
        Show-FatalError ($_.Exception.Message)
        exit 1
    }
} elseif (Test-Path -LiteralPath $ConsoleScript) {
    . $ConsoleScript -Paths $Paths -BootConfig $bootCfg
} else {
    Show-FatalError (T 'console.no_frontend')
    exit 1
}
