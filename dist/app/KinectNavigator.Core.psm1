# KinectNavigator.Core.psm1 - shared engine for the GUI and Console front-ends.
#
# Everything here is UI-agnostic: i18n, portable config.txt, kinectnav.ini
# read/write, game-folder state detection, and the actual install/uninstall
# file operations. Both front-ends call the SAME Install-KinectNavigator /
# Uninstall-KinectNavigator here -- previously the CLI installer re-implemented
# this in batch, independently of the GUI's inline logic, and that duplication
# is exactly what let a batch-only bug (an unescaped ')' in an echo line) ship
# for two releases before anyone ran the CLI path end-to-end.

$ErrorActionPreference = 'Stop'

# ===========================================================================
# paths (resolved once, from the front-end's $PSScriptRoot)
# ===========================================================================
function Initialize-KinectNavigatorCore {
    param([Parameter(Mandatory = $true)][string]$ScriptDir)
    [PSCustomObject]@{
        ScriptDir   = $ScriptDir
        ShimSrc     = Join-Path $ScriptDir 'Kinect10.dll'
        ExampleIni  = Join-Path $ScriptDir 'kinectnav.example.ini'
        LangDir     = Join-Path $ScriptDir 'lang'
        ConfigPath  = Join-Path $ScriptDir 'config.txt'
        GesturesImg = Join-Path $ScriptDir 'gestures.png'
    }
}

$script:MIN_GENUINE = 1000000            # the real runtime is ~15 MB
$RuntimeUrl = 'https://www.microsoft.com/download/details.aspx?id=40277'

# ===========================================================================
# i18n  (mirrors LegacyDownloader: lang\<code>.json flat key -> string maps,
# English is always the fallback layer, {tokens} filled from -Vars)
# ===========================================================================
$script:LangDir         = $null
$script:Strings         = @{}
$script:StringsFallback = @{}
$script:CurLang         = 'en'

function Import-LangFile([string]$Code) {
    $h = @{}
    if ([string]::IsNullOrWhiteSpace($script:LangDir)) { return $h }
    $path = Join-Path $script:LangDir ($Code + '.json')
    if (-not (Test-Path -LiteralPath $path)) { return $h }
    try {
        $raw  = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
        $json = $raw | ConvertFrom-Json
        foreach ($p in $json.PSObject.Properties) { $h[$p.Name] = [string]$p.Value }
    } catch { return @{} }
    return $h
}

function Get-AvailableLanguages {
    $out = @()
    if ([string]::IsNullOrWhiteSpace($script:LangDir) -or -not (Test-Path -LiteralPath $script:LangDir)) { return $out }
    $files = Get-ChildItem -LiteralPath $script:LangDir -Filter '*.json' -File -ErrorAction SilentlyContinue | Sort-Object Name
    foreach ($f in $files) {
        try {
            $json = ([System.IO.File]::ReadAllText($f.FullName, [System.Text.Encoding]::UTF8)) | ConvertFrom-Json
            $code   = if ($json.'_meta.code')       { [string]$json.'_meta.code' }       else { $f.BaseName }
            $native = if ($json.'_meta.nativeName') { [string]$json.'_meta.nativeName' } else { $code }
            $eng    = if ($json.'_meta.name')       { [string]$json.'_meta.name' }       else { $code }
            $out += [PSCustomObject]@{ Code = $code; NativeName = $native; Name = $eng }
        } catch { }
    }
    return $out
}

function Resolve-DefaultLanguage {
    $avail = @(Get-AvailableLanguages | ForEach-Object { $_.Code })
    if ($avail.Count -eq 0) { return 'en' }
    try { $c = [System.Globalization.CultureInfo]::CurrentUICulture } catch { return 'en' }
    $name = $c.Name
    $two  = $c.TwoLetterISOLanguageName
    if ($two -eq 'zh') {
        if (($name -match 'Hant|TW|HK|MO') -and ($avail -contains 'zh-Hant')) { return 'zh-Hant' }
        if ($avail -contains 'zh-Hans') { return 'zh-Hans' }
        if ($avail -contains 'zh-Hant') { return 'zh-Hant' }
    }
    if ($avail -contains $name) { return $name }
    if ($avail -contains $two)  { return $two }
    return 'en'
}

function Initialize-Language {
    param([string]$LangDir, [string]$Code)
    if ($LangDir) { $script:LangDir = $LangDir }
    $script:StringsFallback = Import-LangFile 'en'
    if ([string]::IsNullOrWhiteSpace($Code)) { $Code = 'en' }
    if ($Code -eq 'en') {
        $script:Strings = $script:StringsFallback
        $script:CurLang = 'en'
        return 'en'
    }
    $loaded = Import-LangFile $Code
    if ($loaded.Count -eq 0) {
        $script:Strings = $script:StringsFallback
        $script:CurLang = 'en'
    } else {
        $script:Strings = $loaded
        $script:CurLang = $Code
    }
    return $script:CurLang
}

function Get-CurrentLanguage { return $script:CurLang }

function T {
    param(
        [Parameter(Mandatory = $true, Position = 0)][string]$Key,
        [Parameter(Position = 1)][hashtable]$Vars
    )
    $s = $null
    if ($script:Strings -and $script:Strings.ContainsKey($Key))                     { $s = $script:Strings[$Key] }
    elseif ($script:StringsFallback -and $script:StringsFallback.ContainsKey($Key)) { $s = $script:StringsFallback[$Key] }
    if ($null -eq $s) { return $Key }
    if ($null -ne $Vars -and $Vars.get_Count() -gt 0) {
        $s = [regex]::Replace($s, '\{([A-Za-z0-9_]+)\}', {
            param($m)
            $n = $m.Groups[1].Value
            if ($Vars.ContainsKey($n)) { return [string]$Vars[$n] }
            return $m.Value
        })
    }
    return $s
}

# ===========================================================================
# portable config.txt (next to the front-end script)
# ===========================================================================
function Load-Config([string]$ConfigPath) {
    $gp = ''; $lang = ''
    try {
        if (Test-Path -LiteralPath $ConfigPath) {
            foreach ($line in Get-Content -LiteralPath $ConfigPath) {
                $t = ([string]$line).Trim()
                if ($t -eq '' -or $t.StartsWith('#')) { continue }
                $p = $t.Split('=', 2)
                if ($p.Count -lt 2) { continue }
                switch ($p[0].Trim().ToUpper()) {
                    'GAMEPATH' { $gp   = $p[1].Trim() }
                    'LANG'     { $lang = $p[1].Trim() }
                }
            }
        }
    } catch { }
    return [PSCustomObject]@{ GamePath = $gp; Lang = $lang }
}
function Save-Config([string]$ConfigPath, [string]$GamePath, [string]$Lang) {
    if ([string]::IsNullOrWhiteSpace($Lang)) { $Lang = $script:CurLang }
    try {
        @(
            '# KinectNavigator Setup - remembered settings. Safe to delete.'
            "GAMEPATH=$GamePath"
            "LANG=$Lang"
        ) | Set-Content -LiteralPath $ConfigPath -Encoding UTF8
    } catch { }   # read-only location -> just don't remember
}

# ===========================================================================
# preset tables: id -> fixed set of kinectnav.ini values. Display text is
# T "preset.<group>.<id>"; the id is what's stored / compared.
# ===========================================================================
$REACH_IDS  = @('sensitive', 'normal', 'big')
$SCROLL_IDS = @('slow', 'normal', 'fast')
$CMD_IDS    = @('easy', 'normal', 'strict')
$HOLD_IDS   = @('short', 'normal', 'long')

$PRESET_REACH = @{
    sensitive = [ordered]@{ dpad_park_radius = '0.26' }
    normal    = [ordered]@{ dpad_park_radius = '0.32' }
    big       = [ordered]@{ dpad_park_radius = '0.40' }
}
$PRESET_SCROLL = @{
    slow   = [ordered]@{ dpad_repeat_dwell_ms = '750'; dpad_repeat_first_ms = '550'; dpad_repeat_min_ms = '320'; dpad_repeat_accel_ms = '16' }
    normal = [ordered]@{ dpad_repeat_dwell_ms = '600'; dpad_repeat_first_ms = '430'; dpad_repeat_min_ms = '200'; dpad_repeat_accel_ms = '22' }
    fast   = [ordered]@{ dpad_repeat_dwell_ms = '450'; dpad_repeat_first_ms = '320'; dpad_repeat_min_ms = '130'; dpad_repeat_accel_ms = '30' }
}
$PRESET_CMD = @{
    easy   = [ordered]@{ dpad_cmd_gate_radius = '0.50' }
    normal = [ordered]@{ dpad_cmd_gate_radius = '0.42' }
    strict = [ordered]@{ dpad_cmd_gate_radius = '0.32' }
}
$PRESET_HOLD = @{
    short  = [ordered]@{ dpad_cmd_dwell_ms = '400'; dpad_back_dwell_ms = '1000' }
    normal = [ordered]@{ dpad_cmd_dwell_ms = '600'; dpad_back_dwell_ms = '1500' }
    long   = [ordered]@{ dpad_cmd_dwell_ms = '900'; dpad_back_dwell_ms = '2500' }
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
$CFG_DEFAULTS = @{
    dpad_park_radius = '0.32'
    dpad_repeat_dwell_ms = '600'; dpad_repeat_first_ms = '430'; dpad_repeat_min_ms = '200'; dpad_repeat_accel_ms = '22'
    dpad_cmd_gate_radius = '0.42'
    dpad_cmd_dwell_ms = '600'; dpad_back_dwell_ms = '1500'
}

# ===========================================================================
# version helpers
# ===========================================================================
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

# ===========================================================================
# best-effort checks
# ===========================================================================
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

# ---------------------------------------------------------------------------
# game-folder inspection  (returns a state id + a T key + format vars, so the
# text can be localised at display time)
# ---------------------------------------------------------------------------
function Get-State([string]$gf) {
    $r = [ordered]@{ folder = $gf; state = 'none'; detailKey = 'state.none'; detailVars = $null; fs = $null; instVer = $null }
    if ([string]::IsNullOrWhiteSpace($gf)) { return $r }
    if (-not (Test-Path -LiteralPath $gf)) { $r.detailKey = 'state.notexist'; return $r }
    if (-not (Test-Path -LiteralPath (Join-Path $gf 'legacy.exe'))) {
        $r.state = 'nogame'; $r.detailKey = 'state.nogame'; return $r
    }
    $dll     = Join-Path $gf 'Kinect10.dll'
    $backend = Join-Path $gf 'Kinect10_backend.dll'
    if (Test-Path -LiteralPath $backend) {
        $r.state = 'installed'; $r.detailKey = 'state.installed'
        $r.instVer = Get-DllVersion $dll
    }
    elseif (-not (Test-Path -LiteralPath $dll)) {
        $r.state = 'nodll'; $r.detailKey = 'state.nodll'
    }
    else {
        $sz = (Get-Item -LiteralPath $dll).Length
        if ($sz -ge $script:MIN_GENUINE) {
            $r.state = 'genuine'; $r.detailKey = 'state.genuine'
            $r.detailVars = @{ mb = ('{0:N1}' -f ($sz / 1MB)) }
        } else {
            $r.state = 'foreign'; $r.detailKey = 'state.foreign'
            $r.detailVars = @{ kb = ('{0:N0}' -f ($sz / 1KB)) }
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
# kinectnav.ini read / write  (not user-facing text)
# ---------------------------------------------------------------------------
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
function Set-IniKey([string]$iniPath, [string]$key, [string]$value) {
    if ([string]::IsNullOrWhiteSpace($iniPath)) { return }   # no game folder selected -- nothing to write
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
    if ([string]::IsNullOrWhiteSpace($iniPath) -or -not (Test-Path -LiteralPath $iniPath)) { return }
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
function Test-PresetMatch($ini, $ids, $table, $defs) {
    foreach ($id in $ids) {
        $ok = $true
        foreach ($kv in $table[$id].GetEnumerator()) {
            $cur = Get-IniVal $ini $kv.Key $defs[$kv.Key]
            if ((NormNum $cur) -ne (NormNum $kv.Value)) { $ok = $false; break }
        }
        if ($ok) { return $id }
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
    try {
        Add-Type -AssemblyName System.Windows.Forms -ErrorAction SilentlyContinue
        $n = [Enum]::GetName([System.Windows.Forms.Keys], $vk)
    } catch { }
    if ($n) { return ('{0}   (0x{1:X2})' -f $n, $vk) }
    return ('0x{0:X2}' -f $vk)
}
function Set-ConfigWindowed([string]$gf) {
    if ([string]::IsNullOrWhiteSpace($gf)) { return $false }
    $cfg = Join-Path $gf 'config.xml'
    if (-not (Test-Path -LiteralPath $cfg)) { return $false }
    try {
        $t = [System.IO.File]::ReadAllText($cfg)
        $n = [regex]::Replace($t, 'FullScreen\s*=\s*"1"', 'FullScreen="0"')
        if ($n -ne $t) { [System.IO.File]::WriteAllText($cfg, $n); return $true }
    } catch { }
    return $false
}

# ===========================================================================
# install / uninstall  -- the ONE implementation both front-ends call.
# Each returns @{ Ok = $bool; Key = <T key for the result message>; Vars = <hashtable, or $null> }
# and never throws for an expected failure -- only for something the caller
# should treat as unexpected (e.g. a permissions error mid-copy).
# ===========================================================================
function Install-KinectNavigator {
    param(
        [Parameter(Mandatory = $true)][string]$GameFolder,
        [Parameter(Mandatory = $true)][string]$ShimSrc,
        [Parameter(Mandatory = $true)][bool]$IsUpdate
    )
    $dll = Join-Path $GameFolder 'Kinect10.dll'
    if (-not $IsUpdate) {
        Copy-Item -LiteralPath $dll -Destination (Join-Path $GameFolder 'Kinect10.dll.orig-backup') -Force
        Rename-Item -LiteralPath $dll -NewName 'Kinect10_backend.dll'
    }
    Copy-Item -LiteralPath $ShimSrc -Destination $dll -Force

    $srcLen = (Get-Item -LiteralPath $ShimSrc).Length
    $dstLen = (Get-Item -LiteralPath $dll).Length
    if ($dstLen -ne $srcLen) {
        return @{ Ok = $false; Key = 'log.av_altered'; Vars = @{ got = $dstLen; want = $srcLen } }
    }
    return @{ Ok = $true; Key = $(if ($IsUpdate) { 'log.installed_upd' } else { 'log.installed_new' }); Vars = $null; WasUpdate = $IsUpdate; RenamedBackend = (-not $IsUpdate) }
}

function Uninstall-KinectNavigator {
    param([Parameter(Mandatory = $true)][string]$GameFolder)
    $dll     = Join-Path $GameFolder 'Kinect10.dll'
    $backend = Join-Path $GameFolder 'Kinect10_backend.dll'
    if (Test-Path -LiteralPath $dll) { Remove-Item -LiteralPath $dll -Force }
    Rename-Item -LiteralPath $backend -NewName 'Kinect10.dll'
    if ((Get-Item -LiteralPath (Join-Path $GameFolder 'Kinect10.dll')).Length -lt $script:MIN_GENUINE) {
        return @{ Ok = $false; Key = 'log.dll_small'; Vars = $null }
    }
    $bak = Join-Path $GameFolder 'Kinect10.dll.orig-backup'
    if (Test-Path -LiteralPath $bak) { Remove-Item -LiteralPath $bak -Force }
    return @{ Ok = $true; Key = 'log.uninstalled'; Vars = $null }
}

Export-ModuleMember -Function * -Variable REACH_IDS, SCROLL_IDS, CMD_IDS, HOLD_IDS, `
    PRESET_REACH, PRESET_SCROLL, PRESET_CMD, PRESET_HOLD, KEY_ORDER, KEY_DEFS, CFG_DEFAULTS, RuntimeUrl
