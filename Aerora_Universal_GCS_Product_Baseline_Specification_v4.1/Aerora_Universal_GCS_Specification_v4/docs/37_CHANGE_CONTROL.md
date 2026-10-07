# 37 — Change Control

## Change classes

- documentation
- UI
- non-critical feature
- protocol
- flight-related
- safety-critical
- security-critical
- licensing/dependency
- architecture

Higher-risk changes require deeper review.

## Change record

- reason
- affected requirements
- affected interfaces
- risk
- implementation
- tests
- migration
- rollback
- release impact

## Upstream QGC updates

Treat each upstream update as a controlled change, not a simple Git pull.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.
