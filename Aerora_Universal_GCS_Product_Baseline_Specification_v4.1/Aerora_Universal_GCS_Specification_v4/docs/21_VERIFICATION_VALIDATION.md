# 21 — Verification and Validation

## Verification methods

- inspection
- analysis
- unit test
- component test
- integration test
- simulation
- HIL
- bench test
- controlled flight

## Traceability

Every critical requirement must link to:
requirement -> design -> implementation -> test -> evidence -> release.

## Independence

For high-consequence functions, verification should be reviewed independently of the person/agent that implemented the change where practical.

## Acceptance

No production claim without evidence.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.
