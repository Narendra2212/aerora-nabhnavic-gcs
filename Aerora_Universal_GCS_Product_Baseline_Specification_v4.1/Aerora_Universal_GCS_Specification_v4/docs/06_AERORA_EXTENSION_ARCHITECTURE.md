# 06 — Aerora Extension Architecture

## Proposed modules

```text
Aerora/
  Core/
  Vehicle/
  Firmware/
  MAVLink/
  Mission/
  Payload/
    Core/
    Painting/
    Firefighting/
    Agriculture/
  AI/
  ROS2/
  Safety/
  Diagnostics/
  UI/
```

## Preferred QGC integration

Use QGC custom-build overlays and documented plugin architectures first.

QGC currently documents:
- `QGCCorePlugin`
- `FirmwarePlugin`
- `AutoPilotPlugin`

as custom-build extension mechanisms. Its current CMake configuration also supports a custom source overlay.

## Upstream modification policy

Before modifying upstream code:
1. identify requirement
2. identify extension point
3. prove extension point is insufficient
4. document alternatives
5. estimate upstream divergence
6. approve change
7. add regression tests

## Upstream synchronization

Track:
- base commit
- Aerora patches
- upstream changes
- conflicts
- compatibility notes

Avoid a long-lived uncontrolled fork.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.
