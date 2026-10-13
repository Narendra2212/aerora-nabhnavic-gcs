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
# resolved path -> list of "<manifest>:<variable>" labels. Check 3 needs the origins, not just a
# count: $declared collapses duplicates on insert, so a count comparison can say that a duplicate
# exists but never which file or which two manifests disagree.
$declaredOrigins = @{}
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

    # Track the enclosing set(<LIST_VAR> ... ) block as well as the path. Checks 3 and 8 need to
    # know which list a file was declared into: the directory variable in the path tells us where
    # the file is, not which build role it was given.
    $listVar = ''
    foreach ($line in $text.Split([char]10)) {
        $clean = $line.Trim()
        if ($clean.StartsWith('#')) { continue }
        if ($clean -match '^set\(\s*([A-Z0-9_]+)\s*$') { $listVar = $Matches[1]; continue }
        if ($clean -eq ')') { $listVar = ''; continue }
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
        $origin = if ($listVar) { "{0}:{1}" -f $m.Name, $listVar } else { "{0}:<no list>" -f $m.Name }
        if (-not $declaredOrigins.ContainsKey($resolved)) { $declaredOrigins[$resolved] = @() }
        $declaredOrigins[$resolved] += $origin
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
    if (-not $declaredOrigins.ContainsKey($resolved)) { $declaredOrigins[$resolved] = @() }
    $declaredOrigins[$resolved] += 'CMakeLists.txt:<inline>'
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
#
# Previously this section printed $declared.Count and nothing else. That cannot fail: $declared is
# a HashSet, so a path declared twice is already collapsed by the time the count is taken. The
# comparison that carries information is raw entries against distinct paths, plus the origins of
# any path that was declared more than once.
#
# REMOVE_DUPLICATES in the aggregation point means a duplicate does not break the build, so this
# is a conflicting-declaration check rather than a build-failure one: two manifests claiming the
# same file means two modules each believe they own it, and whichever list is later dropped takes
# the file with it.
# ---------------------------------------------------------------------------
Write-Section '3. No source declared twice'
$duplicates = @($declaredOrigins.Keys | Where-Object { $declaredOrigins[$_].Count -gt 1 })
foreach ($path in ($duplicates | Sort-Object)) {
    Write-Output ("  DUPLICATE  {0}  declared in: {1}" -f
        $path.Replace("$root\", ''), ($declaredOrigins[$path] -join ', '))
    $problems++
}
Write-Output ("  raw entries: {0}   distinct declared paths: {1}   duplicated: {2}" -f
    $entryCount, $declared.Count, $duplicates.Count)

# ---------------------------------------------------------------------------
# 4. Every *_TEST_NAMES entry is registered with add_qgc_test
# ---------------------------------------------------------------------------
Write-Section '4. Declared test names are registered with CTest'
$registered = @()
foreach ($m in [regex]::Matches($aggText, 'add_qgc_test\(\s*([A-Za-z0-9_]+)')) {
    $registered += $m.Groups[1].Value
}

$declaredTests = @()
# Manifests that declare test sources, and those that also declare the matching test names.
# Check 7 reports the difference: a manifest contributing test translation units but no
# _TEST_NAMES list is the condition that makes check 4 vacuous for that module.
$manifestsWithTestSources = @()
$manifestsWithTestNames = @()
foreach ($m in $manifests) {
    $text = [System.IO.File]::ReadAllText($m.FullName)
    if ($text -match 'set\(\s*[A-Z_]*_TEST_SOURCES') { $manifestsWithTestSources += $m.Name }
    $sawNames = $false
    foreach ($block in [regex]::Matches($text, '(?s)set\([A-Z_]*_TEST_NAMES([^)]*)\)')) {
        $sawNames = $true
        foreach ($tok in ($block.Groups[1].Value -split '\s+')) {
            $name = $tok.Trim()
            if ($name -match '^Aerora[A-Za-z0-9_]+Test$') { $declaredTests += $name }
        }
    }
    if ($sawNames) { $manifestsWithTestNames += $m.Name }
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

# ---------------------------------------------------------------------------
# 7. Every CTest entry is declared in a manifest  (registered subset-of declared)
#
# The converse of check 4, and the direction that was missing. Checks 4, 5 and 6 cover
# declared -> registered, self-registering -> registered and registered -> self-registering, which
# leaves one edge of the triangle unwalked: an add_qgc_test entry that no manifest declares.
#
# That gap is not cosmetic. Check 4 can only report a test the manifest declares, so a module
# whose manifest omits _TEST_NAMES entirely is a module where check 4 is vacuously satisfied - it
# has nothing to iterate. The test then exists in exactly one place, the aggregation point, and
# the manifest that owns the sources makes no claim about it. Dropping the add_qgc_test line
# removes the test from ctest with no check objecting, which is the silent-registration failure
# this suite exists to prevent.
# ---------------------------------------------------------------------------
Write-Section '7. Every CTest entry is declared in a manifest'
foreach ($name in $registered) {
    if ($declaredTests -notcontains $name) {
        Write-Output ("  NOT DECLARED  {0}  (add_qgc_test entry in no manifest _TEST_NAMES; check 4 cannot see it)" -f $name)
        $problems++
    }
}
foreach ($name in ($manifestsWithTestSources | Sort-Object)) {
    if ($manifestsWithTestNames -notcontains $name) {
        Write-Output ("  NO _TEST_NAMES  {0}  (declares test sources but no test names, so check 4 is vacuous for it)" -f $name)
        $problems++
    }
}
Write-Output ("  manifests with test sources: {0}   of those declaring test names: {1}" -f
    $manifestsWithTestSources.Count, $manifestsWithTestNames.Count)

# ---------------------------------------------------------------------------
# 8. Declared files are in a list of the right kind
#
# A .qml entry only reaches the QML module through a *_QML_FILES list, which the aggregation point
# feeds to qt_add_qml_module. A .qml listed in a *_SOURCES list is still "declared", so checks 1
# and 2 both pass, but the file never becomes a QML module member: the type does not resolve and
# nothing says so until a QML load at runtime. The reverse - a .cc or .h in a QML_FILES list -
# fails louder but is checked here too since the rule is the same one.
# ---------------------------------------------------------------------------
Write-Section '8. Declared files are in a list of the right kind'
$misplaced = 0
foreach ($path in ($declaredOrigins.Keys | Sort-Object)) {
    $ext = [System.IO.Path]::GetExtension($path).TrimStart('.').ToLowerInvariant()
    foreach ($origin in $declaredOrigins[$path]) {
        $listVar = ($origin -split ':', 2)[1]
        if ($listVar -eq '<inline>' -or $listVar -eq '<no list>') { continue }
        $isQmlList = $listVar.EndsWith('_QML_FILES')
        if ($ext -eq 'qml' -and -not $isQmlList) {
            Write-Output ("  WRONG LIST  {0}  in {1}  (.qml outside a *_QML_FILES list is not a QML module member)" -f
                $path.Replace("$root\", ''), $origin)
            $misplaced++
            $problems++
        } elseif ($ext -ne 'qml' -and $isQmlList) {
            Write-Output ("  WRONG LIST  {0}  in {1}  (only .qml belongs in a *_QML_FILES list)" -f
                $path.Replace("$root\", ''), $origin)
            $misplaced++
            $problems++
        }
    }
}
Write-Output ("  misplaced declarations: {0}" -f $misplaced)

# ---------------------------------------------------------------------------
# 9. Each overlay QML root type resolves from an import in scope
#
# SCOPE - READ THIS BEFORE TRUSTING THE RESULT.
#
# This is not a QML validator. It checks exactly one failure, the one that is invisible to every
# other check in this file: a .qml file whose ROOT type is declared in some other QML module in
# this repository, where that module's URI is in neither the file's own import list nor the
# IMPORTS list of the qt_add_qml_module that owns the file. That combination compiles, ships the
# resource, and registers the page; it fails only when QML loads the file, with
# "<Type> is not a type".
#
# The import scope deliberately unions two sources, and missing either one produces a wrong
# answer. A scan of file-level `import` lines alone reports AeroraOperationsHub.qml as broken -
# it roots on AnalyzePage with no AnalyzeView import in the file - when the import may instead be
# supplied at module level by IMPORTS in aerora/CMakeLists.txt. That false positive has already
# been reported once against this overlay.
#
# The two routes are NOT equally certain, and this check no longer reports them as if they were.
#
#   * a file-level `import X` is unambiguous: X's types are in that file's scope. Reported OK.
#
#   * a module-level IMPORTS entry is NOT documented to reach the module's own files, so it is
#     reported ASSUMED, not OK. Qt documents the consumer direction only. qt_add_qml_module says
#     of IMPORTS: "If a QML file imports this module, it also imports all the modules listed
#     under IMPORTS", and the qmldir `import` command says the imported module's types "are made
#     available in the same type namespace as this module is imported into". Both describe what a
#     file importing Aerora.Controls receives - not what a file that *is a member of*
#     Aerora.Controls receives. Member files reach their own module through the implicit import
#     rather than a written one, and whether the implicit import also pulls in the module's
#     qmldir `import` lines is not stated in either reference.
#     (doc.qt.io/qt-6/qt-add-qml-module.html, doc.qt.io/qt-6/qtqml-modules-qmldir.html)
#
#     It may well hold - AeroraOperationsHub.qml is written as though it does - but an ASSUMED
#     row records an unproven premise instead of laundering it into a pass. Resolving it needs
#     qmllint against generated type info, or a load. Both are blocked by D-006.
#
# What it does NOT check, in order of how likely each is to matter:
#   * only the root type. Types used inside the file - every delegate, attached property,
#     singleton reference and nested element - are not resolved. AnalyzePage happened to be a
#     root type; the next missing import may not be.
#   * only modules declared by a qt_add_qml_module in this repository. QtQuick, QtQuick.Layouts
#     and QtQuick.Controls live in the Qt installation, so a root type from one of those is
#     reported UNATTRIBUTED and passes. With no Qt present there is nothing to resolve against.
#   * C++-registered types are attributed by QML_ELEMENT / QML_NAMED_ELEMENT name only. The URI
#     such a type lands in is decided by which CMake target compiles the file, which this script
#     does not model, so these are not resolved either.
#   * import VERSION numbers, `as` qualifiers and conditional imports are ignored.
#
# A passing result here means this one failure mode is absent from the root types. It is not
# evidence that the QML loads. Only qmllint plus an executed Qt Quick Test can establish that;
# both are blocked while no Qt toolchain exists (D-006).
# ---------------------------------------------------------------------------
Write-Section '9. Overlay QML root types resolve from an import in scope'

function Get-CMakeLiveText($path) {
    $raw = ([System.IO.File]::ReadAllText($path)) -replace "`r", ''
    # Drop whole-line comments. Ui.cmake carries a commented-out copy of the real
    # qt_add_qml_module block, and parsing that copy as live config is how a stale prescription
    # gets mistaken for the configuration in force.
    return ((($raw -split "`n") | Where-Object { -not $_.TrimStart().StartsWith('#') }) -join "`n")
}

# Build URI -> member type names over every QML module in the repository. Members are the
# basenames of the QML_FILES entries: qgc_set_qt_resource_alias() flattens each file to its bare
# name, so the type name is the basename regardless of source subdirectory.
$moduleMembers = @{}
$aeroraUri = ''
$aeroraImports = @()
$aeroraMembers = @()
$unresolvedUriVars = 0

$cmakeFiles = @(Get-ChildItem -Recurse -Path $root -Include 'CMakeLists.txt', '*.cmake' -File |
    Where-Object { $_.FullName -notlike '*\build*' -and $_.FullName -notlike '*\.git\*' })

foreach ($cf in $cmakeFiles) {
    $live = Get-CMakeLiveText $cf.FullName
    if ($live -notmatch 'qt_add_qml_module\(') { continue }
    $listVars = @{}
    foreach ($sm in [regex]::Matches($live, '(?s)set\(\s*([A-Z0-9_]+)\s+"?([^"\n)]*)"?\s*\)')) {
        $listVars[$sm.Groups[1].Value] = $sm.Groups[2].Value.Trim()
    }

    foreach ($mm in [regex]::Matches($live, '(?s)qt_add_qml_module\((.*?)\n\)')) {
        $body = $mm.Groups[1].Value
        if ($body -notmatch 'URI\s+(\S+)') { continue }
        $uri = $Matches[1]
        if ($uri -match '^\$\{([A-Z0-9_]+)\}$') {
            # One variable hop. Aerora's URI is set in Ui.cmake, not inline here, so a literal
            # scan would miss the overlay's own module entirely.
            $uriVar = $Matches[1]
            $resolvedUri = ''
            foreach ($cf2 in $cmakeFiles) {
                $live2 = Get-CMakeLiveText $cf2.FullName
                $vm = [regex]::Match($live2, ('set\(\s*' + [regex]::Escape($uriVar) + '\s+"?([^"\n)]+)"?\s*\)'))
                if ($vm.Success) { $resolvedUri = $vm.Groups[1].Value.Trim(); break }
            }
            if (-not $resolvedUri) {
                Write-Output ("  UNRESOLVED URI VAR  `${{{0}}}  in {1}" -f $uri, $cf.Name)
                $unresolvedUriVars++
                $problems++
                continue
            }
            $uri = $resolvedUri
        }

        $qmlBlock = [regex]::Match($body, '(?s)QML_FILES(.*?)(?:\n\s*[A-Z_]{3,}\s|$)')
        if (-not $qmlBlock.Success) { continue }
        $members = @()
        foreach ($tok in ($qmlBlock.Groups[1].Value -split '\s+')) {
            $t = $tok.Trim()
            if (-not $t) { continue }
            if ($t -match '^\$\{([A-Z0-9_]+)\}$') {
                foreach ($e in (($listVars[$Matches[1]]) -split '\s+')) {
                    if ($e -match '([^/\\]+)\.qml$') { $members += $Matches[1] }
                }
                continue
            }
            if ($t -match '([^/\\]+)\.qml$') { $members += $Matches[1] }
        }
        if (-not $moduleMembers.ContainsKey($uri)) { $moduleMembers[$uri] = @() }
        $moduleMembers[$uri] += $members

        if ($cf.FullName -eq (Join-Path $aerora 'CMakeLists.txt')) {
            $aeroraUri = $uri
            $aeroraMembers = $members
            $im = [regex]::Match($body, '(?s)\n\s*IMPORTS\s*\n(.*?)(?:\n\s*[A-Z_]{3,}|$)')
            if ($im.Success) {
                $aeroraImports = @(($im.Groups[1].Value -split '\s+') | Where-Object { $_.Trim() })
            }
        }
    }
}

$aeroraUriLabel = if ($aeroraUri) { $aeroraUri } else { '<none>' }
Write-Output ("  repo QML modules: {0}   overlay module: {1}   module-level IMPORTS: {2}" -f
    $moduleMembers.Count, $aeroraUriLabel, ($aeroraImports -join ' '))

if (-not $aeroraUri) {
    Write-Output '  NO OVERLAY QML MODULE  (no qt_add_qml_module found in aerora/CMakeLists.txt)'
    $problems++
} else {
    $scope = @($aeroraImports)
    $rootsChecked = 0
    $unattributed = 0
    $assumed = 0

    foreach ($qf in (Get-ChildItem -Recurse -Path $aerora -Filter '*.qml' -File | Sort-Object FullName)) {
        $qtext = ([System.IO.File]::ReadAllText($qf.FullName)) -replace "`r", ''
        $qtext = [regex]::Replace($qtext, '(?s)/\*.*?\*/', '')

        $fileImports = @()
        $rootType = ''
        foreach ($line in ($qtext -split "`n")) {
            $c = $line.Trim()
            if (-not $c -or $c.StartsWith('//')) { continue }
            if ($c -match '^import\s+([A-Za-z0-9_.]+)') { $fileImports += $Matches[1]; continue }
            if ($c -match '^([A-Z][A-Za-z0-9_.]*)\s*\{') { $rootType = $Matches[1]; break }
        }

        $rel = $qf.FullName.Replace("$root\", '')
        if (-not $rootType) {
            Write-Output ("  NO ROOT TYPE  {0}  (could not determine the root type; not checked)" -f $rel)
            $unattributed++
            continue
        }
        $rootsChecked++

        if ($aeroraMembers -contains $rootType) {
            Write-Output ("  OK            {0}  root {1} (same module, mutually visible)" -f $rel, $rootType)
            continue
        }

        $owners = @($moduleMembers.Keys | Where-Object { $moduleMembers[$_] -contains $rootType })
        if ($owners.Count -eq 0) {
            Write-Output ("  UNATTRIBUTED  {0}  root {1} (declared by no repo QML module; see SCOPE)" -f $rel, $rootType)
            $unattributed++
            continue
        }

        # A file-level import is decisive; a module-level IMPORTS entry is not documented to reach
        # the module's own members, so it is reported separately. See the SCOPE block.
        $byFile = @($owners | Where-Object { $fileImports -contains $_ })
        $byModule = @($owners | Where-Object { $scope -contains $_ })
        if ($byFile.Count -gt 0) {
            Write-Output ("  OK            {0}  root {1} <- {2} (file import)" -f $rel, $rootType, $byFile[0])
        } elseif ($byModule.Count -gt 0) {
            Write-Output ("  ASSUMED       {0}  root {1} <- {2} (module IMPORTS only; not documented to reach module members)" -f
                $rel, $rootType, $byModule[0])
            $assumed++
        } else {
            Write-Output ("  NOT IMPORTED  {0}  root {1} is declared in {2}, which is in neither the file imports nor IMPORTS of {3}" -f
                $rel, $rootType, ($owners -join '/'), $aeroraUri)
            Write-Output ("                this loads only at runtime and fails with `"{0} is not a type`"" -f $rootType)
            $problems++
        }
    }
    Write-Output ("  root types checked: {0}   unattributed (not resolvable statically): {1}   assumed via module IMPORTS: {2}" -f
        $rootsChecked, $unattributed, $assumed)
}

Write-Output ''
Write-Output ("problems found: {0}" -f $problems)
Write-Output 'NOTE: file-existence and cross-reference only. This is not a CMake configure.'
Write-Output 'NOTE: check 9 covers root types against repo-declared QML modules only. It is not qmllint'
Write-Output '      and a pass is not evidence that any QML file loads. UNATTRIBUTED and ASSUMED rows'
Write-Output '      are unresolved, not passed. See the SCOPE block above.'
exit ([int]($problems -gt 0))
