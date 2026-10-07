# 22 — Requirements Traceability Matrix

## Required columns

| ID | Requirement | Source | Design | Implementation | Test | Evidence | Status |
|---|---|---|---|---|---|---|---|

## Rules

- IDs are stable.
- Do not reuse retired IDs.
- Changes require impact analysis.
- Critical requirements cannot be marked complete without verification evidence.

## Traceability levels

Product -> System -> Software -> Interface -> Test -> Release.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.
