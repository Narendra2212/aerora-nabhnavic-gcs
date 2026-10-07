# 05 — QGroundControl Baseline Analysis

## Status

Analysis-only specification. Kiro must complete this against the exact checked-out repository.

## Required evidence

Record:
- repository URL
- commit SHA
- branch/tag
- submodule SHAs
- Git working-tree status
- build tool versions
- Qt version
- CMake version
- compiler version
- supported host platforms
- build presets
- test commands

## Architecture analysis

Identify:
- application entry
- core services
- MAVLink transport
- vehicle lifecycle
- vehicle plugins
- mission framework
- parameter system
- map
- video
- logging
- settings
- QML modules
- custom build overlay
- custom plugins
- tests

## Current upstream facts to verify

The current QGC repository describes QGC as a cross-platform MAVLink GCS and contains `custom-example`, `CMakePresets.json`, `CMakeLists.txt`, `AGENTS.md`, `CLAUDE.md`, license files and other development infrastructure. QGC's current custom-build docs identify `QGCCorePlugin`, `FirmwarePlugin` and `AutoPilotPlugin` as primary customization mechanisms.

The exact checked-out repository is authoritative for implementation details.

## Deliverables

- `docs/QGC_BASELINE_REPORT.md`
- `docs/QGC_ARCHITECTURE_MAP.md`
- `docs/QGC_DEPENDENCY_INVENTORY.md`
- `docs/QGC_EXTENSION_POINT_MAP.md`
- `docs/QGC_BUILD_BASELINE.md`

## Constraint

No source modification during this analysis task.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.
