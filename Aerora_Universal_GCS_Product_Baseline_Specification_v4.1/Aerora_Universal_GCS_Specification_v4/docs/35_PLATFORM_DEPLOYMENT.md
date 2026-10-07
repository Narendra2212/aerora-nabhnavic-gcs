# 35 — Platform and Deployment

## Initial target

Windows x64.

## Deployment package

Must define:
- installer/package format
- prerequisites
- configuration location
- logs location
- update path
- uninstall behavior
- permissions
- offline operation behavior

## Offline-first consideration

Core flight/mission operations should not depend unnecessarily on internet availability. Any cloud feature must have explicit degraded/offline behavior.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.
