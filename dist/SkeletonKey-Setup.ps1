# SkeletonKey-Setup.ps1 - windowed installer for Skeleton Key.
#
# Swaps the game's Kinect10.dll for the Skeleton Key shim (and back). Launched
# flash-free by SkeletonKey-Setup.vbs / .bat. Anything fatal here must surface
# as a MessageBox, not Write-Host.

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
$StateDir     = Join-Path $env:LOCALAPPDATA 'SkeletonKey'
$LastPathFile = Join-Path $StateDir 'gamefolder.txt'
$MIN_GENUINE  = 1000000            # the real runtime is ~15 MB
$RuntimeUrl   = 'https://www.microsoft.com/download/details.aspx?id=40277'

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
    [System.Windows.Forms.MessageBox]::Show($Form, $Text, 'Skeleton Key Setup',
        [System.Windows.Forms.MessageBoxButtons]::OK, $Icon) | Out-Null
}

if (-not (Test-Path -LiteralPath $ShimSrc)) {
    [System.Windows.Forms.MessageBox]::Show(
        "Kinect10.dll (the Skeleton Key shim) isn't next to this installer." + [Environment]::NewLine +
        "Extract the whole release together, then run it again.",
        'Skeleton Key Setup', 'OK', 'Error') | Out-Null
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
    $r = [ordered]@{ folder = $gf; state = 'none'; detail = 'Choose your game folder.'; fs = $null }
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
        $r.state = 'installed'; $r.detail = 'Skeleton Key is installed here.'
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
                        'If you drive the game with a webcam emulator, Skeleton Key cannot sit on top of it.'
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

function Set-IniKey([string]$iniPath, [string]$key, [string]$value) {
    $line = "$key = $value"
    if (Test-Path -LiteralPath $iniPath) {
        $lines = Get-Content -LiteralPath $iniPath
        $pat = '^\s*#?\s*' + [regex]::Escape($key) + '\s*='
        if ($lines -match $pat) {
            $lines = $lines | ForEach-Object { if ($_ -match $pat) { $line } else { $_ } }
        } else {
            $lines += $line
        }
        Set-Content -LiteralPath $iniPath -Value $lines -Encoding ASCII
    } else {
        Set-Content -LiteralPath $iniPath -Value @(
            '# Skeleton Key config -- see kinectnav.example.ini for every option.'
            $line
        ) -Encoding ASCII
    }
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
$Form.Text = 'Skeleton Key - Setup'
$Form.ClientSize = New-Object System.Drawing.Size(600, 534)
$Form.StartPosition = 'CenterScreen'
$Form.FormBorderStyle = 'FixedSingle'
$Form.MaximizeBox = $false
$Form.BackColor = $ColBg
$Form.Font = $FontBase

$lblTitle = New-Object System.Windows.Forms.Label
$lblTitle.Text = 'Skeleton Key'; $lblTitle.Font = $FontTitle; $lblTitle.ForeColor = $ColText
$lblTitle.SetBounds(20, 16, 400, 30); $Form.Controls.Add($lblTitle)

$lblSub = New-Object System.Windows.Forms.Label
$lblSub.Text = 'Hands-free Kinect menu navigation for Just Dance Legacy Offline PC'
$lblSub.ForeColor = $ColMuted; $lblSub.SetBounds(22, 48, 560, 20); $Form.Controls.Add($lblSub)

$lblGF = New-Object System.Windows.Forms.Label
$lblGF.Text = 'Game folder'; $lblGF.Font = $FontBold; $lblGF.ForeColor = $ColText
$lblGF.SetBounds(20, 86, 120, 20); $Form.Controls.Add($lblGF)

$txtGF = New-Object System.Windows.Forms.TextBox
$txtGF.SetBounds(20, 108, 455, 24); $txtGF.Font = $FontBase; $Form.Controls.Add($txtGF)

$btnBrowse = New-Btn 'Browse...' 483 107 97 26
$Form.Controls.Add($btnBrowse)

$pnl = New-Object System.Windows.Forms.Panel
$pnl.SetBounds(20, 148, 560, 132); $pnl.BackColor = $ColCard; $pnl.BorderStyle = 'FixedSingle'
$Form.Controls.Add($pnl)
$lblStatus = New-Object System.Windows.Forms.Label
$lblStatus.SetBounds(14, 12, 532, 108); $lblStatus.Font = $FontBase; $lblStatus.ForeColor = $ColText
$pnl.Controls.Add($lblStatus)

$lnkRuntime = New-Object System.Windows.Forms.LinkLabel
$lnkRuntime.SetBounds(20, 288, 560, 20); $lnkRuntime.Font = $FontBase
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

$chkHud = New-Object System.Windows.Forms.CheckBox
$chkHud.Text = 'Also enable the on-screen HUD  (writes overlay = 1; only shows when the game runs windowed)'
$chkHud.SetBounds(20, 314, 560, 20); $chkHud.ForeColor = $ColText; $Form.Controls.Add($chkHud)

$btnInstall   = New-Btn 'Install'   20 348 130 34 $true
$btnUninstall = New-Btn 'Uninstall' 160 348 130 34
$btnHelp      = New-Btn 'Gestures'  350 348 100 34
$btnClose     = New-Btn 'Close'     460 348 100 34
$Form.Controls.AddRange(@($btnInstall, $btnUninstall, $btnHelp, $btnClose))

$txtLog = New-Object System.Windows.Forms.TextBox
$txtLog.SetBounds(20, 396, 560, 118); $txtLog.Multiline = $true; $txtLog.ReadOnly = $true
$txtLog.ScrollBars = 'Vertical'; $txtLog.BackColor = [System.Drawing.Color]::White; $txtLog.Font = $FontMono
$Form.Controls.Add($txtLog)

function Write-Log([string]$s) {
    $txtLog.AppendText($s + "`r`n")
    $txtLog.SelectionStart = $txtLog.TextLength; $txtLog.ScrollToCaret()
}

$script:CurState = $null
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
    if ($st.fs -eq 1)     { $lines += '[ i  ]  config.xml is set to fullscreen - the HUD stays hidden unless you set FullScreen="0".' }
    elseif ($st.fs -eq 0) { $lines += '[ i  ]  config.xml is windowed - the HUD can show.' }
    $lblStatus.Text = ($lines -join "`r`n")

    $canInstall = ($st.state -eq 'genuine' -or $st.state -eq 'installed')
    $btnInstall.Enabled = $canInstall
    $btnInstall.Text    = if ($st.state -eq 'installed') { 'Update' } else { 'Install' }
    $btnUninstall.Enabled = ($st.state -eq 'installed')
    if ($canInstall) { Save-LastPath $st.folder }
}

$btnBrowse.Add_Click({
    $d = New-Object System.Windows.Forms.FolderBrowserDialog
    $d.Description = 'Pick the folder that has legacy.exe in it'
    $d.ShowNewFolderButton = $false
    if (Test-Path -LiteralPath $txtGF.Text.Trim()) { $d.SelectedPath = $txtGF.Text.Trim() }
    if ($d.ShowDialog($Form) -eq [System.Windows.Forms.DialogResult]::OK) {
        $txtGF.Text = $d.SelectedPath.TrimEnd('\')
    }
})
$txtGF.Add_TextChanged({ Refresh-Status })

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
        Write-Log $(if ($isUpdate) { 'Updated the installed Kinect10.dll.' } else { 'Installed Skeleton Key as Kinect10.dll.' })

        if ($chkHud.Checked) {
            Set-IniKey (Join-Path $gf 'kinectnav.ini') 'overlay' '1'
            Write-Log 'Set overlay = 1 in kinectnav.ini.'
        }
        Refresh-Status

        $extra = ''
        if ($script:CurState.fs -eq 1 -and $chkHud.Checked) {
            $extra = [Environment]::NewLine + [Environment]::NewLine +
                     'Note: set FullScreen="0" in config.xml if you want to see the HUD.'
        }
        $head = if ($isUpdate) { 'Skeleton Key is updated.' } else { 'Skeleton Key is installed.' }
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
        Write-Log 'Left kinectnav.ini / SkeletonKey.log / skcap-*.skcap in place.'
        Refresh-Status
        Msg 'Skeleton Key removed. The original Kinect10.dll is back.' 'Information'
    } catch {
        Write-Log ('ERROR: ' + $_.Exception.Message)
        Msg ('Uninstall failed:' + [Environment]::NewLine + $_.Exception.Message) 'Error'
        Refresh-Status
    }
})

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
    $gf.Text = 'Skeleton Key - gestures'
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
