<#
    Aerora GCS build preflight gate.

    Phase 4 non-product tooling. Read-only by default: it measures the host against the
    build-readiness criteria in .kiro/specs/03-implementation-specification/24-build-readiness.md
    and prints a go / no-go verdict. It installs nothing unless -InstallUserTools is given, and
    even then only the three admin-free tools.

    Usage
        powershell -NoProfile -ExecutionPolicy Bypass -File .kiro\tools\aerora-preflight.ps1
        powershell -NoProfile -ExecutionPolicy Bypass -File .kiro\tools\aerora-preflight.ps1 -InstallUserTools

    Exit codes
        0  GO        every blocking criterion met
        1  NO-GO     at least one blocking criterion failed
        2  usage or environment error

    Deliberately NOT done by this script:
        - installing MSVC Build Tools or GStreamer  (both require elevation)
        - creating the Aerora overlay               (gated on a proven baseline build)
        - modifying any QGroundControl source file
#>

[CmdletBinding()]
param(
    [switch]$InstallUserTools,
    [int]$RequiredFreeGB = 40
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:Rows = @()
$script:Blocking = 0

function Add-Check {
    param(
        [string]$Name,
        [string]$State,      # PASS | FAIL | WARN | SKIP
        [string]$Detail,
        [bool]$IsBlocking = $true
    )
    $script:Rows += [PSCustomObject]@{
        Check    = $Name
        State    = $State
        Detail   = $Detail
        Blocking = $IsBlocking
    }
    if ($State -eq 'FAIL' -and $IsBlocking) { $script:Blocking++ }
}

function Get-ToolVersion {
    param([string]$Exe, [string[]]$VersionArgs)
    $cmd = Get-Command $Exe -ErrorAction SilentlyContinue
    if (-not $cmd) { return $null }
    try {
        $raw = & $Exe @VersionArgs 2>&1 | Select-Object -First 1
        return [string]$raw
    } catch {
        return $cmd.Source
    }
}

# ---------------------------------------------------------------- host

$os = Get-CimInstance Win32_OperatingSystem
$cs = Get-CimInstance Win32_ComputerSystem
$sysDrive = ($env:SystemDrive)
$disk = Get-CimInstance Win32_LogicalDisk -Filter ("DeviceID='" + $sysDrive + "'")
$freeGB = [math]::Round($disk.FreeSpace / 1GB, 2)
$cores = $cs.NumberOfLogicalProcessors
$ramGB = [math]::Round($cs.TotalPhysicalMemory / 1GB, 1)

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

Write-Host ''
Write-Host 'Aerora GCS build preflight' -ForegroundColor Cyan
Write-Host ('  host      : ' + $os.Caption + ' ' + $os.Version)
Write-Host ('  cores/ram : ' + $cores + ' logical, ' + $ramGB + ' GB')
Write-Host ('  free disk : ' + $freeGB + ' GB on ' + $sysDrive)
Write-Host ''

# ------------------------------------------------- blocking environment

if ($freeGB -ge $RequiredFreeGB) {
    Add-Check 'Free disk space' 'PASS' ($freeGB.ToString() + ' GB free, ' + $RequiredFreeGB + ' GB required')
} else {
    Add-Check 'Free disk space' 'FAIL' ($freeGB.ToString() + ' GB free, ' + $RequiredFreeGB + ' GB required')
}

if ($isAdmin) {
    Add-Check 'Elevated session' 'PASS' 'running as Administrator'
} else {
    Add-Check 'Elevated session' 'FAIL' 'MSVC Build Tools and GStreamer both require elevation'
}

if ($cores -ge 4) {
    Add-Check 'CPU cores' 'PASS' ($cores.ToString() + ' logical') $false
} else {
    Add-Check 'CPU cores' 'WARN' ($cores.ToString() + ' logical; builds will be slow') $false
}

if ($ramGB -ge 16) {
    Add-Check 'Memory' 'PASS' ($ramGB.ToString() + ' GB') $false
} else {
    Add-Check 'Memory' 'WARN' ($ramGB.ToString() + ' GB; cap build parallelism, see R4-004') $false
}

# ----------------------------------------------------------- toolchain

$cmakeVer = Get-ToolVersion 'cmake' @('--version')
if ($cmakeVer) {
    $m = [regex]::Match($cmakeVer, '(\d+)\.(\d+)\.(\d+)')
    if ($m.Success -and ([int]$m.Groups[1].Value -gt 3 -or
        ([int]$m.Groups[1].Value -eq 3 -and [int]$m.Groups[2].Value -ge 25))) {
        Add-Check 'CMake >= 3.25' 'PASS' $cmakeVer
    } else {
        Add-Check 'CMake >= 3.25' 'FAIL' ($cmakeVer + ' (3.25 required)')
    }
} else {
    Add-Check 'CMake >= 3.25' 'FAIL' 'absent'
}

$ninjaVer = Get-ToolVersion 'ninja' @('--version')
if ($ninjaVer) { Add-Check 'Ninja' 'PASS' $ninjaVer } else { Add-Check 'Ninja' 'FAIL' 'absent' }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $vcPath = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                         -property installationPath 2>$null | Select-Object -First 1
    if ($vcPath) {
        Add-Check 'MSVC C++ toolset' 'PASS' $vcPath
    } else {
        Add-Check 'MSVC C++ toolset' 'FAIL' 'vswhere present but VC.Tools.x86.x64 not installed'
    }
} else {
    Add-Check 'MSVC C++ toolset' 'FAIL' 'vswhere.exe absent; no Visual Studio or Build Tools'
}

$qtVer = Get-ToolVersion 'qmake' @('-query', 'QT_VERSION')
if (-not $qtVer) { $qtVer = Get-ToolVersion 'qmake6' @('-query', 'QT_VERSION') }
if ($qtVer) {
    if ($qtVer.Trim() -eq '6.11.1') {
        Add-Check 'Qt 6.11.1' 'PASS' $qtVer.Trim()
    } else {
        Add-Check 'Qt 6.11.1' 'FAIL' ($qtVer.Trim() + ' found; supported range is 6.11.0 to 6.11.1')
    }
} else {
    Add-Check 'Qt 6.11.1' 'FAIL' 'qmake not on PATH'
}

$gst = Get-ToolVersion 'gst-launch-1.0' @('--version')
if ($gst) {
    Add-Check 'GStreamer 1.28.4' 'PASS' $gst
} else {
    Add-Check 'GStreamer 1.28.4' 'FAIL' 'absent (required for the video pipeline)'
}

foreach ($t in @('python', 'git', 'node')) {
    $v = Get-ToolVersion $t @('--version')
    if ($v) { Add-Check ($t + ' present') 'PASS' $v $false }
    else    { Add-Check ($t + ' present') 'FAIL' 'absent' }
}

# ---------------------------------------------------------- repository

$pinned = '9459a4b57986f40df8bdc193d5453c0596fbb416'
try {
    $head = (& git rev-parse HEAD 2>$null).Trim()
    if ($head -eq $pinned) {
        Add-Check 'HEAD at pinned commit' 'PASS' $head $false
    } else {
        Add-Check 'HEAD at pinned commit' 'WARN' ('HEAD is ' + $head + '; pinned is ' + $pinned) $false
    }

    $dirty = @(& git diff --name-only HEAD 2>$null)
    if ($dirty.Count -gt 0) {
        Add-Check 'QGC source unmodified' 'FAIL' ($dirty.Count.ToString() + ' tracked file(s) modified')
    } else {
        Add-Check 'QGC source unmodified' 'PASS' 'git diff against HEAD is empty'
    }

    $pushUrl = (& git remote get-url --push origin 2>$null).Trim()
    if ($pushUrl -match 'mavlink/qgroundcontrol') {
        Add-Check 'origin push target' 'FAIL' ('origin pushes to UPSTREAM: ' + $pushUrl + '  (R-008)')
    } elseif ($pushUrl) {
        Add-Check 'origin push target' 'PASS' $pushUrl
    } else {
        Add-Check 'origin push target' 'WARN' 'no origin remote' $false
    }
} catch {
    Add-Check 'Repository state' 'FAIL' 'git unavailable or not a repository'
}

# --------------------------------------------------- optional installs

if ($InstallUserTools) {
    Write-Host 'InstallUserTools requested.' -ForegroundColor Yellow
    if ($freeGB -lt 10) {
        Write-Host ('  REFUSED: only ' + $freeGB + ' GB free. Qt alone needs 4 to 6 GB.') -ForegroundColor Red
        Add-Check 'User-tool install' 'SKIP' 'refused, insufficient disk' $false
    } else {
        Write-Host '  Installing Qt via the repository installer (admin-free, aqtinstall)...'
        $modules = 'qtgraphs qtlocation qtpositioning qtspeech qtmultimedia qtserialport ' +
                   'qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors qtscxml ' +
                   'qtwebsockets qthttpserver'
        & python 'tools/setup/install_qt.py' --version '6.11.1' --host windows `
                 --arch win64_msvc2022_64 --modules $modules
        if ($LASTEXITCODE -eq 0) {
            Add-Check 'User-tool install' 'PASS' 'Qt install reported success' $false
        } else {
            Add-Check 'User-tool install' 'FAIL' ('installer exit code ' + $LASTEXITCODE) $false
        }
        Write-Host '  CMake and Ninja are not installed by this script; use winget or a user-local archive.'
    }
}

# -------------------------------------------------------------- report

Write-Host ''
$script:Rows | Format-Table -AutoSize @(
    @{ Label = 'Check';  Expression = { $_.Check } }
    @{ Label = 'State';  Expression = { $_.State } }
    @{ Label = 'Detail'; Expression = { $_.Detail } }
)

# @(...) forces an array so .Count is valid for 0 or 1 matches under StrictMode.
$pass = @($script:Rows | Where-Object { $_.State -eq 'PASS' }).Count
$fail = @($script:Rows | Where-Object { $_.State -eq 'FAIL' }).Count
$warn = @($script:Rows | Where-Object { $_.State -eq 'WARN' }).Count

Write-Host ('  pass ' + $pass + '   fail ' + $fail + '   warn ' + $warn)
Write-Host ''

if ($script:Blocking -eq 0) {
    Write-Host 'VERDICT: GO' -ForegroundColor Green
    Write-Host '  Next: just configure && just build && just test   (baseline, unmodified upstream)'
    Write-Host '  Do not add Aerora code until the baseline build and test run are recorded.'
    exit 0
} else {
    Write-Host ('VERDICT: NO-GO  (' + $script:Blocking + ' blocking criterion/criteria failed)') -ForegroundColor Red
    Write-Host '  See .kiro/specs/03-implementation-specification/24-build-readiness.md'
    exit 1
}
