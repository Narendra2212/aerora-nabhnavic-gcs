# QGC Baseline Analysis — Requirements

## Goal
Produce a factual architecture, build, test, dependency and extension-point baseline for the exact QGC repository checkout without modifying source.

### R1
Record repository commit, branch, submodules and working-tree state.

### R2
Map major C++ and QML modules.

### R3
Trace MAVLink transport -> vehicle model -> application state -> QML.

### R4
Analyze custom build overlays and plugin extension points.

### R5
Inventory dependencies and licensing.

### R6
Document build/test toolchain.

### R7
Identify Aerora extension boundaries and upstream-protected areas.

## Acceptance

- no source modifications
- exact commit recorded
- baseline report created
- dependency inventory created
- extension-point map created
- build/test procedure recorded
