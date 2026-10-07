#!/usr/bin/env pwsh
# Source-level hygiene checks for the Aerora UI module (M-Ui / Tab 7).
#
# Substitute for the real lint gate while no build environment exists (D-006): clang-format,
# clang-tidy, qmllint, qmlformat and pre-commit are all absent. This checks only what can be
# checked by reading bytes, and it is NOT a replacement for `just lint`.
#
# Usage:  pwsh -File .kiro/tools/aerora-ui-static-check.ps1

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$targets = @(
    'aerora/src/UI/AeroraUiLog.h'
    'aerora/src/UI/AeroraUiLog.cc'
    'aerora/src/UI/AeroraQmlOverrides.h'
    'aerora/src/UI/AeroraQmlOverrides.cc'
    'aerora/src/UI/AeroraVehicleUiContext.h'
    'aerora/src/UI/AeroraVehicleUiContext.cc'
    'aerora/src/UI/Ui.cmake'
    'aerora/src/UI/Controls/AeroraStateText.qml'
    'aerora/src/UI/Controls/AeroraAgeLabel.qml'
    'aerora/src/UI/Controls/AeroraTokenDisplay.qml'
    'aerora/src/UI/Controls/AeroraValueDisplay.qml'
    'aerora/test/UI/AeroraUiFoundationTest.h'
    'aerora/test/UI/AeroraUiFoundationTest.cc'
    'aerora/test/UI/AeroraUiSafetyTest.h'
    'aerora/test/UI/AeroraUiSafetyTest.cc'
)

$CR = [char]13
$LF = [char]10
$TAB = [char]9
$failures = 0

foreach ($rel in $targets) {
    $path = Join-Path $root $rel
    if (-not (Test-Path $path)) {
        Write-Output ("MISSING   {0}" -f $rel)
        $failures++
        continue
    }

    $text = [System.IO.File]::ReadAllText($path)
    $issues = New-Object System.Collections.ArrayList

    if ($text.Contains($CR)) { [void]$issues.Add('CRLF line endings') }
    if ($text.Contains($TAB)) { [void]$issues.Add('tab character') }
    if (-not $text.EndsWith($LF)) { [void]$issues.Add('no newline at EOF') }

    $lines = $text.Split($LF)
    $trailing = 0
    $long = 0
    foreach ($line in $lines) {
        $clean = $line.TrimEnd($CR)
        if ($clean -ne $clean.TrimEnd()) { $trailing++ }
        if ($clean.Length -gt 120) { $long++ }
    }
    if ($trailing -gt 0) { [void]$issues.Add("trailing whitespace x$trailing") }
    if ($long -gt 0) { [void]$issues.Add("over 120 columns x$long") }

    foreach ($banned in @('Q_ASSERT', 'QTest::ignoreMessage')) {
        if ($text.Contains($banned)) { [void]$issues.Add("banned construct: $banned") }
    }
    if ($text -match '(?<!qC)(?<!\w)qDebug\s*\(') { [void]$issues.Add('uncategorized qDebug()') }
    if ($text -match 'qWait\(\s*[1-9]') { [void]$issues.Add('fixed-delay qWait') }

    $braceOpen = ([regex]::Matches($text, '\{')).Count
    $braceClose = ([regex]::Matches($text, '\}')).Count
    if ($braceOpen -ne $braceClose) { [void]$issues.Add("unbalanced braces $braceOpen/$braceClose") }
    $parenOpen = ([regex]::Matches($text, '\(')).Count
    $parenClose = ([regex]::Matches($text, '\)')).Count
    if ($parenOpen -ne $parenClose) { [void]$issues.Add("unbalanced parens $parenOpen/$parenClose") }

    if ($rel.EndsWith('.h') -and -not $text.Contains('#pragma once')) {
        [void]$issues.Add('missing #pragma once')
    }

    # No MAVLink identifier may be hard-coded anywhere in the UI module.
    if ($text -match 'MAV_CMD_|MAVLINK_MSG_ID_|MAV_COMP_ID_') {
        [void]$issues.Add('hard-coded MAVLink identifier')
    }

    if ($issues.Count -eq 0) {
        Write-Output ("OK        {0}" -f $rel)
    } else {
        Write-Output ("ISSUE     {0}  ->  {1}" -f $rel, ($issues -join '; '))
        $failures++
    }
}

Write-Output ''
Write-Output ("files checked: {0}   files with issues: {1}" -f $targets.Count, $failures)
exit ([int]($failures -gt 0))
