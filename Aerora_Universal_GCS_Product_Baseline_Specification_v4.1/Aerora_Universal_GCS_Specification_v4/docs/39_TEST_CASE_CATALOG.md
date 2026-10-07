# 39 — Test Case Catalog

## Core

- TC-CON-001 connect/disconnect
- TC-CON-002 reconnect
- TC-CON-003 stale telemetry
- TC-CON-004 malformed message
- TC-CMD-001 command acknowledgement
- TC-CMD-002 command timeout
- TC-MIS-001 mission validation
- TC-MIS-002 mission upload failure
- TC-MIS-003 mission pause/resume
- TC-PAY-001 payload discovery
- TC-PAY-002 payload fault
- TC-AI-001 AI unavailable
- TC-AI-002 stale AI result
- TC-ROS-001 ROS2 unavailable
- TC-SEC-001 unauthorized access
- TC-UPD-001 failed update rollback
- TC-LOG-001 diagnostic export
- TC-SIM-001 simulation/live separation

Each test must later have prerequisites, steps, expected result, actual result, evidence and verdict.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.
