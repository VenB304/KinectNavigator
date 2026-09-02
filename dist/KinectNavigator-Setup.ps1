# KinectNavigator-Setup.ps1 - windowed installer + config editor for KinectNavigator.
#
# Swaps the game's Kinect10.dll for the KinectNavigator shim (and back), and edits
# kinectnav.ini in the game folder live. Launched flash-free by
# KinectNavigator-Setup.vbs / .bat. Anything fatal here must surface as a MessageBox,
# not Write-Host.

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
try { [System.Windows.Forms.Application]::EnableVisualStyles() } catch { }
try { [System.Windows.Forms.Application]::SetCompatibleTextRenderingDefault($false) } catch { }

$ScriptDir = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ScriptDir)) {
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}

$ShimSrc      = Join-Path $ScriptDir 'Kinect10.dll'
$ExampleIni   = Join-Path $ScriptDir 'kinectnav.example.ini'
$StateDir     = Join-Path $env:LOCALAPPDATA 'KinectNavigator'
$LastPathFile = Join-Path $StateDir 'gamefolder.txt'
$MIN_GENUINE  = 1000000            # the real runtime is ~15 MB
$RuntimeUrl   = 'https://www.microsoft.com/download/details.aspx?id=40277'

$script:Loading    = $false        # true while controls are being populated from the ini
$script:dlgLoading = $false        # ditto, for the "more settings" dialog
$script:CurState   = $null

# FILEVERSION baked into the shim we ship (version.rc -> version.h). $null if the
# DLL beside us predates the version resource.
function Get-DllVersion([string]$path) {
    try {
        if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path -LiteralPath $path)) {
            $raw = (Get-Item -LiteralPath $path).VersionInfo.FileVersion
            if ($raw -and $raw -match '(\d+(?:\.\d+){1,3})') { return [version]$Matches[1] }
        }
    } catch { }
    return $null
}
function VerShort($v) {
    if ($null -eq $v) { return '?' }
    if ($v.Build -gt 0) { return ('{0}.{1}.{2}' -f $v.Major, $v.Minor, $v.Build) }
    return ('{0}.{1}' -f $v.Major, $v.Minor)
}
$ShimVer = Get-DllVersion $ShimSrc

# ---- theme (matches Legacy Downloader) ----
$FontBase  = New-Object System.Drawing.Font('Segoe UI', 9)
$FontBold  = New-Object System.Drawing.Font('Segoe UI', 9, [System.Drawing.FontStyle]::Bold)
$FontTitle = New-Object System.Drawing.Font('Segoe UI', 13, [System.Drawing.FontStyle]::Bold)
$FontMono  = New-Object System.Drawing.Font('Consolas', 9)
$ColBg     = [System.Drawing.Color]::FromArgb(246, 248, 250)
$ColCard   = [System.Drawing.Color]::White
$ColPrim   = [System.Drawing.Color]::FromArgb(18, 98, 200)
$ColText   = [System.Drawing.Color]::FromArgb(30, 41, 59)
$ColBorder = [System.Drawing.Color]::FromArgb(203, 213, 225)
$ColMuted  = [System.Drawing.Color]::FromArgb(100, 116, 139)
$ColWarn   = [System.Drawing.Color]::FromArgb(180, 83, 9)
$ColOk     = [System.Drawing.Color]::FromArgb(21, 128, 61)

# ---- preset tables (a preset = a fixed set of kinectnav.ini values) ----
$REACH_NAMES    = @('More sensitive', 'Normal', 'Bigger reach')
$SCROLL_NAMES   = @('Slower', 'Normal', 'Faster')
$CMDREACH_NAMES = @('Easier', 'Normal', 'Stricter')
$HOLD_NAMES     = @('Shorter', 'Normal', 'Longer')

$PRESET_REACH = @{
    'More sensitive' = [ordered]@{ dpad_park_radius = '0.26' }
    'Normal'         = [ordered]@{ dpad_park_radius = '0.32' }
    'Bigger reach'   = [ordered]@{ dpad_park_radius = '0.40' }
}
$PRESET_SCROLL = @{
    'Slower' = [ordered]@{ dpad_repeat_dwell_ms = '750'; dpad_repeat_first_ms = '550'; dpad_repeat_min_ms = '320'; dpad_repeat_accel_ms = '16' }
    'Normal' = [ordered]@{ dpad_repeat_dwell_ms = '600'; dpad_repeat_first_ms = '430'; dpad_repeat_min_ms = '200'; dpad_repeat_accel_ms = '22' }
    'Faster' = [ordered]@{ dpad_repeat_dwell_ms = '450'; dpad_repeat_first_ms = '320'; dpad_repeat_min_ms = '130'; dpad_repeat_accel_ms = '30' }
}
$PRESET_CMDREACH = @{
    'Easier'   = [ordered]@{ dpad_cmd_gate_radius = '0.50' }
    'Normal'   = [ordered]@{ dpad_cmd_gate_radius = '0.42' }
    'Stricter' = [ordered]@{ dpad_cmd_gate_radius = '0.32' }
}
$PRESET_HOLD = @{
    'Shorter' = [ordered]@{ dpad_cmd_dwell_ms = '400';  dpad_back_dwell_ms = '1500' }
    'Normal'  = [ordered]@{ dpad_cmd_dwell_ms = '600';  dpad_back_dwell_ms = '3000' }
    'Longer'  = [ordered]@{ dpad_cmd_dwell_ms = '1000'; dpad_back_dwell_ms = '4000' }
}

$KEY_ORDER = @('Left', 'Right', 'Up', 'Down', 'Confirm', 'Back')
$KEY_DEFS  = @{
    'Left'    = @('key_left', '0x25')
    'Right'   = @('key_right', '0x27')
    'Up'      = @('key_up', '0x26')
    'Down'    = @('key_down', '0x28')
    'Confirm' = @('key_confirm', '0x0D')
    'Back'    = @('key_back', '0x1B')
}
# compiled defaults for the keys the presets touch -- an unset key counts as this
$CFG_DEFAULTS = @{
    dpad_park_radius = '0.32'
    dpad_repeat_dwell_ms = '600'; dpad_repeat_first_ms = '430'; dpad_repeat_min_ms = '200'; dpad_repeat_accel_ms = '22'
    dpad_cmd_gate_radius = '0.42'
    dpad_cmd_dwell_ms = '600'; dpad_back_dwell_ms = '3000'
}

function New-Btn([string]$Text, [int]$X, [int]$Y, [int]$W, [int]$H = 30, [bool]$Primary = $false) {
    $b = New-Object System.Windows.Forms.Button
    $b.Text = $Text; $b.SetBounds($X, $Y, $W, $H)
    $b.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
    $b.Cursor = [System.Windows.Forms.Cursors]::Hand
    if ($Primary) {
        $b.Font = $FontBold; $b.BackColor = $ColPrim; $b.ForeColor = [System.Drawing.Color]::White
        $b.FlatAppearance.BorderSize = 0
    } else {
        $b.Font = $FontBase; $b.BackColor = $ColCard; $b.ForeColor = $ColText
        $b.FlatAppearance.BorderColor = $ColBorder; $b.FlatAppearance.BorderSize = 1
    }
    return $b
}
function Msg([string]$Text, $Icon) {
    [System.Windows.Forms.MessageBox]::Show($Form, $Text, 'KinectNavigator Setup',
        [System.Windows.Forms.MessageBoxButtons]::OK, $Icon) | Out-Null
}

if (-not (Test-Path -LiteralPath $ShimSrc)) {
    [System.Windows.Forms.MessageBox]::Show(
        "Kinect10.dll (the KinectNavigator shim) isn't next to this installer." + [Environment]::NewLine +
        "Extract the whole release together, then run it again.",
        'KinectNavigator Setup', 'OK', 'Error') | Out-Null
    exit 1
}

# ---------------------------------------------------------------------------
# best-effort checks
# ---------------------------------------------------------------------------

# Looks for ANY "Kinect ... Runtime/SDK" entry in the installed-programs
# registry (both native and the 32-bit view on a 64-bit OS). This is a
# heuristic, not a guarantee -- it only tells us an installer of that shape
# ran at some point, never gates Install, and a miss just shows a soft note.
function Test-KinectRuntime {
    $roots = @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*'
    )
    foreach ($p in $roots) {
        try {
            $hit = Get-ItemProperty -Path $p -ErrorAction SilentlyContinue |
                   Where-Object { $_.DisplayName -match 'Kinect.*(Runtime|SDK)' }
            if ($hit) { return $true }
        } catch { }
    }
    return $false
}
function Test-GameRunning { return [bool](Get-Process -Name 'legacy' -ErrorAction SilentlyContinue) }

$script:RuntimeDetected = Test-KinectRuntime

# ---------------------------------------------------------------------------
# game-folder inspection
# ---------------------------------------------------------------------------
function Get-State([string]$gf) {
    $r = [ordered]@{ folder = $gf; state = 'none'; detail = 'Choose your game folder.'; fs = $null; instVer = $null }
    if ([string]::IsNullOrWhiteSpace($gf)) { return $r }
    if (-not (Test-Path -LiteralPath $gf)) {
        $r.detail = "That folder doesn't exist - choose your game folder."; return $r
    }
    if (-not (Test-Path -LiteralPath (Join-Path $gf 'legacy.exe'))) {
        $r.state = 'nogame'; $r.detail = 'No legacy.exe in this folder - pick the folder that has the game in it.'
        return $r
    }
    $dll     = Join-Path $gf 'Kinect10.dll'
    $backend = Join-Path $gf 'Kinect10_backend.dll'
    if (Test-Path -LiteralPath $backend) {
        $r.state = 'installed'; $r.detail = 'KinectNavigator is installed here.'
        $r.instVer = Get-DllVersion $dll
    }
    elseif (-not (Test-Path -LiteralPath $dll)) {
        $r.state = 'nodll'; $r.detail = 'No Kinect10.dll here - is this the right folder?'
    }
    else {
        $sz = (Get-Item -LiteralPath $dll).Length
        if ($sz -ge $MIN_GENUINE) {
            $r.state = 'genuine'; $r.detail = ('Genuine Kinect runtime found ({0:N1} MB). Ready to install.' -f ($sz / 1MB))
        } else {
            $r.state = 'foreign'
            $r.detail = ('The Kinect10.dll here is only {0:N0} KB - not the genuine ~15 MB runtime. ' -f ($sz / 1KB)) +
                        'If you drive the game with a webcam emulator, KinectNavigator cannot sit on top of it.'
        }
    }
    $cfg = Join-Path $gf 'config.xml'
    if (Test-Path -LiteralPath $cfg) {
        $t = Get-Content -LiteralPath $cfg -Raw
        if ($t -match 'FullScreen\s*=\s*"1"') { $r.fs = 1 }
        elseif ($t -match 'FullScreen\s*=\s*"0"') { $r.fs = 0 }
    }
    return $r
}

# ---------------------------------------------------------------------------
# kinectnav.ini read / write
# ---------------------------------------------------------------------------

# Parse the ini into @{ key = value } for ACTIVE (non-comment) lines only.
# A "# key = value" template line counts as NOT set.
function Read-IniMap([string]$iniPath) {
    $m = @{}
    if ([string]::IsNullOrWhiteSpace($iniPath) -or -not (Test-Path -LiteralPath $iniPath)) { return $m }
    foreach ($raw in @(Get-Content -LiteralPath $iniPath)) {
        $line = ([string]$raw).Trim()
        if ($line -eq '' -or $line.StartsWith('#') -or $line.StartsWith(';')) { continue }
        $i = $line.IndexOf('=')
        if ($i -lt 1) { continue }
        $k = $line.Substring(0, $i).Trim().ToLower()
        $v = $line.Substring($i + 1).Trim()
        $c = $v.IndexOfAny([char[]]@(';', '#'))
        if ($c -ge 0) { $v = $v.Substring(0, $c).Trim() }
        if ($k) { $m[$k] = $v }
    }
    return $m
}

function Get-IniVal($map, [string]$key, $default) {
    $k = $key.ToLower()
    if ($map.ContainsKey($k)) { return $map[$k] }
    return $default
}

function IniBool($v) {
    $s = ([string]$v).Trim()
    $n = 0
    if ([int]::TryParse($s, [ref]$n)) { return ($n -ne 0) }
    return ($s -match '^(true|yes|on)$')
}

function NormNum($v) {
    $d = 0.0
    if ([double]::TryParse(([string]$v).Trim(), [ref]$d)) { return $d }
    return ([string]$v).Trim().ToLower()
}

# Set (or uncomment-and-set, or append) one key. Collapses any duplicates /
# commented template lines for that key to a single active line.
function Set-IniKey([string]$iniPath, [string]$key, [string]$value) {
    $line = "$key = $value"
    $pat  = '^\s*#?\s*' + [regex]::Escape($key) + '\s*='
    if (Test-Path -LiteralPath $iniPath) {
        $lines = @(Get-Content -LiteralPath $iniPath)
        $done  = $false
        $out   = foreach ($l in $lines) {
            if ($l -match $pat) {
                if (-not $done) { $done = $true; $line }
            } else { $l }
        }
        if (-not $done) { $out = @($out) + $line }
        Set-Content -LiteralPath $iniPath -Value $out -Encoding ASCII
    } else {
        Set-Content -LiteralPath $iniPath -Value @(
            '# KinectNavigator config -- see kinectnav.example.ini for every option.'
            $line
        ) -Encoding ASCII
    }
}

function Remove-IniKeys([string]$iniPath, [string[]]$keys) {
    if (-not (Test-Path -LiteralPath $iniPath)) { return }
    $lines = @(Get-Content -LiteralPath $iniPath)
    $out = foreach ($l in $lines) {
        $hit = $false
        foreach ($k in $keys) {
            if ($l -match ('^\s*#?\s*' + [regex]::Escape($k) + '\s*=')) { $hit = $true; break }
        }
        if (-not $hit) { $l }
    }
    Set-Content -LiteralPath $iniPath -Value $out -Encoding ASCII
}

# Return the preset name whose every value matches the ini (unset = its default),
# or $null if none match exactly.
function Test-PresetMatch($ini, $names, $table, $defs) {
    foreach ($n in $names) {
        $ok = $true
        foreach ($kv in $table[$n].GetEnumerator()) {
            $cur = Get-IniVal $ini $kv.Key $defs[$kv.Key]
            if ((NormNum $cur) -ne (NormNum $kv.Value)) { $ok = $false; break }
        }
        if ($ok) { return $n }
    }
    return $null
}

function ConvertTo-Vk([string]$s) {
    if ($null -eq $s) { return $null }
    $s = $s.Trim()
    if ($s -match '\(0x([0-9A-Fa-f]{1,2})\)') { return [Convert]::ToInt32($Matches[1], 16) }
    if ($s -match '^0[xX][0-9A-Fa-f]{1,2}$')  { return [Convert]::ToInt32($s, 16) }
    $n = 0
    if ([int]::TryParse($s, [ref]$n)) { return $n }
    return $null
}
function VkName([int]$vk) {
    $n = $null
    try { $n = [Enum]::GetName([System.Windows.Forms.Keys], $vk) } catch { }
    if ($n) { return ('{0}   (0x{1:X2})' -f $n, $vk) }
    return ('0x{0:X2}' -f $vk)
}

# fill the six key-binding textboxes from the ini (hoisted so the dialog's
# event handlers can call it -- $iniPath / $boxes come in as arguments)
function Update-KeyBoxes($iniPath, $boxes) {
    $m = Read-IniMap $iniPath
    foreach ($nm in $KEY_ORDER) {
        $def = $KEY_DEFS[$nm]
        $cur = Get-IniVal $m $def[0] $def[1]
        $vk  = ConvertTo-Vk ([string]$cur)
        $boxes[$nm].Text = $(if ($null -ne $vk) { VkName $vk } else { [string]$cur })
    }
}

function Set-ConfigWindowed([string]$gf) {
    $cfg = Join-Path $gf 'config.xml'
    if (-not (Test-Path -LiteralPath $cfg)) { return $false }
    try {
        $t = [System.IO.File]::ReadAllText($cfg)
        $n = [regex]::Replace($t, 'FullScreen\s*=\s*"1"', 'FullScreen="0"')
        if ($n -ne $t) { [System.IO.File]::WriteAllText($cfg, $n); return $true }
    } catch { }
    return $false
}

function Save-LastPath([string]$gf) {
    try {
        if (-not (Test-Path -LiteralPath $StateDir)) { New-Item -ItemType Directory -Path $StateDir -Force | Out-Null }
        Set-Content -LiteralPath $LastPathFile -Value $gf -Encoding UTF8
    } catch { }
}
function Load-LastPath {
    try { if (Test-Path -LiteralPath $LastPathFile) { return (Get-Content -LiteralPath $LastPathFile -Raw).Trim() } } catch { }
    return ''
}

# ---------------------------------------------------------------------------
# form
# ---------------------------------------------------------------------------
$Form = New-Object System.Windows.Forms.Form
$Form.Text = 'KinectNavigator - Setup'
$Form.ClientSize = New-Object System.Drawing.Size(600, 612)
$Form.StartPosition = 'CenterScreen'
$Form.FormBorderStyle = 'FixedSingle'
$Form.MaximizeBox = $false
$Form.BackColor = $ColBg
$Form.Font = $FontBase

$lblTitle = New-Object System.Windows.Forms.Label
$lblTitle.Text = 'KinectNavigator'; $lblTitle.Font = $FontTitle; $lblTitle.ForeColor = $ColText
$lblTitle.SetBounds(20, 16, 400, 30); $Form.Controls.Add($lblTitle)

$lblSub = New-Object System.Windows.Forms.Label
$lblSub.Text = 'Hands-free Kinect menu navigation for Just Dance Legacy Offline PC'
$lblSub.ForeColor = $ColMuted; $lblSub.SetBounds(22, 48, 460, 20); $Form.Controls.Add($lblSub)

$lblVer = New-Object System.Windows.Forms.Label
$lblVer.Text = $(if ($ShimVer) { 'installer  v' + (VerShort $ShimVer) } else { '' })
$lblVer.ForeColor = $ColMuted; $lblVer.TextAlign = 'TopRight'
$lblVer.SetBounds(360, 22, 220, 18); $Form.Controls.Add($lblVer)

$lblGF = New-Object System.Windows.Forms.Label
$lblGF.Text = 'Game folder'; $lblGF.Font = $FontBold; $lblGF.ForeColor = $ColText
$lblGF.SetBounds(20, 86, 120, 20); $Form.Controls.Add($lblGF)

$txtGF = New-Object System.Windows.Forms.TextBox
$txtGF.SetBounds(20, 108, 455, 24); $txtGF.Font = $FontBase; $Form.Controls.Add($txtGF)

$btnBrowse = New-Btn 'Browse...' 483 107 97 26
$Form.Controls.Add($btnBrowse)

$pnl = New-Object System.Windows.Forms.Panel
$pnl.SetBounds(20, 148, 560, 112); $pnl.BackColor = $ColCard; $pnl.BorderStyle = 'FixedSingle'
$Form.Controls.Add($pnl)
$lblStatus = New-Object System.Windows.Forms.Label
$lblStatus.SetBounds(14, 10, 532, 92); $lblStatus.Font = $FontBase; $lblStatus.ForeColor = $ColText
$pnl.Controls.Add($lblStatus)

$lnkRuntime = New-Object System.Windows.Forms.LinkLabel
$lnkRuntime.SetBounds(20, 268, 560, 20); $lnkRuntime.Font = $FontBase
$lnkRuntime.LinkBehavior = [System.Windows.Forms.LinkBehavior]::HoverUnderline
$Form.Controls.Add($lnkRuntime)
$lnkRuntime.Add_LinkClicked({ Start-Process $RuntimeUrl })
if ($script:RuntimeDetected) {
    $lnkRuntime.Text = 'Kinect for Windows Runtime: detected'
    $lnkRuntime.LinkColor = $ColMuted; $lnkRuntime.ActiveLinkColor = $ColMuted; $lnkRuntime.DisabledLinkColor = $ColMuted
    $lnkRuntime.Enabled = $false
} else {
    $lnkRuntime.Text = 'Kinect for Windows Runtime: not detected - click to download it'
    $lnkRuntime.LinkColor = $ColWarn
}

# ---- settings panel (live editor for kinectnav.ini in the game folder) ----
$lblOpt = New-Object System.Windows.Forms.Label
$lblOpt.Text = 'Settings  -  saved to kinectnav.ini in the game folder, applied next launch'
$lblOpt.Font = $FontBold; $lblOpt.ForeColor = $ColText
$lblOpt.SetBounds(20, 296, 560, 20); $Form.Controls.Add($lblOpt)

$pnlOpt = New-Object System.Windows.Forms.Panel
$pnlOpt.SetBounds(20, 318, 560, 112); $pnlOpt.BackColor = $ColCard; $pnlOpt.BorderStyle = 'FixedSingle'
$Form.Controls.Add($pnlOpt)

$lblHand = New-Object System.Windows.Forms.Label
$lblHand.Text = 'Navigation hand:'; $lblHand.ForeColor = $ColText
$lblHand.SetBounds(14, 15, 108, 20); $pnlOpt.Controls.Add($lblHand)

$cmbHand = New-Object System.Windows.Forms.ComboBox
$cmbHand.DropDownStyle = 'DropDownList'
$cmbHand.SetBounds(124, 12, 170, 24)
[void]$cmbHand.Items.Add('Right hand')
[void]$cmbHand.Items.Add('Left hand')
$pnlOpt.Controls.Add($cmbHand)

$chkMirror = New-Object System.Windows.Forms.CheckBox
$chkMirror.Text = 'Reverse left / right'; $chkMirror.ForeColor = $ColText
$chkMirror.SetBounds(320, 13, 230, 22); $pnlOpt.Controls.Add($chkMirror)

$chkBack = New-Object System.Windows.Forms.CheckBox
$chkBack.Text = 'Enable the Back gesture (Esc)'; $chkBack.ForeColor = $ColText
$chkBack.SetBounds(14, 44, 290, 22); $pnlOpt.Controls.Add($chkBack)

$chkHud = New-Object System.Windows.Forms.CheckBox
$chkHud.AutoSize = $false
$chkHud.Text = 'Show the on-screen HUD  (windowed only)'; $chkHud.ForeColor = $ColText
$chkHud.SetBounds(320, 44, 230, 22); $pnlOpt.Controls.Add($chkHud)

$btnMore = New-Btn 'More settings...' 14 74 130 27
$btnOpenIni = New-Btn 'Open kinectnav.ini' 152 74 160 27
$btnResetCfg = New-Btn 'Reset to defaults' 320 74 150 27
$pnlOpt.Controls.AddRange(@($btnMore, $btnOpenIni, $btnResetCfg))

$btnInstall   = New-Btn 'Install'   20 444 130 34 $true
$btnUninstall = New-Btn 'Uninstall' 160 444 130 34
$btnHelp      = New-Btn 'Gestures'  350 444 100 34
$btnClose     = New-Btn 'Close'     460 444 100 34
$Form.Controls.AddRange(@($btnInstall, $btnUninstall, $btnHelp, $btnClose))

$txtLog = New-Object System.Windows.Forms.TextBox
$txtLog.SetBounds(20, 488, 560, 104); $txtLog.Multiline = $true; $txtLog.ReadOnly = $true
$txtLog.ScrollBars = 'Vertical'; $txtLog.BackColor = [System.Drawing.Color]::White; $txtLog.Font = $FontMono
$Form.Controls.Add($txtLog)

function Write-Log([string]$s) {
    $txtLog.AppendText($s + "`r`n")
    $txtLog.SelectionStart = $txtLog.TextLength; $txtLog.ScrollToCaret()
}

function Get-IniPath {
    $st = $script:CurState
    if ($null -eq $st) { return $null }
    if ($st.state -ne 'genuine' -and $st.state -ne 'installed') { return $null }
    return (Join-Path $st.folder 'kinectnav.ini')
}

function Populate-Config([string]$gf) {
    $script:Loading = $true
    try {
        $ini = Read-IniMap (Join-Path $gf 'kinectnav.ini')
        $h = ([string](Get-IniVal $ini 'handedness' 'right')).Trim().ToLower()
        $cmbHand.SelectedIndex = $(if ($h -eq 'left' -or $h -eq '1') { 1 } else { 0 })
        $chkMirror.Checked = (IniBool (Get-IniVal $ini 'mirror' '0'))
        $chkBack.Checked   = (IniBool (Get-IniVal $ini 'enable_back' '1'))
        $chkHud.Checked    = (IniBool (Get-IniVal $ini 'overlay' '0'))
    } finally {
        $script:Loading = $false
    }
}

function Refresh-Status {
    $st = Get-State ($txtGF.Text.Trim())
    $script:CurState = $st
    $lines = @()
    switch ($st.state) {
        'genuine'   { $lines += '[ OK ]  ' + $st.detail }
        'installed' { $lines += '[ OK ]  ' + $st.detail }
        'foreign'   { $lines += '[ !! ]  ' + $st.detail }
        'nogame'    { $lines += '[ X  ]  ' + $st.detail }
        'nodll'     { $lines += '[ X  ]  ' + $st.detail }
        default     { $lines += '        ' + $st.detail }
    }

    $canInstall = ($st.state -eq 'genuine' -or $st.state -eq 'installed')
    $btnUninstall.Enabled = ($st.state -eq 'installed')

    # Install / Update button + version line
    if ($st.state -eq 'installed') {
        $iv = $st.instVer
        if ($null -ne $ShimVer -and $null -ne $iv) {
            if ($ShimVer -gt $iv) {
                $btnInstall.Text = 'Update'; $btnInstall.Enabled = $true
                $lines += ('[ i  ]  Update available - installed v{0}, this download is v{1}.' -f (VerShort $iv), (VerShort $ShimVer))
            } elseif ($ShimVer -eq $iv) {
                $btnInstall.Text = 'Up to date'; $btnInstall.Enabled = $false
                $lines += ('[ OK ]  KinectNavigator v{0} is installed and up to date.' -f (VerShort $iv))
            } else {
                $btnInstall.Text = 'Reinstall'; $btnInstall.Enabled = $true
                $lines += ('[ !! ]  This download is v{0} - OLDER than the installed v{1}.' -f (VerShort $ShimVer), (VerShort $iv))
            }
        } else {
            $btnInstall.Text = 'Update'; $btnInstall.Enabled = $true
            $lines += '[ i  ]  Click Update to replace the installed DLL with this download (its version could not be read).'
        }
    } else {
        $btnInstall.Text = 'Install'; $btnInstall.Enabled = $canInstall
    }

    if ($st.fs -eq 1)     { $lines += '[ i  ]  config.xml is set to fullscreen - the HUD stays hidden unless you set FullScreen="0".' }
    elseif ($st.fs -eq 0) { $lines += '[ i  ]  config.xml is windowed - the HUD can show.' }
    $lblStatus.Text = ($lines -join "`r`n")

    foreach ($c in @($cmbHand, $chkMirror, $chkBack, $chkHud, $btnMore, $btnOpenIni, $btnResetCfg)) {
        $c.Enabled = $canInstall
    }
    if ($canInstall) {
        Save-LastPath $st.folder
        Populate-Config $st.folder
    }
}

$btnBrowse.Add_Click({
    $d = New-Object System.Windows.Forms.FolderBrowserDialog
    $d.Description = 'Pick the folder that has legacy.exe in it'
    $d.ShowNewFolderButton = $false
    $cur = $txtGF.Text.Trim()
    if ($cur -and (Test-Path -LiteralPath $cur)) { $d.SelectedPath = $cur }
    if ($d.ShowDialog($Form) -eq [System.Windows.Forms.DialogResult]::OK) {
        $txtGF.Text = $d.SelectedPath.TrimEnd('\')
    }
})
$txtGF.Add_TextChanged({ Refresh-Status })

# ---- live config handlers (main window) ----
$cmbHand.Add_SelectedIndexChanged({
    if ($script:Loading) { return }
    $ini = Get-IniPath; if ($null -eq $ini) { return }
    $val = if ($cmbHand.SelectedIndex -eq 1) { 'left' } else { 'right' }
    Set-IniKey $ini 'handedness' $val
    Write-Log "kinectnav.ini: handedness = $val"
})
$chkMirror.Add_CheckedChanged({
    if ($script:Loading) { return }
    $ini = Get-IniPath; if ($null -eq $ini) { return }
    $v = if ($chkMirror.Checked) { '1' } else { '0' }
    Set-IniKey $ini 'mirror' $v
    Write-Log "kinectnav.ini: mirror = $v"
})
$chkBack.Add_CheckedChanged({
    if ($script:Loading) { return }
    $ini = Get-IniPath; if ($null -eq $ini) { return }
    $v = if ($chkBack.Checked) { '1' } else { '0' }
    Set-IniKey $ini 'enable_back' $v
    Write-Log "kinectnav.ini: enable_back = $v"
})
$chkHud.Add_CheckedChanged({
    if ($script:Loading) { return }
    $st = $script:CurState
    $ini = Get-IniPath; if ($null -eq $ini) { return }
    if ($chkHud.Checked) {
        Set-IniKey $ini 'overlay' '1'
        Write-Log 'kinectnav.ini: overlay = 1 (HUD on)'
        if ($st.fs -eq 1) {
            $r = [System.Windows.Forms.MessageBox]::Show($Form,
                'The HUD only shows in windowed mode, and config.xml is set to fullscreen.' +
                [Environment]::NewLine + [Environment]::NewLine +
                'Set FullScreen="0" in config.xml now?',
                'KinectNavigator Setup', 'YesNo', 'Question')
            if ($r -eq [System.Windows.Forms.DialogResult]::Yes) {
                if (Set-ConfigWindowed $st.folder) { Write-Log 'config.xml: FullScreen="0"' }
                else { Write-Log 'config.xml: could not change it - edit FullScreen="0" by hand.' }
                Refresh-Status
            }
        }
    } else {
        Set-IniKey $ini 'overlay' '0'
        Write-Log 'kinectnav.ini: overlay = 0 (HUD off)'
    }
})

$btnOpenIni.Add_Click({
    $ini = Get-IniPath; if ($null -eq $ini) { return }
    if (-not (Test-Path -LiteralPath $ini)) {
        if (Test-Path -LiteralPath $ExampleIni) {
            Copy-Item -LiteralPath $ExampleIni -Destination $ini -Force
            Write-Log 'Created kinectnav.ini from the example template.'
        } else {
            Set-IniKey $ini 'nav_model' 'extend'
        }
    }
    Start-Process notepad.exe -ArgumentList $ini
    Write-Log 'Opened kinectnav.ini - re-pick the game folder afterward to reload these controls.'
})

$btnResetCfg.Add_Click({
    $ini = Get-IniPath; if ($null -eq $ini) { return }
    if (-not (Test-Path -LiteralPath $ini)) { Write-Log 'No kinectnav.ini - already at defaults.'; return }
    $r = [System.Windows.Forms.MessageBox]::Show($Form,
        'Delete kinectnav.ini and return every setting to its default?',
        'KinectNavigator Setup', 'YesNo', 'Warning')
    if ($r -eq [System.Windows.Forms.DialogResult]::Yes) {
        Remove-Item -LiteralPath $ini -Force
        Write-Log 'kinectnav.ini deleted - all settings back to defaults.'
        Refresh-Status
    }
})

# ---------------------------------------------------------------------------
# "press a key" capture dialog
# ---------------------------------------------------------------------------
function Capture-Key {
    $d = New-Object System.Windows.Forms.Form
    $d.Text = 'Press a key'
    $d.ClientSize = New-Object System.Drawing.Size(340, 128)
    $d.FormBorderStyle = 'FixedDialog'; $d.StartPosition = 'CenterParent'
    $d.MaximizeBox = $false; $d.MinimizeBox = $false; $d.KeyPreview = $true
    $d.BackColor = $ColBg; $d.Font = $FontBase
    $l = New-Object System.Windows.Forms.Label
    $l.Text = 'Press the key you want to use for this action.'
    $l.SetBounds(16, 16, 308, 44); $d.Controls.Add($l)
    $c = New-Btn 'Cancel' 232 78 92 28
    $c.Add_Click({ $script:capVk = $null; $d.Close() })
    $d.Controls.Add($c)
    $script:capVk = $null
    $ignore = @(16, 17, 18, 91, 92, 20, 144, 145)   # shift/ctrl/alt/win/capslock/numlock/scroll
    $d.Add_KeyDown({
        $vk = [int]$_.KeyCode
        if ($ignore -contains $vk) { return }
        $script:capVk = $vk
        $_.SuppressKeyPress = $true
        $d.Close()
    })
    $d.ShowDialog($Form) | Out-Null
    return $script:capVk
}

# ---------------------------------------------------------------------------
# "more settings" dialog (presets + clutch + key bindings)
# ---------------------------------------------------------------------------
function Show-MoreSettings {
    $ini = Get-IniPath
    if ($null -eq $ini) { return }
    $iniMap = Read-IniMap $ini

    $d = New-Object System.Windows.Forms.Form
    $d.Text = 'KinectNavigator - more settings'
    $d.ClientSize = New-Object System.Drawing.Size(474, 520)
    $d.FormBorderStyle = 'FixedDialog'; $d.StartPosition = 'CenterParent'
    $d.MaximizeBox = $false; $d.MinimizeBox = $false
    $d.BackColor = $ColBg; $d.Font = $FontBase

    $script:dlgLoading = $true

    $lf = New-Object System.Windows.Forms.Label
    $lf.Text = 'Feel  -  each row picks a preset, saved to kinectnav.ini'
    $lf.Font = $FontBold; $lf.SetBounds(16, 14, 444, 20); $d.Controls.Add($lf)

    function New-PresetRow([string]$text, [int]$y, [string[]]$items) {
        $lbl = New-Object System.Windows.Forms.Label
        $lbl.Text = $text; $lbl.SetBounds(16, ($y + 3), 210, 20); $d.Controls.Add($lbl)
        $cb = New-Object System.Windows.Forms.ComboBox
        $cb.DropDownStyle = 'DropDownList'; $cb.SetBounds(232, $y, 226, 24)
        foreach ($it in $items) { [void]$cb.Items.Add($it) }
        $d.Controls.Add($cb)
        return $cb
    }

    $cbReach  = New-PresetRow 'Reach to navigate'         40  $REACH_NAMES
    $cbScroll = New-PresetRow 'Scroll speed when held'    72  $SCROLL_NAMES
    $cbCmd    = New-PresetRow 'Command-mode reach'       104  $CMDREACH_NAMES
    $cbHold   = New-PresetRow 'Hold time for Enter/Esc'  136  $HOLD_NAMES

    $chkClutch = New-Object System.Windows.Forms.CheckBox
    $chkClutch.Text = 'Require waking it up first  (park hand at the shoulder to arm)'
    $chkClutch.SetBounds(16, 174, 444, 22); $d.Controls.Add($chkClutch)

    $lk = New-Object System.Windows.Forms.Label
    $lk.Text = 'Key bindings'; $lk.Font = $FontBold; $lk.SetBounds(16, 210, 444, 20); $d.Controls.Add($lk)

    $keyBoxes = @{}
    $ri = 0
    foreach ($nm in $KEY_ORDER) {
        $ky = 236 + $ri * 30
        $lb = New-Object System.Windows.Forms.Label
        $lb.Text = $nm; $lb.SetBounds(16, ($ky + 4), 84, 20); $d.Controls.Add($lb)
        $tb = New-Object System.Windows.Forms.TextBox
        $tb.ReadOnly = $true; $tb.SetBounds(104, $ky, 226, 24); $tb.Font = $FontMono
        $d.Controls.Add($tb); $keyBoxes[$nm] = $tb
        $bc = New-Btn 'Change...' 338 ($ky - 1) 120 26
        $bc.Tag = $nm
        $bc.Add_Click({
            $name = $this.Tag
            $vk = Capture-Key
            if ($null -ne $vk) {
                Set-IniKey $ini $KEY_DEFS[$name][0] ('0x{0:X2}' -f $vk)
                $keyBoxes[$name].Text = (VkName $vk)
                Write-Log ('kinectnav.ini: {0} = 0x{1:X2}' -f $KEY_DEFS[$name][0], $vk)
            }
        }.GetNewClosure())
        $d.Controls.Add($bc)
        $ri++
    }

    $bResetKeys = New-Btn 'Reset keys' 16 430 110 28
    $bResetKeys.Add_Click({
        Remove-IniKeys $ini @('key_left', 'key_right', 'key_up', 'key_down', 'key_confirm', 'key_back')
        Update-KeyBoxes $ini $keyBoxes
        Write-Log 'kinectnav.ini: key bindings reset to defaults'
    }.GetNewClosure())

    $bOk = New-Btn 'Close' 384 472 74 28 $true
    $bOk.Add_Click({ $d.Close() })
    $d.Controls.AddRange(@($bResetKeys, $bOk))

    # --- populate ---
    function Fill-Preset($cb, $names, $table) {
        $cb.Items.Clear()
        foreach ($n in $names) { [void]$cb.Items.Add($n) }
        $m = Test-PresetMatch $iniMap $names $table $CFG_DEFAULTS
        if ($m) { $cb.SelectedItem = $m }
        else { [void]$cb.Items.Add('(custom)'); $cb.SelectedItem = '(custom)' }
    }
    Fill-Preset $cbReach  $REACH_NAMES    $PRESET_REACH
    Fill-Preset $cbScroll $SCROLL_NAMES   $PRESET_SCROLL
    Fill-Preset $cbCmd    $CMDREACH_NAMES $PRESET_CMDREACH
    Fill-Preset $cbHold   $HOLD_NAMES     $PRESET_HOLD
    $chkClutch.Checked = (IniBool (Get-IniVal $iniMap 'dpad_arm' '1'))
    Update-KeyBoxes $ini $keyBoxes

    function Make-PresetHandler($cb, $table, $label) {
        return {
            if ($script:dlgLoading) { return }
            $sel = [string]$cb.SelectedItem
            if ($sel -eq '' -or $sel -eq '(custom)') { return }
            foreach ($kv in $table[$sel].GetEnumerator()) { Set-IniKey $ini $kv.Key ([string]$kv.Value) }
            if ($cb.Items.Contains('(custom)')) { $cb.Items.Remove('(custom)') }
            Write-Log ('kinectnav.ini: {0} preset = {1}' -f $label, $sel)
        }.GetNewClosure()
    }
    $cbReach.Add_SelectedIndexChanged( (Make-PresetHandler $cbReach  $PRESET_REACH    'reach') )
    $cbScroll.Add_SelectedIndexChanged((Make-PresetHandler $cbScroll $PRESET_SCROLL   'scroll speed') )
    $cbCmd.Add_SelectedIndexChanged(   (Make-PresetHandler $cbCmd    $PRESET_CMDREACH 'command reach') )
    $cbHold.Add_SelectedIndexChanged(  (Make-PresetHandler $cbHold   $PRESET_HOLD     'hold time') )
    $chkClutch.Add_CheckedChanged({
        if ($script:dlgLoading) { return }
        $v = if ($chkClutch.Checked) { '1' } else { '0' }
        Set-IniKey $ini 'dpad_arm' $v
        Write-Log "kinectnav.ini: dpad_arm = $v"
    }.GetNewClosure())

    $script:dlgLoading = $false
    $d.ShowDialog($Form) | Out-Null
}
$btnMore.Add_Click({ Show-MoreSettings })

# ---------------------------------------------------------------------------
# install / uninstall
# ---------------------------------------------------------------------------
$btnInstall.Add_Click({
    $st = $script:CurState
    if ($null -eq $st -or ($st.state -ne 'genuine' -and $st.state -ne 'installed')) { return }
    $gf  = $st.folder
    $dll = Join-Path $gf 'Kinect10.dll'

    if (Test-GameRunning) {
        Msg 'Legacy is currently running. Close the game first, then try again.' 'Warning'
        return
    }
    try {
        $isUpdate = ($st.state -eq 'installed')
        if (-not $isUpdate) {
            Copy-Item -LiteralPath $dll -Destination (Join-Path $gf 'Kinect10.dll.orig-backup') -Force
            Rename-Item -LiteralPath $dll -NewName 'Kinect10_backend.dll'
            Write-Log 'Renamed the genuine runtime to Kinect10_backend.dll (backup: Kinect10.dll.orig-backup).'
        }
        Copy-Item -LiteralPath $ShimSrc -Destination $dll -Force

        $srcLen = (Get-Item -LiteralPath $ShimSrc).Length
        $dstLen = (Get-Item -LiteralPath $dll).Length
        if ($dstLen -ne $srcLen) {
            throw "Kinect10.dll copied but landed at $dstLen bytes, not $srcLen -- antivirus may have altered it. Check your antivirus quarantine / add a folder exclusion, then try again."
        }
        Write-Log $(if ($isUpdate) { 'Updated the installed Kinect10.dll.' } else { 'Installed KinectNavigator as Kinect10.dll.' })
        Refresh-Status

        $extra = ''
        if ($script:CurState.fs -eq 1 -and $chkHud.Checked) {
            $extra = [Environment]::NewLine + [Environment]::NewLine +
                     'Note: set FullScreen="0" in config.xml if you want to see the HUD.'
        }
        $head = if ($isUpdate) { 'KinectNavigator is updated.' } else { 'KinectNavigator is installed.' }
        Msg ($head + [Environment]::NewLine + [Environment]::NewLine +
             'Launch the game normally. Stand about 2.5 m back and rest a hand near your shoulder to wake it. ' +
             "Click 'Gestures' above any time for the how-to." + $extra) 'Information'
    } catch {
        Write-Log ('ERROR: ' + $_.Exception.Message)
        Msg ('Install failed:' + [Environment]::NewLine + $_.Exception.Message) 'Error'
        Refresh-Status
    }
})

$btnUninstall.Add_Click({
    $st = $script:CurState
    if ($null -eq $st -or $st.state -ne 'installed') { return }
    $gf = $st.folder
    if (Test-GameRunning) {
        Msg 'Legacy is currently running. Close the game first, then try again.' 'Warning'
        return
    }
    try {
        $dll     = Join-Path $gf 'Kinect10.dll'
        $backend = Join-Path $gf 'Kinect10_backend.dll'
        if (Test-Path -LiteralPath $dll) { Remove-Item -LiteralPath $dll -Force }
        Rename-Item -LiteralPath $backend -NewName 'Kinect10.dll'
        if ((Get-Item -LiteralPath (Join-Path $gf 'Kinect10.dll')).Length -lt $MIN_GENUINE) {
            throw 'Kinect10.dll is back but looks too small -- something is wrong. Check the game folder by hand.'
        }
        $bak = Join-Path $gf 'Kinect10.dll.orig-backup'
        if (Test-Path -LiteralPath $bak) { Remove-Item -LiteralPath $bak -Force }
        Write-Log 'Removed the shim and restored the genuine Kinect10.dll.'
        Write-Log 'Left kinectnav.ini / KinectNavigator.log / skcap-*.skcap in place.'
        Refresh-Status
        Msg 'KinectNavigator removed. The original Kinect10.dll is back.' 'Information'
    } catch {
        Write-Log ('ERROR: ' + $_.Exception.Message)
        Msg ('Uninstall failed:' + [Environment]::NewLine + $_.Exception.Message) 'Error'
        Refresh-Status
    }
})

# ---------------------------------------------------------------------------
# gestures help
# ---------------------------------------------------------------------------
$GesturesImg = Join-Path $ScriptDir 'gestures.png'

function Show-GesturesText {
    Msg (
        "WAKING IT UP" + [Environment]::NewLine +
        "Rest your dominant hand near your shoulder for a moment. It sleeps again if your" + [Environment]::NewLine +
        "arm just hangs or you dance, so it won't fire mid-routine." + [Environment]::NewLine + [Environment]::NewLine +
        "NAVIGATING - a small + centred on your dominant shoulder" + [Environment]::NewLine +
        "  reach out to the side ......... Left / Right" + [Environment]::NewLine +
        "  reach up ....................... Up" + [Environment]::NewLine +
        "  reach down-and-out ............. Down" + [Environment]::NewLine +
        "  hold the reach ................. repeats (speeds up)" + [Environment]::NewLine +
        "  bend the elbow / pull back ..... stops" + [Environment]::NewLine + [Environment]::NewLine +
        "CONFIRM / BACK" + [Environment]::NewLine +
        "  put your OTHER hand on your OTHER shoulder, then:" + [Environment]::NewLine +
        "  reach up or right and hold ..... Enter" + [Environment]::NewLine +
        "  reach down or left and hold .... Esc" + [Environment]::NewLine + [Environment]::NewLine +
        "Stand about 2.5 m back, centred, facing the sensor." + [Environment]::NewLine +
        "Full details and troubleshooting are in SETUP.md."
    ) 'Information'
}

$btnHelp.Add_Click({
    if (-not (Test-Path -LiteralPath $GesturesImg)) { Show-GesturesText; return }
    try {
        $img = [System.Drawing.Image]::FromFile($GesturesImg)
    } catch { Show-GesturesText; return }
    $gf = New-Object System.Windows.Forms.Form
    $gf.Text = 'KinectNavigator - gestures'
    $gf.StartPosition = 'CenterParent'
    $gf.FormBorderStyle = 'FixedSingle'
    $gf.MaximizeBox = $false
    $gf.BackColor = $ColBg
    $maxW = 900
    $sc = [Math]::Min(1.0, $maxW / $img.Width)
    $iw = [int]($img.Width * $sc); $ih = [int]($img.Height * $sc)
    $gf.ClientSize = New-Object System.Drawing.Size ($iw + 24), ($ih + 60)
    $pb2 = New-Object System.Windows.Forms.PictureBox
    $pb2.SetBounds(12, 12, $iw, $ih)
    $pb2.SizeMode = 'Zoom'
    $pb2.Image = $img
    $gf.Controls.Add($pb2)
    $ok = New-Btn 'Close' ($iw + 24 - 112) ($ih + 20) 100 30
    $ok.Add_Click({ $gf.Close() })
    $gf.Controls.Add($ok)
    $gf.Add_FormClosed({ $img.Dispose() })
    $gf.ShowDialog($Form) | Out-Null
})

$btnClose.Add_Click({ $Form.Close() })
$Form.Add_Shown({ $Form.Activate(); $Form.TopMost = $true; $Form.TopMost = $false })

$seed = Load-LastPath
if ([string]::IsNullOrWhiteSpace($seed)) {
    foreach ($g in @($ScriptDir, (Split-Path -Parent $ScriptDir))) {
        if ($g -and (Test-Path -LiteralPath (Join-Path $g 'legacy.exe'))) { $seed = $g; break }
    }
}
$txtGF.Text = $seed
Refresh-Status

[void][System.Windows.Forms.Application]::Run($Form)
