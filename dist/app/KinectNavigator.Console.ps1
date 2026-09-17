# KinectNavigator.Console.ps1 - text-menu installer + config editor.
#
# Full parity with KinectNavigator.Gui.ps1: install/update/uninstall, and every
# setting the GUI's main window + "More settings" dialog exposes. Dot-sourced
# by KinectNavigator.ps1, which has already loaded KinectNavigator.Core.psm1
# and picked a language. Calls the same Install-KinectNavigator /
# Uninstall-KinectNavigator as the GUI -- see KinectNavigator.Core.psm1's
# header comment for why that sharing matters.
param(
    [Parameter(Mandatory = $true)]$Paths,
    [Parameter(Mandatory = $true)]$BootConfig
)

$ShimSrc    = $Paths.ShimSrc
$ExampleIni = $Paths.ExampleIni
$ShimVer    = Get-DllVersion $ShimSrc

# A real folder-browse dialog beats asking a non-technical player to type or
# paste a path -- WinForms works fine from a console host, no window of its
# own is needed for just a dialog.
Add-Type -AssemblyName System.Windows.Forms

try { $Host.UI.RawUI.WindowTitle = (T 'app.title') } catch { }

$script:GameFolder = $BootConfig.GamePath
if ([string]::IsNullOrWhiteSpace($script:GameFolder)) {
    foreach ($g in @($Paths.ScriptDir, (Split-Path -Parent $Paths.ScriptDir))) {
        if ($g -and (Test-Path -LiteralPath (Join-Path $g 'legacy.exe'))) { $script:GameFolder = $g; break }
    }
}
$script:RuntimeDetected = Test-KinectRuntime

function W([string]$s = '') { Write-Host $s }
function WOk([string]$s)   { Write-Host "  [ OK ]  $s" -ForegroundColor Green }
function WWarn([string]$s) { Write-Host "  [ !! ]  $s" -ForegroundColor Yellow }
function WErr([string]$s)  { Write-Host "  [ X  ]  $s" -ForegroundColor Red }
function WInfo([string]$s) { Write-Host "  [ i  ]  $s" -ForegroundColor Cyan }
function Pause-Return { Write-Host ''; Read-Host (T 'console.press_enter') | Out-Null }

function Read-YesNo([string]$prompt) {
    Write-Host "$prompt $(T 'console.yn_suffix')" -NoNewline
    $a = (Read-Host).Trim().ToLower()
    return ($a -eq 'y' -or $a -eq 'yes')
}

# ---------------------------------------------------------------------------
# state + header (mirrors Gui.ps1's Refresh-Status, printed as text)
# ---------------------------------------------------------------------------
function Get-InstallAction($st) {
    # Returns @{ Label = <T'd button text>; Enabled = $bool; IsUpdate = $bool }
    if ($st.state -eq 'installed') {
        $iv = $st.instVer
        if ($null -ne $ShimVer -and $null -ne $iv) {
            if ($ShimVer -gt $iv)  { return @{ Label = (T 'btn.update');   Enabled = $true;  IsUpdate = $true } }
            if ($ShimVer -eq $iv)  { return @{ Label = (T 'btn.uptodate'); Enabled = $false; IsUpdate = $true } }
            return @{ Label = (T 'btn.reinstall'); Enabled = $true; IsUpdate = $true }
        }
        return @{ Label = (T 'btn.update'); Enabled = $true; IsUpdate = $true }
    }
    $canInstall = ($st.state -eq 'genuine')
    return @{ Label = (T 'btn.install'); Enabled = $canInstall; IsUpdate = $false }
}

function Show-Header {
    Clear-Host
    W (T 'app.title')
    W (T 'app.subtitle')
    if ($ShimVer) { W (T 'app.installer_version' @{ v = (VerShort $ShimVer) }) }
    W ('-' * 70)
}

function Show-Status {
    $st = Get-State $script:GameFolder
    $script:CurState = $st
    W (T 'gf.label' ) + ': ' + $(if ($script:GameFolder) { $script:GameFolder } else { '(' + (T 'console.not_set') + ')' })
    $detail = $(if ($st.detailVars) { T $st.detailKey $st.detailVars } else { T $st.detailKey })
    switch ($st.state) {
        'genuine'   { WOk   $detail }
        'installed' { WOk   $detail }
        'foreign'   { WWarn $detail }
        'nogame'    { WErr  $detail }
        'nodll'     { WErr  $detail }
        default     { W ('        ' + $detail) }
    }
    $act = Get-InstallAction $st
    if ($st.state -eq 'installed') {
        $iv = $st.instVer
        if ($null -ne $ShimVer -and $null -ne $iv) {
            if ($ShimVer -gt $iv)      { WInfo (T 'ver.update_avail' @{ inst = (VerShort $iv); shim = (VerShort $ShimVer) }) }
            elseif ($ShimVer -eq $iv)  { WOk   (T 'ver.uptodate' @{ inst = (VerShort $iv) }) }
            else                       { WWarn (T 'ver.older' @{ inst = (VerShort $iv); shim = (VerShort $ShimVer) }) }
        } else { WInfo (T 'ver.unreadable') }
    }
    if ($st.fs -eq 1)     { WInfo (T 'fs.fullscreen') }
    elseif ($st.fs -eq 0) { WInfo (T 'fs.windowed') }
    if (-not $script:RuntimeDetected) { WWarn (T 'runtime.missing') }
    W ('-' * 70)
    return $st
}

# ---------------------------------------------------------------------------
# key capture (console equivalent of Gui.ps1's Capture-Key). ConsoleKey's
# numeric values match Win32 virtual-key codes for the keys people actually
# rebind here (arrows, Enter, Esc, letters, function keys), so casting straight
# to int gives a usable vk -- same table Set-IniKey/VkName already expect.
# ---------------------------------------------------------------------------
function Capture-KeyConsole {
    W (T 'capture.body')
    W (T 'console.press_esc_cancel')
    $ignore = @(16, 17, 18, 20, 144, 145)   # Shift/Ctrl/Alt/CapsLock/NumLock/ScrollLock
    while ($true) {
        $k = [Console]::ReadKey($true)
        $vk = [int]$k.Key
        if ($vk -eq 27) { return $null }   # Esc cancels
        if ($ignore -contains $vk) { continue }
        return $vk
    }
}

# ---------------------------------------------------------------------------
# install / uninstall  -- both call the shared Core implementation
# ---------------------------------------------------------------------------
function Do-Install($st, $act) {
    if (Test-GameRunning) { WWarn (T 'dlg.game_running'); Pause-Return; return }
    try {
        $r = Install-KinectNavigator -GameFolder $st.folder -ShimSrc $ShimSrc -IsUpdate $act.IsUpdate
        if (-not $r.Ok) { throw (T $r.Key $r.Vars) }
        if ($r.RenamedBackend) { WOk (T 'log.renamed_backend') }
        if ($r.Vars) { WOk (T $r.Key $r.Vars) } else { WOk (T $r.Key) }
        $head = if ($act.IsUpdate) { T 'dlg.install_ok_upd' } else { T 'dlg.install_ok_new' }
        W ''
        W $head
        W (T 'dlg.install_ok_body')
        $st2 = Get-State $st.folder
        if ($st2.fs -eq 1) {
            $ini = Join-Path $st.folder 'kinectnav.ini'
            if (IniBool (Get-IniVal (Read-IniMap $ini) 'overlay' '0')) { W (T 'dlg.install_ok_hud_note') }
        }
    } catch {
        WErr (T 'log.error' @{ err = $_.Exception.Message })
        WErr (T 'dlg.install_fail' @{ err = $_.Exception.Message })
    }
    Pause-Return
}

function Do-Uninstall($st) {
    if (Test-GameRunning) { WWarn (T 'dlg.game_running'); Pause-Return; return }
    try {
        $r = Uninstall-KinectNavigator -GameFolder $st.folder
        if (-not $r.Ok) { throw (T $r.Key $r.Vars) }
        WOk (T $r.Key)
        WOk (T 'log.uninstalled_kept')
        W ''
        W (T 'dlg.uninstall_ok')
    } catch {
        WErr (T 'log.error' @{ err = $_.Exception.Message })
        WErr (T 'dlg.uninstall_fail' @{ err = $_.Exception.Message })
    }
    Pause-Return
}

# ---------------------------------------------------------------------------
# settings submenu -- full parity with the GUI main window + More settings
# ---------------------------------------------------------------------------
function Get-IniPathFor($st) {
    if ($st.state -ne 'genuine' -and $st.state -ne 'installed') { return $null }
    return (Join-Path $st.folder 'kinectnav.ini')
}

function Show-SettingsMenu {
    while ($true) {
        $st = Get-State $script:GameFolder
        $ini = Get-IniPathFor $st
        if ([string]::IsNullOrWhiteSpace($ini)) {
            Show-Header
            WErr (T 'state.none')
            Pause-Return
            return
        }
        $m = Read-IniMap $ini

        $hand   = ([string](Get-IniVal $m 'handedness' 'right')).Trim().ToLower()
        $handV  = if ($hand -eq 'left' -or $hand -eq '1') { T 'opt.hand_left' } else { T 'opt.hand_right' }
        $mirror = IniBool (Get-IniVal $m 'mirror' '0')
        $back   = IniBool (Get-IniVal $m 'enable_back' '1')
        $hud    = IniBool (Get-IniVal $m 'overlay' '0')
        $clutch = IniBool (Get-IniVal $m 'dpad_arm' '1')
        $reach  = Test-PresetMatch $m $REACH_IDS  $PRESET_REACH  $CFG_DEFAULTS
        $scroll = Test-PresetMatch $m $SCROLL_IDS $PRESET_SCROLL $CFG_DEFAULTS
        $cmd    = Test-PresetMatch $m $CMD_IDS    $PRESET_CMD    $CFG_DEFAULTS
        $hold   = Test-PresetMatch $m $HOLD_IDS   $PRESET_HOLD   $CFG_DEFAULTS
        $reachT  = if ($reach)  { T "preset.reach.$reach" }   else { T 'preset.custom' }
        $scrollT = if ($scroll) { T "preset.scroll.$scroll" } else { T 'preset.custom' }
        $cmdT    = if ($cmd)    { T "preset.cmd.$cmd" }       else { T 'preset.custom' }
        $holdT   = if ($hold)   { T "preset.hold.$hold" }     else { T 'preset.custom' }
        $onOff   = { param($b) if ($b) { T 'console.on' } else { T 'console.off' } }

        Show-Header
        W (T 'opt.header')
        W ''
        W (' 1) {0,-40} {1}' -f (T 'opt.hand_label'), $handV)
        W (' 2) {0,-40} {1}' -f (T 'opt.mirror'), (& $onOff $mirror))
        W (' 3) {0,-40} {1}' -f (T 'opt.back'), (& $onOff $back))
        W (' 4) {0,-40} {1}' -f (T 'opt.hud'), (& $onOff $hud))
        W ''
        W (T 'more.feel_header')
        W (' 5) {0,-40} {1}' -f (T 'more.reach'), $reachT)
        W (' 6) {0,-40} {1}' -f (T 'more.scroll'), $scrollT)
        W (' 7) {0,-40} {1}' -f (T 'more.cmdreach'), $cmdT)
        W (' 8) {0,-40} {1}' -f (T 'more.hold'), $holdT)
        W (' 9) {0,-40} {1}' -f (T 'more.clutch'), (& $onOff $clutch))
        W ''
        W (T 'more.keys_header')
        W ('10) ' + (T 'btn.change'))
        W ('11) ' + (T 'btn.reset_keys'))
        W ''
        W ('12) ' + (T 'btn.open_ini'))
        W ('13) ' + (T 'btn.reset_cfg'))
        W (' 0) ' + (T 'console.back'))
        W ('-' * 70)
        $c = Read-Host (T 'console.prompt_choice')

        switch ($c.Trim()) {
            '1' {
                $newVal = if ($hand -eq 'left' -or $hand -eq '1') { 'right' } else { 'left' }
                Set-IniKey $ini 'handedness' $newVal
                WOk (T 'log.ini_set' @{ k = 'handedness'; v = $newVal }); Pause-Return
            }
            '2' { $v = if ($mirror) { '0' } else { '1' }; Set-IniKey $ini 'mirror' $v; WOk (T 'log.ini_set' @{ k = 'mirror'; v = $v }); Pause-Return }
            '3' { $v = if ($back)   { '0' } else { '1' }; Set-IniKey $ini 'enable_back' $v; WOk (T 'log.ini_set' @{ k = 'enable_back'; v = $v }); Pause-Return }
            '4' {
                $v = if ($hud) { '0' } else { '1' }
                Set-IniKey $ini 'overlay' $v
                WOk (T $(if ($v -eq '1') { 'log.hud_on' } else { 'log.hud_off' }))
                if ($v -eq '1' -and $st.fs -eq 1) {
                    if (Read-YesNo (T 'dlg.hud_fs_body')) {
                        if (Set-ConfigWindowed $st.folder) { WOk (T 'log.fs0_ok') } else { WErr (T 'log.fs0_fail') }
                    }
                }
                Pause-Return
            }
            '5' { Show-PresetPicker $ini $REACH_IDS  $PRESET_REACH  'preset.reach'  'more.reach' }
            '6' { Show-PresetPicker $ini $SCROLL_IDS $PRESET_SCROLL 'preset.scroll' 'more.scroll' }
            '7' { Show-PresetPicker $ini $CMD_IDS    $PRESET_CMD    'preset.cmd'    'more.cmdreach' }
            '8' { Show-PresetPicker $ini $HOLD_IDS   $PRESET_HOLD   'preset.hold'   'more.hold' }
            '9' { $v = if ($clutch) { '0' } else { '1' }; Set-IniKey $ini 'dpad_arm' $v; WOk (T 'log.ini_set' @{ k = 'dpad_arm'; v = $v }); Pause-Return }
            '10' { Show-KeyBindMenu $ini }
            '11' {
                Remove-IniKeys $ini @('key_left', 'key_right', 'key_up', 'key_down', 'key_confirm', 'key_back')
                WOk (T 'log.keys_reset'); Pause-Return
            }
            '12' {
                if (-not (Test-Path -LiteralPath $ini)) {
                    if (Test-Path -LiteralPath $ExampleIni) { Copy-Item -LiteralPath $ExampleIni -Destination $ini -Force; WOk (T 'log.ini_created') }
                    else { Set-IniKey $ini 'enable_back' '1' }
                }
                Start-Process notepad.exe -ArgumentList $ini -Wait
                WOk (T 'log.ini_opened'); Pause-Return
            }
            '13' {
                if (-not (Test-Path -LiteralPath $ini)) { WInfo (T 'log.ini_none'); Pause-Return }
                elseif (Read-YesNo (T 'dlg.reset_cfg_body')) {
                    Remove-Item -LiteralPath $ini -Force
                    WOk (T 'log.ini_deleted'); Pause-Return
                }
            }
            '0' { return }
            default { WErr (T 'console.invalid_choice'); Pause-Return }
        }
    }
}

function Show-PresetPicker([string]$ini, [string[]]$ids, $table, [string]$prefix, [string]$labelKey) {
    Show-Header
    W (T $labelKey)
    W ''
    for ($i = 0; $i -lt $ids.Count; $i++) { W ('{0}) {1}' -f ($i + 1), (T "$prefix.$($ids[$i])")) }
    W ('0) ' + (T 'console.cancel'))
    $c = Read-Host (T 'console.prompt_choice')
    $n = 0
    if ([int]::TryParse($c.Trim(), [ref]$n) -and $n -ge 1 -and $n -le $ids.Count) {
        $id = $ids[$n - 1]
        foreach ($kv in $table[$id].GetEnumerator()) { Set-IniKey $ini $kv.Key ([string]$kv.Value) }
        WOk (T 'log.preset_set' @{ label = (T $labelKey); name = (T "$prefix.$id") })
        Pause-Return
    }
}

function Show-KeyBindMenu([string]$ini) {
    while ($true) {
        $m = Read-IniMap $ini
        Show-Header
        W (T 'more.keys_header')
        W ''
        $i = 1
        foreach ($nm in $KEY_ORDER) {
            $def = $KEY_DEFS[$nm]
            $cur = Get-IniVal $m $def[0] $def[1]
            $vk  = ConvertTo-Vk ([string]$cur)
            $shown = if ($null -ne $vk) { VkName $vk } else { [string]$cur }
            W ('{0}) {1,-12} {2}' -f $i, (T ('key.' + $nm.ToLower())), $shown)
            $i++
        }
        W ('0) ' + (T 'console.back'))
        $c = Read-Host (T 'console.prompt_choice')
        $n = 0
        if ($c.Trim() -eq '0') { return }
        if ([int]::TryParse($c.Trim(), [ref]$n) -and $n -ge 1 -and $n -le $KEY_ORDER.Count) {
            $nm = $KEY_ORDER[$n - 1]
            $vk = Capture-KeyConsole
            if ($null -ne $vk) {
                Set-IniKey $ini $KEY_DEFS[$nm][0] ('0x{0:X2}' -f $vk)
                WOk (T 'log.ini_set' @{ k = $KEY_DEFS[$nm][0]; v = ('0x{0:X2}' -f $vk) })
                Pause-Return
            }
        } else { WErr (T 'console.invalid_choice'); Pause-Return }
    }
}

# ---------------------------------------------------------------------------
# language picker
# ---------------------------------------------------------------------------
function Show-LanguageMenu {
    $langs = @(Get-AvailableLanguages)
    if ($langs.Count -eq 0) { return }
    Show-Header
    W (T 'app.lang_label')
    W ''
    $cur = Get-CurrentLanguage
    for ($i = 0; $i -lt $langs.Count; $i++) {
        $mark = if ($langs[$i].Code -eq $cur) { '*' } else { ' ' }
        W ('{0} {1}) {2}' -f $mark, ($i + 1), $langs[$i].NativeName)
    }
    W ('0) ' + (T 'console.cancel'))
    $c = Read-Host (T 'console.prompt_choice')
    $n = 0
    if ([int]::TryParse($c.Trim(), [ref]$n) -and $n -ge 1 -and $n -le $langs.Count) {
        $null = Initialize-Language -Code $langs[$n - 1].Code
        Save-Config $Paths.ConfigPath $script:GameFolder (Get-CurrentLanguage)
    }
}

# ---------------------------------------------------------------------------
# "Try it out" -- live interactive tutorial, standalone (no game needed).
# Runs entirely outside the game so exclusive fullscreen (which hides the
# in-game HUD) is a non-issue. The exe handles "no Kinect" itself with its
# own friendly retry screen -- nothing to pre-check here.
# ---------------------------------------------------------------------------
function Start-Tutorial {
    $exe = Join-Path $Paths.ScriptDir 'KinectNavigatorTutorial.exe'
    if (-not (Test-Path -LiteralPath $exe)) { WErr (T 'dlg.no_tutorial'); Pause-Return; return }
    try {
        Start-Process -FilePath $exe -ArgumentList (Get-CurrentLanguage) -WorkingDirectory $Paths.ScriptDir | Out-Null
        WOk (T 'log.tutorial_launched')
    } catch {
        WErr (T 'log.error' @{ err = $_.Exception.Message })
        WErr (T 'dlg.tutorial_fail' @{ err = $_.Exception.Message })
    }
    Pause-Return
}

# ---------------------------------------------------------------------------
# main menu
# ---------------------------------------------------------------------------
while ($true) {
    Show-Header
    $st = Show-Status
    $act = Get-InstallAction $st

    W ('1) ' + (T 'console.menu_gf'))
    $lbl2 = if ($act.Enabled) { $act.Label } else { $act.Label + '  (' + (T 'console.unavailable') + ')' }
    W ('2) ' + $lbl2)
    if ($st.state -eq 'installed') { W ('3) ' + (T 'btn.uninstall')) }
    if ($st.state -eq 'genuine' -or $st.state -eq 'installed') { W ('4) ' + (T 'console.menu_settings')) }
    W ('5) ' + (T 'btn.gestures'))
    W ('6) ' + (T 'app.lang_label'))
    W ('7) ' + (T 'btn.tutorial'))
    W ('0) ' + (T 'console.menu_exit'))
    W ('-' * 70)
    $choice = Read-Host (T 'console.prompt_choice')

    switch ($choice.Trim()) {
        '1' {
            $d = New-Object System.Windows.Forms.FolderBrowserDialog
            $d.Description = (T 'gf.browse_desc')
            $d.ShowNewFolderButton = $false
            if ($script:GameFolder -and (Test-Path -LiteralPath $script:GameFolder)) { $d.SelectedPath = $script:GameFolder }
            if ($d.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                $script:GameFolder = $d.SelectedPath.TrimEnd('\')
            }
        }
        '2' {
            if ($act.Enabled) { Do-Install $st $act }
        }
        '3' {
            if ($st.state -eq 'installed') { Do-Uninstall $st }
        }
        '4' {
            if ($st.state -eq 'genuine' -or $st.state -eq 'installed') { Show-SettingsMenu }
        }
        '5' {
            Show-Header
            W (T 'gestures.body')
            Pause-Return
        }
        '6' { Show-LanguageMenu }
        '7' { Start-Tutorial }
        '0' { exit 0 }
        default { WErr (T 'console.invalid_choice'); Pause-Return }
    }
}
