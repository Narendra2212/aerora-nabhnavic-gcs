#!/usr/bin/env pwsh
# Static consistency check over the Aerora overlay build manifests.
#
# CMake cannot be run (D-006), so this resolves what the manifests declare and compares it against
# what is on disk and against what aerora/CMakeLists.txt registers. It is a file-existence and
# cross-reference check. IT IS NOT A CONFIGURE and must not be read as one.
#
# Usage:  powershell -NoProfile -ExecutionPolicy Bypass -File .kiro/tools/aerora-manifest-check.ps1

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$aerora = Join-Path $root 'aerora'
$problems = 0

function Write-Section($title) {
    Write-Output ''
    Write-Output ("=== {0} ===" -f $title)
}

# ---------------------------------------------------------------------------
# Gather manifests and the aggregation point
# ---------------------------------------------------------------------------
$manifests = @(Get-ChildItem -Recurse -Path $aerora -Filter '*.cmake' |
    Where-Object { $_.FullName -notlike '*\cmake\*' })
$aggregation = Join-Path $aerora 'CMakeLists.txt'
$aggText = [System.IO.File]::ReadAllText($aggregation)

Write-Section 'Manifests found'
foreach ($m in $manifests) { Write-Output ("  {0}" -f $m.FullName.Replace("$root\", '')) }
Write-Output ("  (aggregation) {0}" -f $aggregation.Replace("$root\", ''))

# ---------------------------------------------------------------------------
# 1. Declared source entries resolve on disk
# ---------------------------------------------------------------------------
Write-Section '1. Declared files resolve on disk'
$declared = New-Object System.Collections.Generic.HashSet[string]
$entryCount = 0

foreach ($m in $manifests) {
    $dir = $m.DirectoryName
    $text = [System.IO.File]::ReadAllText($m.FullName)

    # Build the manifest's own directory-variable table. A manifest sets <PREFIX>_DIR from
    # CMAKE_CURRENT_LIST_DIR and then derives <PREFIX>_TEST_DIR from it, so resolving every path
    # against the manifest directory would be wrong for the test entries.
    $vars = @{}
    foreach ($line in $text.Split([char]10)) {
        $clean = $line.Trim()
        if ($clean.StartsWith('#')) { continue }
        if ($clean -match '^set\(\s*([A-Z0-9_]+)\s+\$\{CMAKE_CURRENT_LIST_DIR\}\s*\)\s*$') {
            $vars[$Matches[1]] = $dir
            continue
        }
        if ($clean -match '^set\(\s*([A-Z0-9_]+)\s+\$\{([A-Z0-9_]+)\}(/[^\s)]*)\s*\)\s*$') {
            $base = $vars[$Matches[2]]
            if ($base) {
                $vars[$Matches[1]] = [System.IO.Path]::GetFullPath((Join-Path $base $Matches[3].TrimStart('/')))
            }
            continue
        }
    }

    foreach ($line in $text.Split([char]10)) {
        $clean = $line.Trim()
        if ($clean.StartsWith('#')) { continue }
        if ($clean -notmatch '^\$\{([A-Z0-9_]+)\}/(.+\.(cc|h|qml))\s*$') { continue }
        $varName = $Matches[1]
        $relative = $Matches[2]
        $base = $vars[$varName]
        if (-not $base) {
            Write-Output ("  UNRESOLVED VAR  `${{{0}}}  in {1}" -f $varName, $m.Name)
            $problems++
            continue
        }
        $entryCount++
        $resolved = [System.IO.Path]::GetFullPath((Join-Path $base $relative))
        [void]$declared.Add($resolved)
        if (-not (Test-Path $resolved)) {
            Write-Output ("  MISSING  {0}  (declared in {1})" -f $relative, $m.Name)
            $problems++
        }
    }
}

# Inline entries in the aggregation point
foreach ($line in $aggText.Split([char]10)) {
    $clean = $line.Trim()
    if ($clean.StartsWith('#')) { continue }
    if ($clean -notmatch '\$\{CMAKE_CURRENT_SOURCE_DIR\}/(.+\.(cc|h|qml))\s*$') { continue }
    $relative = $Matches[1]
    $entryCount++
    $resolved = [System.IO.Path]::GetFullPath((Join-Path $aerora $relative))
    [void]$declared.Add($resolved)
    if (-not (Test-Path $resolved)) {
        Write-Output ("  MISSING  {0}  (declared inline in aerora/CMakeLists.txt)" -f $relative)
        $problems++
    }
}
Write-Output ("  manifest entries resolved: {0}" -f $entryCount)

# ---------------------------------------------------------------------------
# 2. Every source on disk is declared somewhere
# ---------------------------------------------------------------------------
Write-Section '2. Every source on disk is declared'
$onDisk = @(Get-ChildItem -Recurse -Path $aerora -Include '*.cc', '*.h', '*.qml' -File)
$undeclared = 0
foreach ($f in $onDisk) {
    if (-not $declared.Contains($f.FullName)) {
        Write-Output ("  UNDECLARED  {0}" -f $f.FullName.Replace("$root\", ''))
        $undeclared++
        $problems++
    }
}
Write-Output ("  sources on disk: {0}   undeclared: {1}" -f $onDisk.Count, $undeclared)

# ---------------------------------------------------------------------------
# 3. Duplicate declarations
# ---------------------------------------------------------------------------
Write-Section '3. No source declared twice'
Write-Output ("  distinct declared paths: {0}   (aggregation applies REMOVE_DUPLICATES regardless)" -f $declared.Count)

# ---------------------------------------------------------------------------
# 4. Every *_TEST_NAMES entry is registered with add_qgc_test
# ---------------------------------------------------------------------------
Write-Section '4. Declared test names are registered with CTest'
$registered = @()
foreach ($m in [regex]::Matches($aggText, 'add_qgc_test\(\s*([A-Za-z0-9_]+)')) {
    $registered += $m.Groups[1].Value
}

$declaredTests = @()
foreach ($m in $manifests) {
    $text = [System.IO.File]::ReadAllText($m.FullName)
    foreach ($block in [regex]::Matches($text, '(?s)set\([A-Z_]*_TEST_NAMES([^)]*)\)')) {
        foreach ($tok in ($block.Groups[1].Value -split '\s+')) {
            $name = $tok.Trim()
            if ($name -match '^Aerora[A-Za-z0-9_]+Test$') { $declaredTests += $name }
        }
    }
}
$declaredTests = @($declaredTests | Sort-Object -Unique)

foreach ($name in $declaredTests) {
    if ($registered -notcontains $name) {
        Write-Output ("  NOT REGISTERED  {0}  (declared in a manifest _TEST_NAMES, no add_qgc_test entry)" -f $name)
        $problems++
    }
}
Write-Output ("  manifest-declared test names: {0}   add_qgc_test entries: {1}" -f $declaredTests.Count, $registered.Count)

# ---------------------------------------------------------------------------
# 5. Every test source with UT_REGISTER_TEST has a CTest entry
# ---------------------------------------------------------------------------
Write-Section '5. Every self-registering test has a CTest entry'
$selfRegistered = @()
foreach ($f in (Get-ChildItem -Recurse -Path (Join-Path $aerora 'test') -Filter '*.cc' -File)) {
    $text = [System.IO.File]::ReadAllText($f.FullName)
    foreach ($m in [regex]::Matches($text, 'UT_REGISTER_TEST\w*\(\s*([A-Za-z0-9_]+)')) {
        $selfRegistered += $m.Groups[1].Value
    }
}
$selfRegistered = @($selfRegistered | Sort-Object -Unique)

foreach ($name in $selfRegistered) {
    if ($registered -notcontains $name) {
        Write-Output ("  NO CTEST ENTRY  {0}  (compiles and self-registers, but ctest -L Unit will not see it)" -f $name)
        $problems++
    }
}
Write-Output ("  self-registering tests: {0}" -f $selfRegistered.Count)

# ---------------------------------------------------------------------------
# 6. Every add_qgc_test entry has a self-registering test
# ---------------------------------------------------------------------------
Write-Section '6. Every CTest entry has a self-registering test'
foreach ($name in $registered) {
    if ($selfRegistered -notcontains $name) {
        Write-Output ("  NO TEST CLASS  {0}  (add_qgc_test entry with no UT_REGISTER_TEST)" -f $name)
        $problems++
    }
}

Write-Output ''
Write-Output ("problems found: {0}" -f $problems)
Write-Output 'NOTE: file-existence and cross-reference only. This is not a CMake configure.'
exit ([int]($problems -gt 0))
